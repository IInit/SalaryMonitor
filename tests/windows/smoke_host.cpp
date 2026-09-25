// smoke_host.cpp : SalaryMonitor 的冒烟测试宿主（两部分）
//
//   A. 计薪引擎单元断言：不依赖 MFC / TrafficMonitor，直接把核心源码
//      （SalaryConfig / Schedule / Professions / Earnings / Format）编进本程序，
//      用"固定时刻 + 固定日期"验证每一种计薪方式的结果。计薪是纯函数，
//      所以这些断言是确定性的，可以在 CI 上稳定复现。
//
//   B. 插件接口契约：按 PluginInterface.h 的约定加载 DLL：
//      LoadLibrary -> TMPluginGetInstance -> OnExtenedInfo -> OnInitialize
//      -> GetInfo / GetItem 枚举 / DataRequired / GetTooltipInfo / 命令列表 / 配置落盘
//
// 编译（x64）：见 build_x64.sh 末尾，或
//   cl /nologo /EHsc /std:c++17 /MD /utf-8 smoke_host.cpp <core .cpp...> \
//      /I..\..\SalaryMonitor /I..\..\SalaryMonitor\include
//
// 用法：PluginSmokeTest.exe <SalaryMonitor.dll 路径> [配置目录]
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "include/PluginInterface.h"
#include "Earnings.h"
#include "Fields.h"
#include "Format.h"
#include "Professions.h"
#include "SalaryConfig.h"
#include "Schedule.h"

typedef ITMPlugin* (*PFN_GetInstance)();

static int g_fail = 0;

static void Check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "[ OK ]" : "[FAIL]", what);
    if (!ok)
        ++g_fail;
}

static bool approx(double a, double b, double eps = 1e-6)
{
    return std::fabs(a - b) <= eps;
}

// ============================================================ A. 计薪引擎
// 测试基准日：2026-09-23 是周三；2026-09 共 30 天，其中 22 个工作日（双休排班）
static const int kYear = 2026;
static const int kMonth = 9;

static LocalTime mk(int day, int hour, int minute)
{
    return sched::MakeLocalTime(kYear, kMonth, day, hour, minute, 0);
}

static SalaryConfig preset(const char* id)
{
    SalaryConfig c;
    c.payday = 10;
    applyProfession(c, id);
    return c;
}

static void test_shifts()
{
    std::printf("\n---- A1. 排班 / 日期基础 ----\n");
    Check(sched::WeekdayOf(2026, 9, 23) == 3, "2026-09-23 是周三（测试基准成立）");
    Check(sched::WeekdayOf(2026, 9, 27) == 0, "2026-09-27 是周日");
    Check(sched::DaysInMonth(2026, 9) == 30, "2026-09 有 30 天");
    Check(sched::DaysInMonth(2024, 2) == 29, "2024-02 是闰月 29 天");
    Check(sched::ParseHM("09:30", -1) == 570, "ParseHM(\"09:30\") == 570");
    Check(sched::ParseHM("9:5", -1) == 545, "ParseHM(\"9:5\") == 545（宽松解析）");
    Check(sched::ParseHM("25:00", -1) == -1, "ParseHM 对非法时间返回 fallback");
    Check(sched::FormatHM(570) == "09:30", "FormatHM(570) == \"09:30\"");
    Check(sched::IsValidHM("13:30") && !sched::IsValidHM("13:70"), "IsValidHM 校验分钟范围");

    SalaryConfig c = preset("programmer");
    Check(sched::WorkDaysInMonth(c, kYear, kMonth) == 22, "程序员（双休）9 月出勤 22 天");
    Check(approx(sched::DayShiftSeconds(c, 3), 7.5 * 3600), "周三排班 7.5 小时（09:30-12:00 + 13:30-18:30）");
    Check(approx(sched::DayShiftSeconds(c, 0), 0.0), "周日休息、排班 0 小时");
    Check(approx(sched::DayStandardSeconds(c, 3), 7.5 * 3600), "标准秒数按实际排班截断（7.5h < 8h）");

    SalaryConfig c996 = preset("programmer996");
    Check(approx(sched::DayShiftSeconds(c996, 3), 10.0 * 3600), "996 工作日 10 小时");
    Check(approx(sched::DayStandardSeconds(c996, 3), 8.0 * 3600), "996 标准秒数 = 8 小时");
    Check(approx(sched::DayStandardSeconds(c996, 6), 0.0), "周末整班算加班（标准秒数 0）");
    Check(sched::WorkDaysInMonth(c996, kYear, kMonth) == 26, "996 9 月出勤 26 天（含 4 个周六）");
}

