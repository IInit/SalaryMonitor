// SalaryConfig.h : 插件配置（计薪方式、工资参数、排班、显示口径）
//
// 本文件为跨平台纯数据层，不依赖 Windows / MFC，可在 Linux g++ 下直接编译测试。
// 文本一律使用 UTF-8 std::string；在插件接口（wchar_t*）边界再做编码转换。
#pragma once

#include <string>

// ---------------------------------------------------------------- 计薪方式
// 不同职业的"钱怎么算"差别很大，统一抽象成 6 种计薪方式：
//   月薪固定   ：月薪 ÷ 当月排班总时长，工时内匀速进账（公务员 / 教师 / 文职）
//   月薪+加班  ：月薪 ÷ 当月标准工时，超出标准工时的部分按倍率计加班费（程序员 / 医护）
//   时薪       ：按小时单价 × 实际工作小时（家教 / 律师 / 网约车）
//   日薪       ：当日固定日薪按排班时长摊平（日结工 / 建筑工人）
//   计件       ：件单价 × 当日件数，再按排班摊到每一秒（骑手 / 快递 / 计件工人）
//   底薪+提成  ：底薪摊到当月工作日 + 当日业绩 × 提成比例（销售 / 理发师 / 主播）
enum PayMode
{
    PAY_MONTHLY_FIXED = 0,
    PAY_MONTHLY_OT = 1,
    PAY_HOURLY = 2,
    PAY_DAILY = 3,
    PAY_PIECE = 4,
    PAY_COMMISSION = 5,
    PAY_MODE_COUNT = 6,
};

const wchar_t* payModeName(int mode);
const wchar_t* payModeHint(int mode);

// ---------------------------------------------------------------- 一天的排班规则
// 上午 / 下午各一段，分别可以用 开关 + 起止时间 描述。
// 工作日两段恒开；周六 / 周日可以只上半天，因此需要独立的开关。
struct DayRule
{
    bool am_on = true;
    bool pm_on = true;
    std::string am_start = "09:00";
    std::string am_end = "12:00";
    std::string pm_start = "13:30";
    std::string pm_end = "18:00";
};

// 休息日规则（默认周六 / 周日休息）
inline DayRule RestDayRule()
{
    DayRule r;
    r.am_on = false;
    r.pm_on = false;
    return r;
}

struct SalaryConfig
{
    // ---- 职业与计薪方式 ----
    std::string profession = "programmer";   // 职业预设 id，见 Professions.h
    int mode = PAY_MONTHLY_OT;

    // ---- 各计薪方式对应的金额参数（只有当前方式用到的才会参与计算） ----
    double monthly_salary = 25000.0;     // 月薪（月薪固定 / 月薪+加班）
    double hourly_wage = 60.0;           // 时薪（元/小时）
    double daily_wage = 300.0;           // 日薪（元/天）
    double unit_price = 4.0;             // 件单价（元/件、元/单）
    double unit_count = 60.0;            // 当日件数 / 单量
    double commission_rate = 0.05;       // 提成比例（0.05 = 5%）
    double business_amount = 5000.0;     // 当日业绩额
    double base_monthly = 4000.0;        // 提成制的底薪（月）

    // ---- 加班口径 ----
    bool ot_enabled = true;              // 是否计算加班费
    double standard_hours = 8.0;         // 每天标准工时（超过部分算加班）
    double ot_weekday = 1.5;             // 工作日加班倍率
    double ot_weekend = 2.0;             // 周末上班倍率

    // ---- 发薪 ----
    int payday = 10;                     // 发薪日（1~31）

    // ---- 显示口径 ----
    int decimals = 2;                    // 金额小数位（0~4）
    std::string currency = "\xC2\xA5";   // ¥ 的 UTF-8 编码
    bool show_currency = true;           // 金额是否带货币符号
    bool thousands = true;               // 是否用千分位分隔

    // ---- 排班 ----
    DayRule weekday;                  // 周一 ~ 周五
    DayRule sat = RestDayRule();      // 周六（默认休息）
    DayRule sun = RestDayRule();      // 周日（默认休息）

    // ---- 序列化（JSON，UTF-8） ----
    // toJson 输出 JSON 文本；fromJson 加载，未知字段忽略、缺失字段用默认值。
    std::string toJson() const;
    bool fromJson(const std::string& json_text);

    // 文件读写便捷封装（内部走 FileUtil，Windows 使用宽字符路径）
    bool save(const std::string& utf8_path) const;
    static SalaryConfig load(const std::string& utf8_path);
};
