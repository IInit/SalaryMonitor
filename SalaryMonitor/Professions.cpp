// Professions.cpp : 职业预设实现
#include "Professions.h"
#include <cstring>

// ---------------------------------------------------------------- 预设列表
static const std::vector<ProfessionDef>& table()
{
    static const std::vector<ProfessionDef> t = {
        { "programmer",    L"程序员",           L"月薪按当月标准工时摊成秒薪；每天超出标准工时的部分按 1.5× 计，周末加班 2×。", PAY_MONTHLY_OT },
        { "programmer996", L"程序员（995/996）", L"同上，但排班到 21:00 且周六上班，加班费占比更高。", PAY_MONTHLY_OT },
        { "teacher",       L"教师",             L"月薪固定，按当月排班总时长均摊；寒暑假不计薪请把假期月份的排班关掉。", PAY_MONTHLY_FIXED },
        { "civil",         L"公务员 / 事业单位", L"月薪固定，按月摊平，朝九晚五不加倍率。", PAY_MONTHLY_FIXED },
        { "doctor",        L"医生 / 护士",       L"月薪 + 加班费：超出标准工时的部分 1.5×，周末值班 2×；夜班请把时段填进排班。", PAY_MONTHLY_OT },
        { "accountant",    L"会计 / 财务",       L"月薪固定，按月摊平；月末结账加班请改排班结束时间。", PAY_MONTHLY_FIXED },
        { "banker",        L"银行柜员",         L"月薪固定，按月摊平。", PAY_MONTHLY_FIXED },
        { "clerk",         L"客服 / 文员",       L"月薪固定，按月摊平。", PAY_MONTHLY_FIXED },
        { "sales",         L"销售",             L"底薪摊到当月出勤天数 + 当日业绩 × 提成比例，再按当天排班摊平。", PAY_COMMISSION },
        { "barber",        L"理发师 / 美甲师",   L"底薪 + 当日业绩提成，按当天排班摊平。", PAY_COMMISSION },
        { "rider",         L"外卖骑手",         L"件单价 × 当日单量，按当天排班摊平（相当于把当日收入匀速摊到每一秒）。", PAY_PIECE },
        { "courier",       L"快递员",           L"件单价 × 当日件数，按当天排班摊平。", PAY_PIECE },
        { "factory",       L"工厂计件",         L"件单价 × 当日产量，按当天排班摊平。", PAY_PIECE },
        { "driver",        L"网约车 / 货车司机", L"按小时流水单价 × 班内已工作小时累计。", PAY_HOURLY },
        { "tutor",         L"家教 / 兼职",       L"按时薪 × 班内已工作小时累计。", PAY_HOURLY },
        { "lawyer",        L"律师",             L"按小时费率 × 班内已工作小时累计（按可计费小时估算）。", PAY_HOURLY },
        { "streamer",      L"主播 / 自媒体",     L"按折算时薪 × 直播小时累计；礼物打赏不稳定时可改成「底薪+提成」。", PAY_HOURLY },
        { "construction",  L"建筑 / 日结工",     L"当日日薪按排班摊平；加班超过标准工时的部分另计倍率。", PAY_DAILY },
        { "freelancer",    L"自由职业",         L"当日日薪（项目日单价）按排班摊平。", PAY_DAILY },
        { "custom",        L"自定义",           L"完全按你自己填的参数与排班计算，不受任何预设影响。", PAY_MONTHLY_FIXED },
    };
    return t;
}

const std::vector<ProfessionDef>& professions()
{
    return table();
}

const ProfessionDef* findProfession(const std::string& id)
{
    for (const ProfessionDef& p : table())
        if (p.id == id)
            return &p;
    return nullptr;
}

const ProfessionDef* findProfession(int index)
{
    const std::vector<ProfessionDef>& t = table();
    if (index < 0 || index >= (int)t.size())
        return nullptr;
    return &t[(size_t)index];
}

int professionIndex(const std::string& id)
{
    const std::vector<ProfessionDef>& t = table();
    for (size_t i = 0; i < t.size(); ++i)
        if (t[i].id == id)
            return (int)i;
    return (int)t.size() - 1;   // 未知 id 落到「自定义」
}

std::wstring professionName(const std::string& id)
{
    const ProfessionDef* p = findProfession(id);
    return p != nullptr ? std::wstring(p->name) : std::wstring(L"自定义");
}

std::wstring professionSummary(const std::string& id)
{
    const ProfessionDef* p = findProfession(id);
    return p != nullptr ? std::wstring(p->summary) : std::wstring();
}

// ---------------------------------------------------------------- 套用预设
namespace
{
    void setDay(DayRule& r, const char* am_s, const char* am_e, const char* pm_s, const char* pm_e)
    {
        r.am_on = true;
        r.pm_on = true;
        r.am_start = am_s;
        r.am_end = am_e;
        r.pm_start = pm_s;
        r.pm_end = pm_e;
    }