static void test_monthly_fixed()
{
    std::printf("\n---- A2. 月薪固定（教师 / 公务员） ----\n");
    SalaryConfig c = preset("teacher");
    const double day = earnings::DayTotal(c, earnings::MakeContext(c, kYear, kMonth), 3);
    // 月薪固定 = 月薪 ÷ 当月排班总时长，因此单日应得恒等于 月薪 / 出勤天数
    Check(approx(day, c.monthly_salary / 22.0, 1e-6), "单日应得 == 月薪 / 出勤天数");

    earnings::DayResult r1 = earnings::ComputeDay(c, mk(23, 7, 0));      // 07:50 之前
    earnings::DayResult r2 = earnings::ComputeDay(c, mk(23, 11, 0));     // 上午班进行中
    earnings::DayResult r2b = earnings::ComputeDay(c, mk(23, 11, 50));   // 上午班结束
    earnings::DayResult r3 = earnings::ComputeDay(c, mk(23, 12, 30));    // 午休
    earnings::DayResult r4 = earnings::ComputeDay(c, mk(23, 17, 30));    // 下班
    earnings::DayResult r5 = earnings::ComputeDay(c, mk(23, 20, 0));     // 晚上

    Check(r1.phase == earnings::PHASE_BEFORE && approx(r1.earned, 0.0), "07:00 未上班，已赚 0");
    Check(approx(r2b.earned, day * (4.0 / 7.5), 1e-9), "11:50 已赚 = 4 小时 / 7.5 小时");
    Check(approx(r2.rate_per_sec, day / (7.5 * 3600), 1e-12), "班内速率 = 日应得 / 排班秒数");
    Check(r3.phase == earnings::PHASE_BREAK, "12:30 处于午休（休息中）");
    Check(approx(r3.earned, r2b.earned, 1e-9), "午休期间不计薪（金额与 11:50 相同）");
    Check(approx(r3.rate_per_sec, 0.0), "午休速率为 0");
    Check(approx(r4.earned, day, 1e-9), "17:30 已赚满全天应得");
    Check(r5.phase == earnings::PHASE_AFTER && approx(r5.earned, day, 1e-9), "下班后金额冻结");
    Check(r4.money_permille == 1000, "下班时到账进度 100%");

    earnings::DayResult rest = earnings::ComputeDay(c, mk(27, 11, 0));   // 周日
    Check(rest.phase == earnings::PHASE_REST && approx(rest.earned, 0.0), "周日休息、不计薪");
    Check(approx(rest.day_total, 0.0), "周日应得 0");
}

