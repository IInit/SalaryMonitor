// Format.h : 数值文本化（金额 / 时长 / 百分比）
//
// 全部手写实现，不依赖 locale —— printf 的 ' 分组标志受系统区域设置影响，
// 在中文/英文系统上表现不一致，且插件运行在别人机器上，必须结果稳定。
#pragma once

#include "SalaryConfig.h"
#include <string>

namespace fmt
{
// 金额：带货币符号 + 千分位 + 配置的小数位，如 "¥1,234.56"
std::string Money(double v, const SalaryConfig& cfg);
// 金额：带货币符号 + 指定小数位（不受 cfg.decimals 影响，用于秒薪这类极小值）
std::string MoneyEx(double v, const SalaryConfig& cfg, int decimals);
// 纯数字（无货币符号）+ 千分位
std::string Number(double v, int decimals, bool thousands);

// 时长："4h12m" / "12m" / "45s"（用于已工作时长、距下班）
std::string Duration(double seconds);
// 工时小时数："4.2h"
std::string Hours(double seconds);
// 秒表："04:12:33"
std::string Clock(double seconds);
// 百分比："68.4%"
std::string Percent(int permille);

double Round(double v, int decimals);
} // namespace fmt
