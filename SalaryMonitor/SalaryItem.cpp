// SalaryItem.cpp : 插件显示项实现
#include "pch.h"
#include "SalaryItem.h"
#include "SalaryMonitor.h"
#include "Encoding.h"
#include <cstdio>

CSalaryItem::CSalaryItem(CSalaryMonitor* owner, const std::string& key)
    : m_owner(owner), m_key(key), m_id(Utf8ToWString(key))
{
}

const wchar_t* CSalaryItem::GetItemName() const
{
    m_name = Utf8ToWString(m_owner->itemLabelUtf8(m_key));
    return m_name.c_str();
}

const wchar_t* CSalaryItem::GetItemId() const
{
    return m_id.c_str();
}

const wchar_t* CSalaryItem::GetItemLableText() const
{
    m_label = Utf8ToWString(m_owner->itemShortLabelUtf8(m_key));
    return m_label.c_str();
}

const wchar_t* CSalaryItem::GetItemValueText() const
{
    m_value = Utf8ToWString(m_owner->itemValueUtf8(m_key));
    return m_value.c_str();
}

const wchar_t* CSalaryItem::GetItemValueSampleText() const
{
    m_sample = Utf8ToWString(m_owner->itemSampleUtf8(m_key));
    return m_sample.c_str();
}

// ------------------------------------------------------------------ 自绘进度条
// "今日进度条"是唯一自绘项：一条横向进度条 + 百分比文字。
// 主程序调用 DrawItem 时给的是本项目自己的矩形，w / h 可能大于最小宽度。
bool CSalaryItem::IsCustomDraw() const
{
    return m_owner->itemIsCustomDraw(m_key);
}

int CSalaryItem::GetItemWidth() const
{
    // 96 DPI 下的最小宽度（主程序会按系统 DPI 自动放大）
    return IsCustomDraw() ? 84 : 0;
}

int CSalaryItem::GetItemWidthEx(void* hDC) const
{
    if (!IsCustomDraw() || hDC == nullptr)
        return 0;

    HDC dc = (HDC)hDC;
    // 宽度 = 进度条最小宽度 + "100%" 文字宽度 + 间隔
    SIZE sz{ 0, 0 };
    const wchar_t* sample = L"100%";
    ::GetTextExtentPoint32W(dc, sample, (int)::wcslen(sample), &sz);

    int bar = 44;                       // 进度条本体最小宽度
    int gap = 4;
    int total = bar + gap + sz.cx;
    if (total < 64)
        total = 64;
    return total;
}

void CSalaryItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    if (hDC == nullptr || w <= 0 || h <= 0)
        return;

    HDC dc = (HDC)hDC;
    const int permille = m_owner->todayProgressPermille();
    int pct = (permille + 5) / 10;      // 0~100
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;

    // 百分比文字宽度（按主程序传进来的 DC 字体测量，保证与旁边项目一致）
    wchar_t text[16];
    ::swprintf(text, 16, L"%d%%", pct);
    SIZE text_size{ 0, 0 };
    ::GetTextExtentPoint32W(dc, text, (int)::wcslen(text), &text_size);

    const int gap = 4;
    int bar_w = w - text_size.cx - gap;
    if (bar_w < 8)
    {
        // 空间实在不够：退化成只画百分比，绝不越界绘制
        bar_w = 0;
    }

    COLORREF track = dark_mode ? RGB(0x44, 0x48, 0x4C) : RGB(0xD5, 0xD9, 0xDD);
    COLORREF fill = dark_mode ? RGB(0x4E, 0xC9, 0xA0) : RGB(0x1F, 0x9E, 0x76);
    COLORREF text_color = dark_mode ? RGB(0xF2, 0xF6, 0xF8) : RGB(0x22, 0x26, 0x2A);

    int saved = ::SaveDC(dc);

    if (bar_w > 0)
    {
        // 进度条：垂直方向留出一点内边距，看起来像一条独立的条
        int pad = h / 4;
        if (pad < 1)
            pad = 1;
        if (pad * 2 >= h)
            pad = (h - 1) / 2;
        if (pad < 0)
            pad = 0;

        RECT rc_track{ x, y + pad, x + bar_w, y + h - pad };
        RECT rc_fill = rc_track;
        rc_fill.right = rc_fill.left + (bar_w * permille) / 1000;
        if (rc_fill.right < rc_fill.left)
            rc_fill.right = rc_fill.left;

        HBRUSH br_track = ::CreateSolidBrush(track);
        HBRUSH br_fill = ::CreateSolidBrush(fill);
        ::FillRect(dc, &rc_track, br_track);
        if (rc_fill.right > rc_fill.left)
            ::FillRect(dc, &rc_fill, br_fill);
        ::DeleteObject(br_track);
        ::DeleteObject(br_fill);
    }

    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, text_color);
    RECT rc_text{ x + bar_w + (bar_w > 0 ? gap : 0), y, x + w, y + h };
    ::DrawTextW(dc, text, -1, &rc_text, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::RestoreDC(dc, saved);
}