static void test_monthly_ot()
{
    std::printf("\n---- A3. 月薪 + 加班费（程序员） ----\n");
    SalaryConfig c = preset("programmer996");
    const earnings::PayContext ctx = earnings::MakeContext(c, kYear, kMonth);
    const double base = c.monthly_salary / (22.0 * 8.0 * 3600.0);   // 月薪 ÷ 当月标准工时

    const double wed = earnings::DayTotal(c, ctx, 3);
    // 周三 10 小时 = 8 小时标准 + 2 小时 ×1.5
    Check(approx(wed, base * (8.0 * 3600 + 2.0 * 3600 * 1.5), 1e-6), "工作日：8h 标准 + 2h ×1.5 加班");

    const double sat_total = earnings::DayTotal(c, ctx, 6);
    // 周六 7.5 小时全部按 ×2
    Check(approx(sat_total, base * (7.5 * 3600 * 2.0), 1e-6), "周六：整班 7.5h ×2 倍率");

    earnings::DayResult r = earnings::ComputeDay(c, mk(23, 20, 0));   // 19:00 之后，已进入加班
    Check(r.ot_sec > 0.0, "20:00 今日已发生加班");
    Check(r.rate_per_sec > base * 1.4, "加班时段速率明显高于基准时薪");

    earnings::DayResult night = earnings::ComputeDay(c, mk(23, 21, 30));
    Check(night.phase == earnings::PHASE_AFTER && approx(night.earned, wed, 1e-6),
          "21:30 与全天应得一致（下班后不再增长）");

    // 关闭加班费 -> 月薪应当正好摊完整月（整月应得 == 月薪）
    SalaryConfig no_ot = c;
    no_ot.ot_enabled = false;
    const earnings::Aggregates agg_no_ot = earnings::ComputeAggregates(no_ot, mk(23, 15, 0));
    Check(approx(agg_no_ot.month_total, no_ot.monthly_salary, 1e-6),
          "关闭加班费：整月应得正好 == 月薪（月薪被整月摊平）");
}

static void test_hourly_daily_piece_commission()
{
    std::printf("\n---- A4. 时薪 / 日薪 / 计件 / 底薪+提成 ----\n");

    // ---- 时薪 ----
    SalaryConfig h = preset("tutor");        // 80 元/时，18:00-20:00，工作日
    earnings::DayResult hr = earnings::ComputeDay(h, mk(23, 19, 0));
    Check(approx(hr.day_total, 160.0, 1e-9), "家教：日应得 = 80 × 2 小时 = 160");
    Check(approx(hr.earned, 80.0, 1e-9), "19:00 已赚 1 小时 = 80");
    Check(approx(hr.rate_per_sec, 80.0 / 3600.0, 1e-12), "时薪模式速率 = 时薪 / 3600");

    // ---- 日薪 ----
    SalaryConfig d1 = preset("construction");   // 350/天，07:00-11:30 + 13:30-18:00，标准 8h
    const earnings::PayContext dctx = earnings::MakeContext(d1, kYear, kMonth);
    const double dtotal = earnings::DayTotal(d1, dctx, 3);
    // 排班 9 小时，超过标准 8 小时 -> 1 小时按 1.5 倍
    const double dbase = d1.daily_wage / (9.0 * 3600.0);
    Check(approx(dtotal, dbase * (8.0 * 3600 + 1.0 * 3600 * 1.5), 1e-6),
          "日薪 + 加班：8h + 1h ×1.5，当天超过日薪");
    SalaryConfig d2 = d1;
    d2.ot_enabled = false;
    Check(approx(earnings::DayTotal(d2, earnings::MakeContext(d2, kYear, kMonth), 3), 350.0, 1e-9),
          "日薪（关闭加班费）当天恰好 = 350");

    // ---- 计件 ----
    SalaryConfig p = preset("rider");     // 4 元/单 × 60 单，10:00-14:00 + 17:00-21:00，全年无休
    const earnings::PayContext pctx = earnings::MakeContext(p, kYear, kMonth);
    const double ptotal = earnings::DayTotal(p, pctx, 3);
    Check(approx(ptotal, 240.0, 1e-9), "骑手：日应得 = 4 × 60 = 240（不乘加班倍率）");
    earnings::DayResult pr = earnings::ComputeDay(p, mk(23, 12, 0));   // 排班 8 小时，过了 2 小时
    Check(approx(pr.earned, 60.0, 1e-9), "骑手 12:00 已赚 = 240 × 2/8 = 60");
    Check(approx(earnings::DayTotal(p, pctx, 0), 240.0, 1e-9), "骑手周日同样上班（全年无休）");

    // ---- 底薪 + 提成 ----
    SalaryConfig s = preset("sales");     // 底薪 4000 + 5% 提成，当日业绩 5000
    const earnings::PayContext sctx = earnings::MakeContext(s, kYear, kMonth);
    const double stotal = earnings::DayTotal(s, sctx, 3);
    const int work_days = sched::WorkDaysInMonth(s, kYear, kMonth);
    Check(approx(stotal, 4000.0 / work_days + 5000.0 * 0.05, 1e-6),
          "销售：日应得 = 底薪/出勤天数 + 业绩 × 提成比例");
    Check(stotal > 400.0, "销售单日应得 > 400（底薪 + 提成合计）");
}

