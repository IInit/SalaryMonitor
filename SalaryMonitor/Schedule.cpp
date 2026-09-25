// Schedule.cpp : 排班 / 时段 / 当月工时 计算实现
#include "Schedule.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace sched
{

// ---------------------------------------------------------------- 日期基础
// 以 1970-01-01 为 0 的"天数"（Howard Hinnant 的 days_from_civil）
static int DaysFromCivil(int y, int m, int d)
{
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int mp = m > 2 ? m - 3 : m + 9;
    int doy = (153 * mp + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int DaysInMonth(int year, int month)
{
    static const int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12)
        return 30;
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        return 29;
    return days[month - 1];
}

int WeekdayOf(int year, int month, int day)
{
    // 1970-01-01 是周四 => (days + 4) % 7，0 = 周日
    int days = DaysFromCivil(year, month, day);
    int w = (days + 4) % 7;
    if (w < 0)
        w += 7;
    return w;
}

bool IsWeekend(int wday)
{
    return wday == 0 || wday == 6;
}

const wchar_t* WeekdayName(int wday)
{
    switch (wday)
    {
    case 0: return L"周日";
    case 1: return L"周一";
    case 2: return L"周二";
    case 3: return L"周三";
    case 4: return L"周四";
    case 5: return L"周五";
    case 6: return L"周六";
    default: return L"";
    }
}

LocalTime MakeLocalTime(int year, int month, int day, int hour, int minute, int second)
{
    LocalTime t;
    t.year = year;
    t.month = month;
    t.day = day;
    t.hour = hour;
    t.minute = minute;
    t.second = second;
    t.wday = WeekdayOf(year, month, day);
    t.sec_of_day = hour * 3600.0 + minute * 60.0 + second;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
    t.date = buf;
    return t;
}

// ---------------------------------------------------------------- 时间文本
int ParseHM(const std::string& text, int fallback_minutes)
{
    // 允许 "9:5" / "09:05" / "9：05"（全角冒号）这类写法
    int h = -1, m = -1;
    std::string s;
    s.reserve(text.size());
    for (char c : text)
    {
        if (c == ' ' || c == '\t')
            continue;
        if (c == ':')
            s += ':';
        else if (c == '\xA3' || (unsigned char)c == 0xEF)   // 全角冒号的首字节，粗略过滤
            s += ':';
        else
            s += c;
    }
    // 逐段解析
    size_t colon = s.find(':');
    if (colon == std::string::npos)
    {
        h = std::atoi(s.c_str());
        m = 0;
        if (s.empty())
            return fallback_minutes;
    }
    else
    {
        std::string hs = s.substr(0, colon);
        std::string ms = s.substr(colon + 1);
        if (hs.empty() || ms.empty())
            return fallback_minutes;
        h = std::atoi(hs.c_str());
        m = std::atoi(ms.c_str());
    }
    if (h < 0 || h > 23 || m < 0 || m > 59)
        return fallback_minutes;
    return h * 60 + m;
}

bool IsValidHM(const std::string& text)
{
    int h = 0, m = 0;
    std::string s;
    for (char c : text)
    {
        if (c != ' ' && c != '\t')
            s += c;
    }
    size_t colon = s.find(':');
    if (colon == std::string::npos)
        return false;
    std::string hs = s.substr(0, colon);
    std::string ms = s.substr(colon + 1);
    if (hs.empty() || ms.empty() || hs.size() > 2 || ms.size() > 2)
        return false;
    for (char c : hs)
        if (c < '0' || c > '9')
            return false;
    for (char c : ms)
        if (c < '0' || c > '9')
            return false;
    h = std::atoi(hs.c_str());
    m = std::atoi(ms.c_str());
    return h >= 0 && h <= 23 && m >= 0 && m <= 59;
}

std::string FormatHM(int minutes)
{
    if (minutes < 0)
        minutes = 0;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", (minutes / 60) % 24, minutes % 60);
    return buf;
}

// ---------------------------------------------------------------- 班次
const DayRule& RuleFor(const SalaryConfig& cfg, int wday)
{
    if (wday == 0)
        return cfg.sun;
    if (wday == 6)
        return cfg.sat;
    return cfg.weekday;
}

void ShiftsOf(const DayRule& rule, std::vector<Shift>& out)
{
    out.clear();

    auto push = [&out](bool on, const std::string& s, const std::string& e) {
        if (!on)
            return;
        int b = ParseHM(s, -1);
        int en = ParseHM(e, -1);
        if (b < 0 || en < 0 || en <= b)
            return;                       // 非法或零长度时段直接忽略
        Shift sh;
        sh.begin = b * 60;
        sh.end = en * 60;
        out.push_back(sh);
    };

    push(rule.am_on, rule.am_start, rule.am_end);
    push(rule.pm_on, rule.pm_start, rule.pm_end);

    std::sort(out.begin(), out.end(), [](const Shift& a, const Shift& b) { return a.begin < b.begin; });

    // 合并重叠 / 相邻段，避免重复计薪
    std::vector<Shift> merged;
    for (const Shift& s : out)
    {
        if (!merged.empty() && s.begin <= merged.back().end)
            merged.back().end = (std::max)(merged.back().end, s.end);
        else
            merged.push_back(s);
    }
    out.swap(merged);
}

double DayShiftSeconds(const SalaryConfig& cfg, int wday)
{
    std::vector<Shift> shifts;
    ShiftsOf(RuleFor(cfg, wday), shifts);
    double total = 0.0;
    for (const Shift& s : shifts)
        total += (s.end - s.begin);
    return total;
}

double DayStandardSeconds(const SalaryConfig& cfg, int wday)
{
    if (IsWeekend(wday))
        return 0.0;                       // 周末整班算加班
    double std_hours = cfg.standard_hours;
    if (std_hours < 0.0)
        std_hours = 0.0;
    return (std::min)(DayShiftSeconds(cfg, wday), std_hours * 3600.0);
}

int WorkDaysInMonth(const SalaryConfig& cfg, int year, int month)
{
    int n = DaysInMonth(year, month);
    int work = 0;
    for (int d = 1; d <= n; ++d)
    {
        if (DayShiftSeconds(cfg, WeekdayOf(year, month, d)) > 0.0)
            ++work;
    }
    return work;
}

double MonthShiftSeconds(const SalaryConfig& cfg, int year, int month)
{
    int n = DaysInMonth(year, month);
    double total = 0.0;
    for (int d = 1; d <= n; ++d)
        total += DayShiftSeconds(cfg, WeekdayOf(year, month, d));
    return total;
}

double MonthStandardSeconds(const SalaryConfig& cfg, int year, int month)
{
    int n = DaysInMonth(year, month);
    double total = 0.0;
    for (int d = 1; d <= n; ++d)
        total += DayStandardSeconds(cfg, WeekdayOf(year, month, d));
    return total;
}

int DaysToPayday(const SalaryConfig& cfg, const LocalTime& now)
{
    int payday = cfg.payday;
    if (payday < 1)
        payday = 1;
    if (payday > 31)
        payday = 31;

    int dim = DaysInMonth(now.year, now.month);
    int target = (std::min)(payday, dim);          // 2 月没有 30 号 -> 顺延到月末
    if (now.day < target)
        return target - now.day;
    if (now.day == target)
        return 0;

    // 已过本月发薪日 -> 看下个月
    int ny = now.year, nm = now.month + 1;
    if (nm > 12)
    {
        nm = 1;
        ++ny;
    }
    int ndim = DaysInMonth(ny, nm);
    int ntarget = (std::min)(payday, ndim);
    return (dim - now.day) + ntarget;
}

} // namespace sched
