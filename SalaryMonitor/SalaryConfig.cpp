// SalaryConfig.cpp : 配置 JSON 序列化实现（基于 yyjson）
#include "SalaryConfig.h"
#include "FileUtil.h"
#include "yyjson/yyjson.h"
#include <cstdlib>

const wchar_t* payModeName(int mode)
{
    switch (mode)
    {
    case PAY_MONTHLY_FIXED: return L"月薪固定";
    case PAY_MONTHLY_OT:    return L"月薪 + 加班费";
    case PAY_HOURLY:        return L"时薪";
    case PAY_DAILY:         return L"日薪";
    case PAY_PIECE:         return L"计件";
    case PAY_COMMISSION:    return L"底薪 + 提成";
    default:                return L"未知";
    }
}

const wchar_t* payModeHint(int mode)
{
    switch (mode)
    {
    case PAY_MONTHLY_FIXED:
        return L"月薪按当月排班总时长均摊，班内匀速进账；周末班同样按正常倍率计。";
    case PAY_MONTHLY_OT:
        return L"月薪按当月标准工时分摊得到基准时薪；每天超出标准工时的部分按加班倍率计，周末整班按周末倍率计。";
    case PAY_HOURLY:
        return L"按小时单价 × 班内已工作小时累计；开启加班后，超出标准工时的部分同样乘倍率。";
    case PAY_DAILY:
        return L"当日固定日薪，按当天排班时长摊平；开启加班后，超出标准工时的部分另乘倍率（加班越多当天越超日薪）。";
    case PAY_PIECE:
        return L"件单价 × 当日件数得到当日收入，再按排班时长摊到每一秒（计件按当日匀速完成估算）。";
    case PAY_COMMISSION:
        return L"底薪摊到当月工作日 + 当日业绩 × 提成比例，合计按当天排班时长摊平。";
    default:
        return L"";
    }
}

// ---------------------------------------------------------------- JSON 读写
// 数字字段读取：兼容 int / real
static double GetNum(yyjson_val* v)
{
    if (v == nullptr)
        return 0.0;
    if (yyjson_is_real(v))
        return yyjson_get_real(v);
    if (yyjson_is_int(v) || yyjson_is_uint(v))
        return (double)yyjson_get_int(v);
    return 0.0;
}

static int GetInt(yyjson_val* v, int fallback)
{
    if (v == nullptr || (!yyjson_is_int(v) && !yyjson_is_uint(v) && !yyjson_is_real(v)))
        return fallback;
    return (int)GetNum(v);
}

static bool GetBool(yyjson_val* v, bool fallback)
{
    return v && yyjson_is_bool(v) ? yyjson_get_bool(v) : fallback;
}

static std::string GetStr(yyjson_val* v, const std::string& fallback)
{
    return (v && yyjson_is_str(v)) ? std::string(yyjson_get_str(v)) : fallback;
}

static void addDayRule(yyjson_mut_doc* doc, yyjson_mut_val* root, int which, const DayRule& r);
static void readDayRule(yyjson_val* root, int which, DayRule& r);

std::string SalaryConfig::toJson() const
{
    yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val* root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str(doc, root, "profession", profession.c_str());
    yyjson_mut_obj_add_int(doc, root, "mode", mode);

    yyjson_mut_obj_add_real(doc, root, "monthly_salary", monthly_salary);
    yyjson_mut_obj_add_real(doc, root, "hourly_wage", hourly_wage);
    yyjson_mut_obj_add_real(doc, root, "daily_wage", daily_wage);
    yyjson_mut_obj_add_real(doc, root, "unit_price", unit_price);
    yyjson_mut_obj_add_real(doc, root, "unit_count", unit_count);
    yyjson_mut_obj_add_real(doc, root, "commission_rate", commission_rate);
    yyjson_mut_obj_add_real(doc, root, "business_amount", business_amount);
    yyjson_mut_obj_add_real(doc, root, "base_monthly", base_monthly);

    yyjson_mut_obj_add_bool(doc, root, "ot_enabled", ot_enabled);
    yyjson_mut_obj_add_real(doc, root, "standard_hours", standard_hours);
    yyjson_mut_obj_add_real(doc, root, "ot_weekday", ot_weekday);
    yyjson_mut_obj_add_real(doc, root, "ot_weekend", ot_weekend);

    yyjson_mut_obj_add_int(doc, root, "payday", payday);

    yyjson_mut_obj_add_int(doc, root, "decimals", decimals);
    yyjson_mut_obj_add_str(doc, root, "currency", currency.c_str());
    yyjson_mut_obj_add_bool(doc, root, "show_currency", show_currency);
    yyjson_mut_obj_add_bool(doc, root, "thousands", thousands);

    addDayRule(doc, root, 0, weekday);
    addDayRule(doc, root, 1, sat);
    addDayRule(doc, root, 2, sun);

    size_t out_len = 0;
    char* str = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY, &out_len);
    std::string result;
    if (str != nullptr)
    {
        result = str;
        std::free(str);
    }
    yyjson_mut_doc_free(doc);
    return result;
}