// A4.5 职业预设自洽性
//
// 这一组断言来自一个真实缺陷：早期「家教 / 主播」预设把同一个时段同时写进
// 上午和下午（am 与 pm 都是 18:00-20:00）。计薪引擎会把两段合并，所以钱算得
// 没错，但设置对话框当时会把它判定成"下午早于上午结束"而拒绝保存——用户套用
// 预设后一按确定就弹错。因此这里同时锁住两件事：
//   1. 每个预设的排班都必须能通过设置对话框的校验口径（两段不能倒挂）；
//   2. 晚间单时段职业的当日排班时长必须正好等于其真实工时。
static void test_profession_presets()
{
    std::printf("\n---- A4.5 职业预设自洽性 ----\n");

    int inverted = 0;
    int no_weekday = 0;
    for (const ProfessionDef& p : professions())
    {
        SalaryConfig c = preset(p.id.c_str());
        const DayRule& wd = c.weekday;

        if (wd.am_on && wd.pm_on)
        {
            const int am_e = sched::ParseHM(wd.am_end, -1);
            const int pm_s = sched::ParseHM(wd.pm_start, -1);
            if (am_e >= 0 && pm_s >= 0 && pm_s < am_e)
            {
                ++inverted;
                std::printf("       预设 %s 上午 %s-%s 与下午 %s-%s 倒挂（设置窗口会拒绝保存）\n",
                            p.id.c_str(), wd.am_start.c_str(), wd.am_end.c_str(),
                            wd.pm_start.c_str(), wd.pm_end.c_str());
            }
        }

        if (sched::DayShiftSeconds(c, 3) <= 0.0)
        {
            ++no_weekday;
            std::printf("       预设 %s 工作日排班为 0 小时\n", p.id.c_str());
        }
    }
    Check(inverted == 0, "所有预设的上午/下午时段都没有倒挂（设置窗口可直接保存）");
    Check(no_weekday == 0, "所有预设的工作日都有实际排班");

    // 晚间单时段：排班时长必须正好是真实工时（不能因为写了两遍而翻倍）
    SalaryConfig tutor = preset("tutor");
    Check(approx(sched::DayShiftSeconds(tutor, 3), 2.0 * 3600), "家教工作日排班 = 2 小时（18:00-20:00）");
    Check(approx(earnings::ComputeDay(tutor, mk(23, 19, 0)).earned, 80.0, 1e-9),
          "家教 19:00 已赚 = 80（时薪 80 × 1 小时）");

    SalaryConfig streamer = preset("streamer");
    Check(approx(sched::DayShiftSeconds(streamer, 3), 3.0 * 3600), "主播工作日排班 = 3 小时（20:00-23:00）");
    Check(approx(sched::DayShiftSeconds(streamer, 0), 3.0 * 3600), "主播周日同样 3 小时（全年开播）");
}

