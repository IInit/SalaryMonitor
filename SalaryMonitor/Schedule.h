// Schedule.h : 排班 / 时段 / 当月工时 计算
//
// 纯逻辑层，不依赖 Windows / MFC。
//
// 术语：
//   班次(Shift)   一天中的一段连续工作时间，用"当天内的秒偏移"表示，如 09:00 = 32400
//   排班时长      当天所有班次秒数之和（不含午休空档）
//   标准工时      每天前 standard_hours 小时（工作日），超出部分算加班；周末整班算加班
#pragma once

#include "SalaryConfig.h"
#include <string>
#include <vector>

// 一个"当前时刻"的本地时间快照（由 Windows 层填充）
struct LocalTime
{
    int year = 1970;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int wday = 4;              // 0 = 周日, 1 = 周一 ... 6 = 周六
    double sec_of_day = 0.0;   // 当天已过秒数（含小数，供平滑进账用）
    std::string date;          // "YYYY-MM-DD"
};

namespace sched
{
struct Shift
{
    int begin = 0;   // 当天内秒偏移，含
    int end = 0;     // 当天内秒偏移，不含
};

// 由年月日时分秒构造 LocalTime（自动补 wday / sec_of_day / date）
LocalTime MakeLocalTime(int year, int month, int day, int hour, int minute, int second);

// "09:30" -> 570（分钟）；解析失败返回 fallback
int ParseHM(const std::string& text, int fallback_minutes);
// 570 -> "09:30"
std::string FormatHM(int minutes);
// 校验 "HH:MM" 是否合法
bool IsValidHM(const std::string& text);

int DaysInMonth(int year, int month);
int WeekdayOf(int year, int month, int day);      // 0 = 周日
bool IsWeekend(int wday);
const wchar_t* WeekdayName(int wday);

const DayRule& RuleFor(const SalaryConfig& cfg, int wday);

// 当天班次（已按开始时间排序、过滤掉零长度与非法段、并把重叠段合并）
void ShiftsOf(const DayRule& rule, std::vector<Shift>& out);

// 当天排班秒数；0 表示当天休息
double DayShiftSeconds(const SalaryConfig& cfg, int wday);

// 当月实际出勤天数（排班秒数 > 0 的天数）
int WorkDaysInMonth(const SalaryConfig& cfg, int year, int month);

// 当月排班总秒数（用于月薪固定制的均摊基准）
double MonthShiftSeconds(const SalaryConfig& cfg, int year, int month);

// 当月标准秒数（仅工作日，按 standard_hours 截断；用于"月薪+加班"的基准时薪）
double MonthStandardSeconds(const SalaryConfig& cfg, int year, int month);

// 某天的标准秒数：工作日 = min(排班时长, 标准工时)，周末 = 0（整班算加班）
double DayStandardSeconds(const SalaryConfig& cfg, int wday);

// 距下一个发薪日的天数（1~31 号；当天即发薪日时返回 0）
int DaysToPayday(const SalaryConfig& cfg, const LocalTime& now);

} // namespace sched