bool SalaryConfig::fromJson(const std::string& json_text)
{
    if (json_text.empty())
        return false;
    std::string buf = json_text;   // yyjson 0.4 需要可写缓冲区
    yyjson_doc* doc = yyjson_read_opts(&buf[0], buf.size(), 0, nullptr, nullptr);
    if (doc == nullptr)
        return false;
    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root))
    {
        yyjson_doc_free(doc);
        return false;
    }

    profession = GetStr(yyjson_obj_get(root, "profession"), profession);
    if (auto v = yyjson_obj_get(root, "mode")) mode = GetInt(v, mode);

    if (auto v = yyjson_obj_get(root, "monthly_salary")) monthly_salary = GetNum(v);
    if (auto v = yyjson_obj_get(root, "hourly_wage")) hourly_wage = GetNum(v);
    if (auto v = yyjson_obj_get(root, "daily_wage")) daily_wage = GetNum(v);
    if (auto v = yyjson_obj_get(root, "unit_price")) unit_price = GetNum(v);
    if (auto v = yyjson_obj_get(root, "unit_count")) unit_count = GetNum(v);
    if (auto v = yyjson_obj_get(root, "commission_rate")) commission_rate = GetNum(v);
    if (auto v = yyjson_obj_get(root, "business_amount")) business_amount = GetNum(v);
    if (auto v = yyjson_obj_get(root, "base_monthly")) base_monthly = GetNum(v);

    ot_enabled = GetBool(yyjson_obj_get(root, "ot_enabled"), ot_enabled);
    if (auto v = yyjson_obj_get(root, "standard_hours")) standard_hours = GetNum(v);
    if (auto v = yyjson_obj_get(root, "ot_weekday")) ot_weekday = GetNum(v);
    if (auto v = yyjson_obj_get(root, "ot_weekend")) ot_weekend = GetNum(v);

    if (auto v = yyjson_obj_get(root, "payday")) payday = GetInt(v, payday);

    if (auto v = yyjson_obj_get(root, "decimals")) decimals = GetInt(v, decimals);
    {
        std::string c = GetStr(yyjson_obj_get(root, "currency"), currency);
        if (!c.empty()) currency = c;
    }
    show_currency = GetBool(yyjson_obj_get(root, "show_currency"), show_currency);
    thousands = GetBool(yyjson_obj_get(root, "thousands"), thousands);

    readDayRule(root, 0, weekday);
    readDayRule(root, 1, sat);
    readDayRule(root, 2, sun);

    yyjson_doc_free(doc);
    return true;
}

bool SalaryConfig::save(const std::string& utf8_path) const
{
    return FileUtil::writeFileAtomic(utf8_path, toJson());
}

SalaryConfig SalaryConfig::load(const std::string& utf8_path)
{
    SalaryConfig cfg;
    std::string text;
    if (FileUtil::readFile(utf8_path, text))
        cfg.fromJson(text);
    return cfg;
}

