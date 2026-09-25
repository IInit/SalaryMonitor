// Earnings.h : 计薪引擎
//
// 核心思想：把"今天能赚多少"建模成一条**与时间一一对应的费率曲线**，
// 再把 0 点到现在的曲线面积积分出来。因此：
//   · 无需累计、无需状态机、重启/休眠/时钟跳变都不会算歪；
//   · 午休不计薪 = 该区间没有段；加班 = 该段的费率乘以倍率；
//   · 换职业 / 改工资后历史与当月口径会随之整体重算，口径始终自洽。
//
// 纯逻辑层，不依赖 Windows / MFC。
#pragma once

#include "SalaryConfig.h"
#include "Schedule.h"
#include <string>
#include <vector>

namespace earnings
{

// 当前所处的工作状态
enum WorkPhase
{
    PHASE_REST = 0,     // 今天休息
    PHASE_BEFORE,       // 还没上班
    PHASE_WORKING,      // 工作中（计薪）
    PHASE_BREAK,        // 休息中（午休等空档，不计薪）
    PHASE_AFTER,        // 已下班
};

// 段：一段连续计薪区间，费率固定
struct Segment
{
    int begin = 0;
    int end = 0;
    double rate = 0.0;      // 元/秒
    bool overtime = false;
};

// 当月基准（月薪制需要"当月工时"才能算出秒薪）
struct PayContext
{
    int year = 0;
    int month = 0;
    double month_shift_sec = 0.0;       // 当月排班总秒数
    double month_standard_sec = 0.0;    // 当月标准秒数（仅工作日、按标准工时截断）
    int work_days = 0;                  // 当月出勤天数
};

struct DayResult
{
    bool is_workday = false;
    int wday = 0;
    WorkPhase phase = PHASE_REST;

    double shift_sec = 0.0;        // 当天排班时长
    double worked_sec = 0.0;       // 到今天为止已计薪的工时
    double remain_sec = 0.0;       // 距当天最后一个班次结束还有多久

    double earned = 0.0;           // 今日已赚
    double day_total = 0.0;        // 今日应得（全天）
    double rate_per_sec = 0.0;     // 当前计薪速率（元/秒），休息/下班为 0
    double avg_hourly = 0.0;       // 今日平均时薪

    double ot_sec = 0.0;           // 已发生的加班时长
    double ot_earned = 0.0;        // 加班部分已赚
    double ot_total = 0.0;         // 全天加班时长

    int time_permille = 0;         // 工时进度 0~1000
    int money_permille = 0;        // 到账进度 0~1000
};

// 月度 / 年度累计（不含"今天"，今天单独叠加，避免每秒重算整月）
struct Aggregates
{
    double month_past = 0.0;       // 本月 1 号 ~ 昨天
    double month_total = 0.0;      // 本月整月
    double week_past = 0.0;        // 本周一 ~ 昨天
    double year_past = 0.0;        // 本年 1/1 ~ 昨天
    double year_total = 0.0;       // 本年整年
    double month_shift_sec = 0.0;
    double month_standard_sec = 0.0;
    int month_work_days = 0;
};

const wchar_t* PhaseName(WorkPhase p);

// 构造某个月的计薪基准
PayContext MakeContext(const SalaryConfig& cfg, int year, int month);

// 某天的全天应得；可选输出段表 / 加班总时长 / 基准秒薪
double DayTotal(const SalaryConfig& cfg, const PayContext& ctx, int wday,
                std::vector<Segment>* segs_out = nullptr,
                double* ot_total_out = nullptr,
                double* base_rate_out = nullptr);

// 给定时刻的当日结果
DayResult ComputeDay(const SalaryConfig& cfg, const LocalTime& now);

// 月度 / 周 / 年累计（不含今天）
Aggregates ComputeAggregates(const SalaryConfig& cfg, const LocalTime& now);

} // namespace earnings