    void setRest(DayRule& r)
    {
        r.am_on = false;
        r.pm_on = false;
    }

    // 只要一个半天（用于晚间 / 单时段工作：家教、主播等）
    //
    // 注意：不要把同一个时段同时写进上午和下午。虽然 ShiftsOf() 会把重叠段
    // 合并（不会重复计薪），但设置对话框会把它当成"下午早于上午结束"而拒绝
    // 保存——用户套用预设后一按确定就报错。
    void setSingle(DayRule& r, bool morning, const char* s, const char* e)
    {
        r.am_on = morning;
        r.pm_on = !morning;
        if (morning)
        {
            r.am_start = s;
            r.am_end = e;
        }
        else
        {
            r.pm_start = s;
            r.pm_end = e;
        }
    }
}

void applyProfession(SalaryConfig& cfg, const std::string& id)
{
    const std::string keep_prof = id.empty() ? cfg.profession : id;
    const ProfessionDef* def = findProfession(keep_prof);
    cfg.profession = def != nullptr ? def->id : "custom";

    // 显示口径（小数位 / 货币 / 发薪日 / 千分位）属于个人偏好，套用职业时不覆盖
    const int keep_payday = cfg.payday;
    const double keep_std_hours = cfg.standard_hours;

    const std::string& k = cfg.profession;

    if (k == "programmer")
    {
        cfg.mode = PAY_MONTHLY_OT;
        cfg.monthly_salary = 20000.0;
        cfg.ot_enabled = true;
        cfg.standard_hours = 8.0;
        cfg.ot_weekday = 1.5;
        cfg.ot_weekend = 2.0;
        setDay(cfg.weekday, "09:30", "12:00", "13:30", "18:30");
        setRest(cfg.sat);
        setRest(cfg.sun);
    }
    else if (k == "programmer996")
    {
        cfg.mode = PAY_MONTHLY_OT;
        cfg.monthly_salary = 25000.0;
        cfg.ot_enabled = true;
        cfg.standard_hours = 8.0;
        cfg.ot_weekday = 1.5;
        cfg.ot_weekend = 2.0;
        setDay(cfg.weekday, "09:30", "12:00", "13:30", "21:00");
        setDay(cfg.sat, "09:30", "12:00", "13:30", "18:30");
        setRest(cfg.sun);
    }
    else if (k == "teacher")
    {
        cfg.mode = PAY_MONTHLY_FIXED;
        cfg.monthly_salary = 9000.0;
        setDay(cfg.weekday, "07:50", "11:50", "14:00", "17:30");
        setRest(cfg.sat);
        setRest(cfg.sun);
    }
    else if (k == "civil")
    {
        cfg.mode = PAY_MONTHLY_FIXED;
        cfg.monthly_salary = 8000.0;
        setDay(cfg.weekday, "09:00", "12:00", "13:30", "17:30");
        setRest(cfg.sat);
        setRest(cfg.sun);
    }
    else if (k == "doctor")
    {
        cfg.mode = PAY_MONTHLY_OT;
        cfg.monthly_salary = 15000.0;
        cfg.ot_enabled = true;
        cfg.standard_hours = 8.0;
        cfg.ot_weekday = 1.5;
        cfg.ot_weekend = 2.0;
        setDay(cfg.weekday, "08:00", "12:00", "14:00", "17:30");
        setDay(cfg.sat, "08:00", "12:00", "14:00", "17:30");
        setRest(cfg.sun);
    }
    else if (k == "accountant")
    {
        cfg.mode = PAY_MONTHLY_FIXED;
        cfg.monthly_salary = 9000.0;
        setDay(cfg.weekday, "09:00", "12:00", "13:30", "17:30");
        setRest(cfg.sat);
        setRest(cfg.sun);
    }
    else if (k == "banker")
    {
        cfg.mode = PAY_MONTHLY_FIXED;
        cfg.monthly_salary = 10000.0;
        setDay(cfg.weekday, "08:30", "12:00", "13:30", "17:30");
        setRest(cfg.sat);
        setRest(cfg.sun);
    }
    else if (k == "clerk")
    {
        cfg.mode = PAY_MONTHLY_FIXED;
        cfg.monthly_salary = 6000.0;
        setDay(cfg.weekday, "09:00", "12:00", "13:30", "18:00");
        setRest(cfg.sat);
        setRest(cfg.sun);
    }
    else if (k == "sales")
    {
        cfg.mode = PAY_COMMISSION;
        cfg.base_monthly = 4000.0;
        cfg.commission_rate = 0.05;
        cfg.business_amount = 5000.0;
        setDay(cfg.weekday, "09:00", "12:00", "13:30", "18:00");
        setDay(cfg.sat, "09:00", "12:00", "13:30", "18:00");
        setRest(cfg.sun);
    }
    else if (k == "barber")
    {
        cfg.mode = PAY_COMMISSION;
        cfg.base_monthly = 3000.0;
        cfg.commission_rate = 0.30;
        cfg.business_amount = 800.0;
        setDay(cfg.weekday, "10:00", "14:00", "15:00", "21:00");
        setDay(cfg.sat, "10:00", "14:00", "15:00", "21:00");
        setDay(cfg.sun, "10:00", "14:00", "15:00", "21:00");
    }
    else if (k == "rider")
    {
        cfg.mode = PAY_PIECE;
        cfg.unit_price = 4.0;
        cfg.unit_count = 60.0;
        setDay(cfg.weekday, "10:00", "14:00", "17:00", "21:00");
        setDay(cfg.sat, "10:00", "14:00", "17:00", "21:00");
        setDay(cfg.sun, "10:00", "14:00", "17:00", "21:00");
    }
    else if (k == "courier")
    {
        cfg.mode = PAY_PIECE;
        cfg.unit_price = 1.5;
        cfg.unit_count = 200.0;
        setDay(cfg.weekday, "07:30", "12:00", "13:30", "18:30");
        setDay(cfg.sat, "07:30", "12:00", "13:30", "18:30");
        setRest(cfg.sun);
    }
    else if (k == "factory")
    {
        cfg.mode = PAY_PIECE;
        cfg.unit_price = 1.2;
        cfg.unit_count = 300.0;
        setDay(cfg.weekday, "08:00", "12:00", "13:00", "17:30");
        setDay(cfg.sat, "08:00", "12:00", "13:00", "17:30");
        setRest(cfg.sun);
    }
    else if (k == "driver")
    {
        cfg.mode = PAY_HOURLY;
        cfg.hourly_wage = 45.0;
        cfg.ot_enabled = false;
        setDay(cfg.weekday, "07:00", "11:00", "16:00", "21:00");
        setDay(cfg.sat, "07:00", "11:00", "16:00", "21:00");
        setDay(cfg.sun, "07:00", "11:00", "16:00", "21:00");
    }
    else if (k == "tutor")
    {
        cfg.mode = PAY_HOURLY;
        cfg.hourly_wage = 80.0;
        cfg.ot_enabled = false;
        setSingle(cfg.weekday, false, "18:00", "20:00");   // 晚间单时段（放进下午半段）
        setRest(cfg.sat);
        setRest(cfg.sun);
    }
    else if (k == "lawyer")
    {
        cfg.mode = PAY_HOURLY;
        cfg.hourly_wage = 800.0;
        cfg.ot_enabled = false;
        setDay(cfg.weekday, "09:00", "12:00", "13:30", "18:00");
        setRest(cfg.sat);
        setRest(cfg.sun);
    }
    else if (k == "streamer")
    {
        cfg.mode = PAY_HOURLY;
        cfg.hourly_wage = 150.0;
        cfg.ot_enabled = false;
        setSingle(cfg.weekday, false, "20:00", "23:00");   // 晚间单时段（放进下午半段）
        setSingle(cfg.sat, false, "20:00", "23:00");
        setSingle(cfg.sun, false, "20:00", "23:00");
    }
    else if (k == "construction")
    {
        cfg.mode = PAY_DAILY;
        cfg.daily_wage = 350.0;
        cfg.ot_enabled = true;
        cfg.standard_hours = 8.0;
        cfg.ot_weekday = 1.5;
        cfg.ot_weekend = 2.0;
        setDay(cfg.weekday, "07:00", "11:30", "13:30", "18:00");
        setDay(cfg.sat, "07:00", "11:30", "13:30", "18:00");
        setDay(cfg.sun, "07:00", "11:30", "13:30", "18:00");
    }
    else if (k == "freelancer")
    {
        cfg.mode = PAY_DAILY;
        cfg.daily_wage = 500.0;
        cfg.ot_enabled = false;
        setDay(cfg.weekday, "09:00", "12:00", "14:00", "18:00");
        setDay(cfg.sat, "09:00", "12:00", "14:00", "18:00");
        setDay(cfg.sun, "09:00", "12:00", "14:00", "18:00");
    }
    else
    {
        // 自定义：保留当前所有参数，只把职业标记改成 custom
        cfg.profession = "custom";
    }

    // 标准工时：所有预设都用 8 小时（即法定标准工时），但如果用户此前手工调过，
    // 就尊重用户的值——它是"加班从第几小时开始"的判定口径，属于个人习惯。
    if (keep_std_hours > 0.0)
        cfg.standard_hours = keep_std_hours;
    cfg.payday = keep_payday;
}