static void test_aggregates_and_misc()
{
    std::printf("\n---- A5. 月度 / 年度累计与发薪日 ----\n");
    SalaryConfig c = preset("civil");     // 8000 元，双休，09:00-12:00 + 13:30-17:30 = 7h/天
    LocalTime now = mk(23, 15, 0);
    earnings::Aggregates a = earnings::ComputeAggregates(c, now);

    Check(approx(a.month_shift_sec, 22.0 * 7.0 * 3600.0, 1e-6), "9 月排班总时长 = 22 × 7h");
    Check(approx(a.month_total, c.monthly_salary, 1e-6), "月薪固定制：整月应得 == 月薪");
    Check(approx(a.year_total, c.monthly_salary * 12.0, 1e-6), "年薪预估 == 月薪 × 12（双休按月摊平）");
    Check(a.month_past > 0.0 && a.month_past < a.month_total, "本月已过天数收入介于 0 与整月之间");

    const earnings::DayResult today = earnings::ComputeDay(c, now);
    Check(a.month_past + today.earned < a.month_total,
          "本月累计（过去天数 + 今日已赚）尚未超过整月应得");
    Check(approx(today.earned, today.day_total * (today.worked_sec / today.shift_sec), 1e-6),
          "月薪固定制：今日已赚与工时进度严格成正比");

    Check(sched::DaysToPayday(c, mk(5, 10, 0)) == 5, "5 号距发薪日（10 号）还有 5 天");
    Check(sched::DaysToPayday(c, mk(10, 10, 0)) == 0, "10 号就是发薪日");
    Check(sched::DaysToPayday(c, mk(20, 10, 0)) == 20, "20 号距下月 10 号还有 20 天");

    std::printf("\n---- A6. 数值格式化 ----\n");
    Check(fmt::Number(1234.5, 2, true) == "1,234.50", "千分位：1,234.50");
    Check(fmt::Number(-1234567.891, 2, true) == "-1,234,567.89", "负数与千分位：-1,234,567.89");
    Check(fmt::Number(0.5, 0, false) == "1", "零位小数四舍五入：0.5 -> 1");
    Check(fmt::Duration(3661) == "1h01m", "时长：1h01m");
    Check(fmt::Duration(90) == "1m30s", "时长：1m30s");
    Check(fmt::Duration(42) == "42s", "时长：42s");
    Check(fmt::Percent(684) == "68.4%", "百分比：68.4%");
    {
        SalaryConfig mc = preset("custom");
        mc.currency = "\xC2\xA5";
        mc.decimals = 2;
        Check(fmt::Money(1234.5, mc) == "\xC2\xA5""1,234.50", "金额带符号：¥1,234.50");
    }
}

static void test_config_roundtrip()
{
    std::printf("\n---- A7. 配置序列化 ----\n");
    SalaryConfig c = preset("rider");
    c.monthly_salary = 12345.67;
    c.unit_count = 77;
    c.sat.am_on = false;
    c.sat.pm_start = "15:45";
    c.currency = "$";
    c.decimals = 3;

    std::string json = c.toJson();
    SalaryConfig back;
    Check(back.fromJson(json), "配置 JSON 可回读");
    Check(approx(back.monthly_salary, 12345.67, 1e-9), "数值字段回读一致");
    Check(approx(back.unit_count, 77.0, 1e-9), "计件数量回读一致");
    Check(back.mode == c.mode, "计薪方式回读一致");
    Check(back.profession == c.profession, "职业回读一致");
    Check(back.sat.am_on == false && back.sat.pm_start == "15:45", "星期六排班回读一致");
    Check(back.currency == "$" && back.decimals == 3, "显示口径回读一致");

    // 职业预设：每个预设都必须能算出一个正数日薪，否则是配置写错了
    int bad = 0;
    for (const ProfessionDef& p : professions())
    {
        SalaryConfig pc = preset(p.id.c_str());
        const earnings::PayContext ctx = earnings::MakeContext(pc, kYear, kMonth);
        double total = 0.0;
        for (int wd = 1; wd <= 5; ++wd)
            total += earnings::DayTotal(pc, ctx, wd);
        if (total <= 0.0)
        {
            ++bad;
            std::printf("       预设 %s 工作日应得为 0\n", p.id.c_str());
        }
    }
    Check(bad == 0, "所有职业预设都能算出正数收入");
}

// ============================================================ B. 插件契约
static std::string ToUtf8(const wchar_t* w)
{
    if (w == nullptr)
        return std::string("(null)");
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1)
        return std::string();
    std::string s(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], len, nullptr, nullptr);
    return s;
}

