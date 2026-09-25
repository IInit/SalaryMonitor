// Earnings.cpp : 计薪引擎实现
#include "Earnings.h"
#include <algorithm>

namespace earnings
{

const wchar_t* PhaseName(WorkPhase p)
{
    switch (p)
    {
    case PHASE_REST:    return L"休息日";
    case PHASE_BEFORE:  return L"未上班";
    case PHASE_WORKING: return L"工作中";
    case PHASE_BREAK:   return L"休息中";
    case PHASE_AFTER:   return L"已下班";
    default:            return L"";
    }
}

static int ClampPermille(double v)
{
    if (v <= 0.0)
        return 0;
    if (v >= 1.0)
        return 1000;
    return (int)(v * 1000.0 + 0.5);
}

PayContext MakeContext(const SalaryConfig& cfg, int year, int month)
{
    PayContext ctx;
    ctx.year = year;
    ctx.month = month;
    ctx.month_shift_sec = sched::MonthShiftSeconds(cfg, year, month);
    ctx.month_standard_sec = sched::MonthStandardSeconds(cfg, year, month);
    ctx.work_days = sched::WorkDaysInMonth(cfg, year, month);
    return ctx;
}

// ---------------------------------------------------------------- 每日费率曲线
// 返回当天应得总额；同时把费率段写入 segs。
//
//   段 = { 起止秒 , 元/秒 , 是否加班 }
//
// 各计薪方式的差别，最终都落在"基准秒薪"与"哪些段算加班"上：
//   月薪固定   base = 月薪 / 当月排班秒数     全段标准
//   月薪+加班  base = 月薪 / 当月标准秒数     超标准工时部分 × 倍率（周末整班 × 周末倍率）
//   时薪       base = 时薪 / 3600             同"月薪+加班"的加班判定
//   日薪       base = 日薪 / 当日排班秒数     同"月薪+加班"的加班判定（加班后当天超过日薪）
//   计件       base = 件单价×件数 / 当日排班秒数   无加班概念（与工时无关，按天摊平）
//   底薪+提成  base = (底薪/当月出勤天数 + 业绩×提成) / 当日排班秒数  无加班概念
double DayTotal(const SalaryConfig& cfg, const PayContext& ctx, int wday,
                std::vector<Segment>* segs_out, double* ot_total_out, double* base_rate_out)
{
    if (segs_out != nullptr)
        segs_out->clear();
    if (ot_total_out != nullptr)
        *ot_total_out = 0.0;
    if (base_rate_out != nullptr)
        *base_rate_out = 0.0;

    std::vector<Segment> segs;
    std::vector<sched::Shift> shifts;
    sched::ShiftsOf(sched::RuleFor(cfg, wday), shifts);

    double shift_sec = 0.0;
    for (const sched::Shift& s : shifts)
        shift_sec += (s.end - s.begin);
    if (shift_sec <= 0.0)
        return 0.0;                       // 当天休息

    const bool weekend = sched::IsWeekend(wday);

    // 计件 / 提成与工时无关，只按天摊平，因此没有"加班加钱"的概念
    const bool time_based = !(cfg.mode == PAY_PIECE || cfg.mode == PAY_COMMISSION);

    // 月薪固定制没有加班概念；计件 / 提成也不按工时算，因此都不做加班加成。
    const bool ot_on = cfg.ot_enabled && time_based && cfg.mode != PAY_MONTHLY_FIXED;

    // ---- 基准秒薪 ----
    double base = 0.0;
    switch (cfg.mode)
    {
    case PAY_MONTHLY_FIXED:
    {
        double base_sec = ctx.month_shift_sec;
        base = base_sec > 0.0 ? cfg.monthly_salary / base_sec : 0.0;
        break;
    }
    case PAY_MONTHLY_OT:
    {
        // 开加班费：月薪是"标准工时"的报酬，基准只摊到标准工时，加班另算；
        // 关加班费：月薪整月摊平，保证"整月应得恰好等于月薪"。
        double base_sec = ot_on ? ctx.month_standard_sec : ctx.month_shift_sec;
        if (base_sec <= 0.0)
            base_sec = ctx.month_shift_sec;
        base = base_sec > 0.0 ? cfg.monthly_salary / base_sec : 0.0;
        break;
    }
    case PAY_HOURLY:
        base = cfg.hourly_wage / 3600.0;
        break;
    case PAY_DAILY:
        base = cfg.daily_wage / shift_sec;
        break;
    case PAY_PIECE:
        base = (cfg.unit_price * cfg.unit_count) / shift_sec;
        break;
    case PAY_COMMISSION:
    {
        double base_daily = 0.0;
        if (ctx.work_days > 0)
            base_daily += cfg.base_monthly / ctx.work_days;
        base_daily += cfg.business_amount * cfg.commission_rate;
        base = base_daily / shift_sec;
        break;
    }
    default:
        base = 0.0;
        break;
    }
    if (base_rate_out != nullptr)
        *base_rate_out = base;

    // ---- 加班判定 ----
    double mult = 1.0;
    if (ot_on)
    {
        mult = weekend ? cfg.ot_weekend : cfg.ot_weekday;
        if (mult < 1.0)
            mult = 1.0;
    }
    const double std_sec = ot_on ? sched::DayStandardSeconds(cfg, wday) : shift_sec;

    // ---- 切段 ----
    double day_total = 0.0;
    double ot_total = 0.0;
    double used_std = 0.0;

    for (const sched::Shift& s : shifts)
    {
        const double len = double(s.end - s.begin);
        double std_part = std_sec - used_std;
        if (std_part < 0.0)
            std_part = 0.0;
        if (std_part > len)
            std_part = len;

        if (std_part > 0.0)
        {
            Segment seg;
            seg.begin = s.begin;
            seg.end = int(s.begin + std_part);
            seg.rate = base;
            seg.overtime = false;
            segs.push_back(seg);
            day_total += std_part * base;
        }

        const double ot_part = len - std_part;
        if (ot_part > 0.0)
        {
            Segment seg;
            seg.begin = int(s.begin + std_part);
            seg.end = s.end;
            seg.rate = base * mult;
            seg.overtime = true;
            segs.push_back(seg);
            day_total += ot_part * seg.rate;
            ot_total += ot_part;
        }

        used_std += std_part;
    }

    if (segs_out != nullptr)
        *segs_out = segs;
    if (ot_total_out != nullptr)
        *ot_total_out = ot_total;
    return day_total;
}

// ---------------------------------------------------------------- 当日结果
DayResult ComputeDay(const SalaryConfig& cfg, const LocalTime& now)
{
    DayResult r;
    r.wday = now.wday;

    PayContext ctx = MakeContext(cfg, now.year, now.month);

    std::vector<Segment> segs;
    double ot_total = 0.0;
    r.day_total = DayTotal(cfg, ctx, now.wday, &segs, &ot_total, nullptr);
    r.ot_total = ot_total;
    r.is_workday = !segs.empty();

    double shift_end = segs.empty() ? 0.0 : double(segs.back().end);
    for (const Segment& s : segs)
        r.shift_sec += (s.end - s.begin);

    const double sod = now.sec_of_day;

    // ---- 积分：0 点 -> 现在 ----
    bool inside = false;
    for (const Segment& s : segs)
    {
        const double b = s.begin;
        const double e = s.end;
        double overlap = 0.0;
        if (sod >= e)
            overlap = e - b;
        else if (sod > b)
            overlap = sod - b;

        if (overlap > 0.0)
        {
            r.earned += overlap * s.rate;
            r.worked_sec += overlap;
            if (s.overtime)
            {
                r.ot_sec += overlap;             // 已发生的加班时长
                r.ot_earned += overlap * s.rate; // 加班部分贡献的收入
            }
        }
        if (sod >= b && sod < e)
        {
            inside = true;
            r.rate_per_sec = s.rate;
        }
    }

    // ---- 状态与剩余时长 ----
    if (!r.is_workday)
    {
        r.phase = PHASE_REST;
    }
    else if (sod < segs.front().begin)
    {
        r.phase = PHASE_BEFORE;
        r.remain_sec = shift_end - sod;
    }
    else if (sod >= shift_end)
    {
        r.phase = PHASE_AFTER;
        r.remain_sec = 0.0;
    }
    else
    {
        r.phase = inside ? PHASE_WORKING : PHASE_BREAK;
        r.remain_sec = shift_end - sod;
    }

    if (r.worked_sec > 0.0)
        r.avg_hourly = r.earned / (r.worked_sec / 3600.0);
    r.time_permille = r.shift_sec > 0.0 ? ClampPermille(r.worked_sec / r.shift_sec) : 0;
    r.money_permille = r.day_total > 0.0 ? ClampPermille(r.earned / r.day_total) : 0;
    return r;
}

// ---------------------------------------------------------------- 月度 / 周 / 年
static double MonthTotalWithCtx(const SalaryConfig& cfg, const PayContext& ctx)
{
    double total = 0.0;
    const int dim = sched::DaysInMonth(ctx.year, ctx.month);
    for (int d = 1; d <= dim; ++d)
        total += DayTotal(cfg, ctx, sched::WeekdayOf(ctx.year, ctx.month, d), nullptr, nullptr, nullptr);
    return total;
}

Aggregates ComputeAggregates(const SalaryConfig& cfg, const LocalTime& now)
{
    Aggregates a;
    PayContext ctx = MakeContext(cfg, now.year, now.month);
    a.month_shift_sec = ctx.month_shift_sec;
    a.month_standard_sec = ctx.month_standard_sec;
    a.month_work_days = ctx.work_days;

    // 本月 1 号 ~ 昨天
    for (int d = 1; d < now.day; ++d)
        a.month_past += DayTotal(cfg, ctx, sched::WeekdayOf(now.year, now.month, d), nullptr, nullptr, nullptr);
    a.month_total = MonthTotalWithCtx(cfg, ctx);

    // 本周一 ~ 昨天（wday: 0=周日）
    int days_since_monday = (now.wday == 0) ? 6 : (now.wday - 1);
    for (int i = days_since_monday; i >= 1; --i)
    {
        int y = now.year, m = now.month, d = now.day - i;
        while (d < 1)
        {
            --m;
            if (m < 1)
            {
                m = 12;
                --y;
            }
            d += sched::DaysInMonth(y, m);
        }
        PayContext c2 = MakeContext(cfg, y, m);
        a.week_past += DayTotal(cfg, c2, sched::WeekdayOf(y, m, d), nullptr, nullptr, nullptr);
    }

    // 本年
    for (int m = 1; m <= 12; ++m)
    {
        PayContext cm = MakeContext(cfg, now.year, m);
        const double mt = MonthTotalWithCtx(cfg, cm);
        a.year_total += mt;
        if (m < now.month)
            a.year_past += mt;
        else if (m == now.month)
        {
            for (int d = 1; d < now.day; ++d)
                a.year_past += DayTotal(cfg, cm, sched::WeekdayOf(now.year, m, d), nullptr, nullptr, nullptr);
        }
    }
    return a;
}

} // namespace earnings
