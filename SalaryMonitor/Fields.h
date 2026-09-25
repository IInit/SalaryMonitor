// Fields.h : 插件的显示项定义
//
// 每个显示项 = 一个 key + 名称 + 短标签 + 示例值 + 取值函数。
// 显示项列表由主程序（TrafficMonitor）在"显示设置"里让用户勾选。
#pragma once

#include "Earnings.h"
#include "SalaryConfig.h"
#include <string>
#include <vector>

struct FieldDef
{
    std::string key;
    std::string label;         // 完整名称（选择列表里显示）
    std::string short_label;   // 数值前面的短标签
    std::string sample;        // 示例值（决定显示区域宽度）
};

// 显示项元数据
const std::vector<FieldDef>& fieldDefs();
const FieldDef* findField(const std::string& key);
std::string fieldLabelUtf8(const std::string& key);
std::string fieldShortLabelUtf8(const std::string& key);
std::string fieldSampleUtf8(const std::string& key);

// 一帧完整的数据快照（由插件主类在 DataRequired 里生成）
struct Snapshot
{
    earnings::DayResult day;
    earnings::Aggregates agg;
    LocalTime now;
    int days_to_payday = 0;
    double month_earned = 0.0;   // = agg.month_past + day.earned
    double week_earned = 0.0;    // = agg.week_past + day.earned
    double year_earned = 0.0;    // = agg.year_past + day.earned
    std::string profession_id;
};

// 取值（UTF-8 文本）
std::string fieldValueUtf8(const std::string& key, const Snapshot& s, const SalaryConfig& cfg);

// 自绘项（今日进度条）：插件自己画，需要单独处理
bool isCustomDrawField(const std::string& key);