static bool FileContains(const std::wstring& path, const char* needle)
{
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path.c_str(), L"rb") != 0 || fp == nullptr)
        return false;
    std::string data;
    char buf[8192];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0)
        data.append(buf, n);
    std::fclose(fp);
    return data.find(needle) != std::string::npos;
}

static void test_plugin(const wchar_t* dll_path, const std::wstring& cfg_dir)
{
    std::printf("\n==== B. 插件接口契约 ====\n");
    std::printf("DLL: %s\nCFG: %s\n\n", ToUtf8(dll_path).c_str(), ToUtf8(cfg_dir.c_str()).c_str());

    HMODULE mod = ::LoadLibraryW(dll_path);
    if (mod == nullptr)
    {
        std::printf("[FAIL] LoadLibrary failed, GetLastError=%lu\n", ::GetLastError());
        ++g_fail;
        return;
    }
    Check(true, "DLL 加载成功");

    PFN_GetInstance get_instance = (PFN_GetInstance)::GetProcAddress(mod, "TMPluginGetInstance");
    Check(get_instance != nullptr, "导出 TMPluginGetInstance");
    if (get_instance == nullptr)
        return;

    ITMPlugin* plugin = get_instance();
    Check(plugin != nullptr, "TMPluginGetInstance() 返回非空");
    if (plugin == nullptr)
        return;

    int api = plugin->GetAPIVersion();
    std::printf("[INFO] GetAPIVersion() = %d\n", api);
    Check(api >= 7, "插件接口版本 >= 7（用到 OnInitialize）");

    const wchar_t* name = plugin->GetInfo(ITMPlugin::TMI_NAME);
    std::printf("[INFO] TMI_NAME        = %s\n", ToUtf8(name).c_str());
    std::printf("[INFO] TMI_DESCRIPTION = %s\n", ToUtf8(plugin->GetInfo(ITMPlugin::TMI_DESCRIPTION)).c_str());
    std::printf("[INFO] TMI_AUTHOR      = %s\n", ToUtf8(plugin->GetInfo(ITMPlugin::TMI_AUTHOR)).c_str());
    std::printf("[INFO] TMI_VERSION     = %s\n", ToUtf8(plugin->GetInfo(ITMPlugin::TMI_VERSION)).c_str());
    std::printf("[INFO] TMI_URL         = %s\n", ToUtf8(plugin->GetInfo(ITMPlugin::TMI_URL)).c_str());
    Check(name != nullptr && name[0] != 0, "TMI_NAME 非空");
    Check(std::wstring(plugin->GetInfo(ITMPlugin::TMI_AUTHOR)) == L"init", "TMI_AUTHOR == init");
    Check(std::wstring(plugin->GetInfo(ITMPlugin::TMI_URL)) == L"https://github.com/IInit/SalaryMonitor",
          "TMI_URL == https://github.com/IInit/SalaryMonitor");

    for (int i = 0; i <= (int)ITMPlugin::TMI_MAX; ++i)
    {
        const wchar_t* v = plugin->GetInfo((ITMPlugin::PluginInfoIndex)i);
        if (v == nullptr)
            continue;
        std::wstring s(v);
        Check(s.find(L"PowerMonitor") == std::wstring::npos &&
              s.find(L"\u6478\u9c7c") == std::wstring::npos,
              "插件信息里没有旧品牌 / 摸鱼残留");
    }

    plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, cfg_dir.c_str());
    plugin->OnInitialize(nullptr);
    Check(true, "OnExtenedInfo + OnInitialize 执行完成");

    std::printf("\n---- 显示项 ----\n");
    int count = 0;
    for (int i = 0; i < 64; ++i)
    {
        IPluginItem* item = plugin->GetItem(i);
        if (item == nullptr)
            break;
        ++count;
        std::printf("%2d | id=%-14s | name=%-12s | label=%-10s | value=%-14s | sample=%s\n",
                    i,
                    ToUtf8(item->GetItemId()).c_str(),
                    ToUtf8(item->GetItemName()).c_str(),
                    ToUtf8(item->GetItemLableText()).c_str(),
                    ToUtf8(item->GetItemValueText()).c_str(),
                    ToUtf8(item->GetItemValueSampleText()).c_str());
    }
    std::printf("item count = %d\n", count);
    Check(count == (int)fieldDefs().size(), "显示项数量与字段表一致");
    Check(plugin->GetItem(-1) == nullptr, "GetItem(-1) == nullptr");
    Check(plugin->GetItem(count) == nullptr, "GetItem(count) == nullptr");

    plugin->DataRequired();
    ::Sleep(1100);
    plugin->DataRequired();

    std::printf("\n---- DataRequired 之后的取值 ----\n");
    int empty_values = 0;
    for (int i = 0; i < count; ++i)
    {
        IPluginItem* item = plugin->GetItem(i);
        std::string v = ToUtf8(item->GetItemValueText());
        std::printf("%2d | %-10s = %s\n", i, ToUtf8(item->GetItemLableText()).c_str(), v.c_str());
        if (v.empty())
            ++empty_values;
    }
    Check(empty_values == 0, "所有显示项都有非空取值");

    const wchar_t* tip = plugin->GetTooltipInfo();
    std::printf("\n---- 鼠标提示 ----\n%s\n", ToUtf8(tip).c_str());
    Check(tip != nullptr && tip[0] != 0, "鼠标提示非空");

    int cmds = plugin->GetCommandCount();
    std::printf("\n---- 命令（%d） ----\n", cmds);
    for (int i = 0; i < cmds; ++i)
        std::printf("%d: %s  checked=%d\n", i, ToUtf8(plugin->GetCommandName(i)).c_str(),
                    plugin->IsCommandChecked(i));
    Check(cmds == 5, "命令数量 == 5");

    void* icon = plugin->GetPluginIcon();
    std::printf("\n[%s] GetPluginIcon() = %p\n", icon != nullptr ? " OK " : "WARN", icon);

    std::wstring cfg_file = cfg_dir + L"\\SalaryMonitor_config.json";
    Check(::GetFileAttributesW(cfg_file.c_str()) != INVALID_FILE_ATTRIBUTES, "配置文件已写入");
    Check(FileContains(cfg_file, "\"profession\""), "配置文件内容包含 profession 字段");
    Check(!FileContains(cfg_file, "PowerMonitor"), "配置文件没有旧品牌残留");

    ::FreeLibrary(mod);
}