// ---------------------------------------------------------------- DayRule 序列化
// 注意：yyjson 的可变 API（yyjson_mut_obj_add_xxx）**不复制 key 字符串**，
// 只保存指针 —— key 必须是字符串字面量这类长生命周期对象。
// 早期版本用临时 std::string 拼 "weekday_am_on" 这类 key，出函数即析构，
// 结果 JSON 里的 key 变成乱码、整个排班配置读不回来。
// 因此这里按 which 分支写死字面量，绝不做运行时拼接。
static void addDayRule(yyjson_mut_doc* doc, yyjson_mut_val* root, int which, const DayRule& r)
{
    switch (which)
    {
    case 0:
        yyjson_mut_obj_add_bool(doc, root, "weekday_am_on", r.am_on);
        yyjson_mut_obj_add_str(doc, root, "weekday_am_start", r.am_start.c_str());
        yyjson_mut_obj_add_str(doc, root, "weekday_am_end", r.am_end.c_str());
        yyjson_mut_obj_add_bool(doc, root, "weekday_pm_on", r.pm_on);
        yyjson_mut_obj_add_str(doc, root, "weekday_pm_start", r.pm_start.c_str());
        yyjson_mut_obj_add_str(doc, root, "weekday_pm_end", r.pm_end.c_str());
        break;
    case 1:
        yyjson_mut_obj_add_bool(doc, root, "sat_am_on", r.am_on);
        yyjson_mut_obj_add_str(doc, root, "sat_am_start", r.am_start.c_str());
        yyjson_mut_obj_add_str(doc, root, "sat_am_end", r.am_end.c_str());
        yyjson_mut_obj_add_bool(doc, root, "sat_pm_on", r.pm_on);
        yyjson_mut_obj_add_str(doc, root, "sat_pm_start", r.pm_start.c_str());
        yyjson_mut_obj_add_str(doc, root, "sat_pm_end", r.pm_end.c_str());
        break;
    default:
        yyjson_mut_obj_add_bool(doc, root, "sun_am_on", r.am_on);
        yyjson_mut_obj_add_str(doc, root, "sun_am_start", r.am_start.c_str());
        yyjson_mut_obj_add_str(doc, root, "sun_am_end", r.am_end.c_str());
        yyjson_mut_obj_add_bool(doc, root, "sun_pm_on", r.pm_on);
        yyjson_mut_obj_add_str(doc, root, "sun_pm_start", r.pm_start.c_str());
        yyjson_mut_obj_add_str(doc, root, "sun_pm_end", r.pm_end.c_str());
        break;
    }
}

static void readDayRule(yyjson_val* root, int which, DayRule& r)
{
    switch (which)
    {
    case 0:
        r.am_on = GetBool(yyjson_obj_get(root, "weekday_am_on"), r.am_on);
        r.am_start = GetStr(yyjson_obj_get(root, "weekday_am_start"), r.am_start);
        r.am_end = GetStr(yyjson_obj_get(root, "weekday_am_end"), r.am_end);
        r.pm_on = GetBool(yyjson_obj_get(root, "weekday_pm_on"), r.pm_on);
        r.pm_start = GetStr(yyjson_obj_get(root, "weekday_pm_start"), r.pm_start);
        r.pm_end = GetStr(yyjson_obj_get(root, "weekday_pm_end"), r.pm_end);
        break;
    case 1:
        r.am_on = GetBool(yyjson_obj_get(root, "sat_am_on"), r.am_on);
        r.am_start = GetStr(yyjson_obj_get(root, "sat_am_start"), r.am_start);
        r.am_end = GetStr(yyjson_obj_get(root, "sat_am_end"), r.am_end);
        r.pm_on = GetBool(yyjson_obj_get(root, "sat_pm_on"), r.pm_on);
        r.pm_start = GetStr(yyjson_obj_get(root, "sat_pm_start"), r.pm_start);
        r.pm_end = GetStr(yyjson_obj_get(root, "sat_pm_end"), r.pm_end);
        break;
    default:
        r.am_on = GetBool(yyjson_obj_get(root, "sun_am_on"), r.am_on);
        r.am_start = GetStr(yyjson_obj_get(root, "sun_am_start"), r.am_start);
        r.am_end = GetStr(yyjson_obj_get(root, "sun_am_end"), r.am_end);
        r.pm_on = GetBool(yyjson_obj_get(root, "sun_pm_on"), r.pm_on);
        r.pm_start = GetStr(yyjson_obj_get(root, "sun_pm_start"), r.pm_start);
        r.pm_end = GetStr(yyjson_obj_get(root, "sun_pm_end"), r.pm_end);
        break;
    }
}
