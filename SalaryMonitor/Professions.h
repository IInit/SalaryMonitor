// Professions.h : 职业预设
//
// 职业 = 一套**默认参数**：计薪方式 + 金额 + 排班。它不是一道枷锁——
// 选中职业后仍然可以逐项改参数（改完 profession 变成 custom 之外的自定义口径），
// 用户的显示偏好（小数位 / 货币符号 / 发薪日）在任何时候都不会被覆盖。
#pragma once

#include "SalaryConfig.h"
#include <string>
#include <vector>

struct ProfessionDef
{
    std::string id;
    const wchar_t* name;
    const wchar_t* summary;   // 一句话说明这个职业的钱怎么算
    int mode;
};

// 全部职业预设（含末尾的「自定义」）
const std::vector<ProfessionDef>& professions();

// 按 id / 下标查找；找不到返回 nullptr
const ProfessionDef* findProfession(const std::string& id);
const ProfessionDef* findProfession(int index);
int professionIndex(const std::string& id);

std::wstring professionName(const std::string& id);
std::wstring professionSummary(const std::string& id);

// 把该职业的默认计薪方式、金额、排班套用到 cfg（保留显示口径与发薪日）
void applyProfession(SalaryConfig& cfg, const std::string& id);