int wmain(int argc, wchar_t** argv)
{
    ::SetConsoleOutputCP(65001);
    setvbuf(stdout, nullptr, _IONBF, 0);

    if (argc < 2)
    {
        std::printf("usage: PluginSmokeTest.exe <SalaryMonitor.dll> [config_dir]\n");
        return 2;
    }
    const wchar_t* dll_path = argv[1];
    std::wstring cfg_dir = argc >= 3 ? argv[2] : L"";
    if (cfg_dir.empty())
    {
        cfg_dir = dll_path;
        size_t pos = cfg_dir.find_last_of(L"\\/");
        cfg_dir = (pos == std::wstring::npos ? std::wstring(L".") : cfg_dir.substr(0, pos)) + L"\\_smokecfg";
    }
    ::CreateDirectoryW(cfg_dir.c_str(), nullptr);

    std::printf("==== A. 计薪引擎单元断言 ====\n");
    test_shifts();
    test_monthly_fixed();
    test_monthly_ot();
    test_hourly_daily_piece_commission();
    test_profession_presets();
    test_aggregates_and_misc();
    test_config_roundtrip();

    test_plugin(dll_path, cfg_dir);

    std::printf("\n==== %s (%d failure) ====\n", g_fail == 0 ? "PASS" : "FAIL", g_fail);
    return g_fail == 0 ? 0 : 1;
}
