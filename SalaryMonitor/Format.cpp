// Format.cpp : 数值文本化实现
#include "Format.h"
#include <cmath>
#include <cstdio>

namespace fmt
{

double Round(double v, int decimals)
{
    if (decimals < 0)
        decimals = 0;
    if (decimals > 8)
        decimals = 8;
    double scale = std::pow(10.0, decimals);
    // 负数四舍五入时保持"远离零"的一致性
    return (v >= 0.0) ? std::floor(v * scale + 0.5) / scale
                      : -std::floor(-v * scale + 0.5) / scale;
}

static std::string GroupThousands(const std::string& digits)
{
    std::string out;
    int count = 0;
    for (int i = (int)digits.size() - 1; i >= 0; --i)
    {
        out.insert(out.begin(), digits[(size_t)i]);
        if (++count % 3 == 0 && i > 0)
            out.insert(out.begin(), ',');
    }
    return out;
}

std::string Number(double v, int decimals, bool thousands)
{
    if (decimals < 0)
        decimals = 0;
    if (decimals > 8)
        decimals = 8;
    if (!(v == v))          // NaN 兜底
        v = 0.0;

    // 先按"四舍五入（远离零）"取整，再交给 snprintf。
    // 直接用 printf 的 %.Nf 会走 round-half-to-even（0.5 -> 0、2.5 -> 2），
    // 与用户对"钱"的直觉不符；金额显示必须可预期。
    v = Round(v, decimals);

    bool neg = v < 0.0;
    double av = neg ? -v : v;

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, av);
    std::string s(buf);

    std::string int_part = s;
    std::string frac_part;
    size_t dot = s.find('.');
    if (dot != std::string::npos)
    {
        int_part = s.substr(0, dot);
        frac_part = s.substr(dot);      // 含小数点
    }
    if (thousands)
        int_part = GroupThousands(int_part);

    std::string out;
    if (neg)
        out += '-';
    out += int_part;
    out += frac_part;
    return out;
}

std::string Money(double v, const SalaryConfig& cfg)
{
    std::string num = Number(v, cfg.decimals, cfg.thousands);
    if (cfg.show_currency)
        return cfg.currency + num;
    return num;
}

std::string MoneyEx(double v, const SalaryConfig& cfg, int decimals)
{
    std::string num = Number(v, decimals, cfg.thousands);
    if (cfg.show_currency)
        return cfg.currency + num;
    return num;
}

std::string Duration(double seconds)
{
    if (!(seconds == seconds) || seconds < 0.0)
        seconds = 0.0;
    long total = (long)(seconds + 0.5);
    long h = total / 3600;
    long m = (total % 3600) / 60;
    long s = total % 60;

    char buf[48];
    if (h > 0)
        std::snprintf(buf, sizeof(buf), "%ldh%02ldm", h, m);
    else if (m > 0)
        std::snprintf(buf, sizeof(buf), "%ldm%02lds", m, s);
    else
        std::snprintf(buf, sizeof(buf), "%lds", s);
    return buf;
}

std::string Hours(double seconds)
{
    if (!(seconds == seconds) || seconds < 0.0)
        seconds = 0.0;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%.2fh", seconds / 3600.0);
    return buf;
}

std::string Clock(double seconds)
{
    if (!(seconds == seconds) || seconds < 0.0)
        seconds = 0.0;
    long total = (long)(seconds + 0.5);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%02ld:%02ld:%02ld", total / 3600, (total % 3600) / 60, total % 60);
    return buf;
}

std::string Percent(int permille)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f%%", permille / 10.0);
    return buf;
}

} // namespace fmt
