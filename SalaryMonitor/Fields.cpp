// Fields.cpp : 显示项定义与取值实现
#include "Fields.h"
#include "Format.h"
#include "Professions.h"
#include <cstdio>

#ifdef _WIN32
#include <windows.h>   // WideCharToMultiByte（宽字符常量 -> UTF-8）
#endif

static const std::vector<FieldDef>& table()
{
    static const std::vector<FieldDef> t = {
        { "today_money",    "今日已赚",     "今日",     "\xC2\xA5""88,888.88" },
        { "today_total",    "今日应得",     "应得",     "\xC2\xA5""88,888.88" },
        { "today_rate",     "当前时薪",     "时薪",     "\xC2\xA5""1,288.88" },
        { "today_progress", "今日工时进度", "工时进度", "100.0%" },
        { "money_progress", "今日到账进度", "到账进度", "100.0%" },
        { "today_bar",      "今日进度条",   "进度条",   "" },
        { "worked",         "今日已工作时长", "已工作", "12h00m" },
        { "remain",         "距下班",       "距下班",   "12h00m" },
        { "overtime",       "今日加班时长", "加班",     "12h00m" },
        { "status",         "出勤状态",     "状态",     "休息中" },
        { "per_hour",       "每小时进账",   "每小时",   "\xC2\xA5""1,288.88" },
        { "per_minute",     "每分钟进账",   "每分钟",   "\xC2\xA5""88.8888" },
        { "per_second",     "每秒进账",     "每秒",     "\xC2\xA5""8.888888" },
        { "month_money",    "本月已赚",     "本月",     "\xC2\xA5""888,888.88" },
        { "month_total",    "本月应得",     "月应得",   "\xC2\xA5""888,888.88" },
        { "month_progress", "本月进度",     "月进度",   "100.0%" },
        { "year_money",     "本年已赚",     "本年",     "\xC2\xA5""888,888.88" },
        { "payday_left",    "距发薪日",     "发薪",     "31 天" },
        { "profession",     "当前职业",     "职业",     "程序员（995/996）" },
        { "pay_mode",       "计薪方式",     "计薪",     "月薪 + 加班费" },
    };
    return t;
}

const std::vector<FieldDef>& fieldDefs()
{
    return table();
}

const FieldDef* findField(const std::string& key)
{
    for (const FieldDef& f : table())
        if (f.key == key)
            return &f;
    return nullptr;
}

std::string fieldLabelUtf8(const std::string& key)
{
    const FieldDef* f = findField(key);
    return f != nullptr ? f->label : key;
}

std::string fieldShortLabelUtf8(const std::string& key)
{
    const FieldDef* f = findField(key);
    return f != nullptr ? f->short_label : key;
}

std::string fieldSampleUtf8(const std::string& key)
{
    const FieldDef* f = findField(key);
    return f != nullptr ? f->sample : std::string("8888.88");
}

bool isCustomDrawField(const std::string& key)
{
    return key == "today_bar";
}

// ---------------------------------------------------------------- 取值
static std::string WideToUtf8(const std::wstring& w)
{
    // 只用于把 payModeName / PhaseName 这类宽字符常量转成 UTF-8
    if (w.empty())
        return std::string();
#ifdef _WIN32
    int len = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return std::string();
    std::string s((size_t)len, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], len, nullptr, nullptr);
    return s;
#else
    // 非 Windows 平台（单元测试）只需要 ASCII 兜底
    std::string s;
    for (wchar_t c : w)
        s += (c < 128) ? char(c) : '?';
    return s;
#endif
}

std::string fieldValueUtf8(const std::string& key, const Snapshot& s, const SalaryConfig& cfg)
{
    const earnings::DayResult& d = s.day;
    char buf[64];

    if (key == "today_money")
        return fmt::Money(d.earned, cfg);
    if (key == "today_total")
        return fmt::Money(d.day_total, cfg);
    if (key == "today_rate")
        return fmt::MoneyEx(d.rate_per_sec * 3600.0, cfg, 2);
    if (key == "today_progress")
        return fmt::Percent(d.time_permille);
    if (key == "money_progress")
        return fmt::Percent(d.money_permille);
    if (key == "today_bar")
        return fmt::Percent(d.money_permille);   // 自绘时也会用到（供鼠标提示等）
    if (key == "worked")
        return fmt::Duration(d.worked_sec);
    if (key == "remain")
        return d.is_workday ? fmt::Duration(d.remain_sec) : std::string("--");
    if (key == "overtime")
        return d.ot_sec > 0.5 ? fmt::Duration(d.ot_sec) : std::string("--");
    if (key == "status")
        return WideToUtf8(earnings::PhaseName(d.phase));

    if (key == "per_hour")
        return fmt::MoneyEx(d.rate_per_sec * 3600.0, cfg, 2);
    if (key == "per_minute")
        return fmt::MoneyEx(d.rate_per_sec * 60.0, cfg, 4);
    if (key == "per_second")
        return fmt::MoneyEx(d.rate_per_sec, cfg, 4);

    if (key == "month_money")
        return fmt::Money(s.month_earned, cfg);
    if (key == "month_total")
        return fmt::Money(s.agg.month_total, cfg);
    if (key == "month_progress")
    {
        int permille = 0;
        if (s.agg.month_total > 0.0)
        {
            double ratio = s.month_earned / s.agg.month_total;
            permille = ratio <= 0.0 ? 0 : (ratio >= 1.0 ? 1000 : (int)(ratio * 1000.0 + 0.5));
        }
        return fmt::Percent(permille);
    }
    if (key == "year_money")
        return fmt::Money(s.year_earned, cfg);
    if (key == "payday_left")
    {
        if (s.days_to_payday <= 0)
            return std::string("今天发薪");
        std::snprintf(buf, sizeof(buf), "%d 天", s.days_to_payday);
        return buf;
    }
    if (key == "profession")
        return WideToUtf8(professionName(s.profession_id));
    if (key == "pay_mode")
        return WideToUtf8(payModeName(cfg.mode));

    return std::string("--");
}
