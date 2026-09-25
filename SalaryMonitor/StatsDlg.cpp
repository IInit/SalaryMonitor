// StatsDlg.cpp : 收益统计实现
//
// 这里的每个数字都由计薪引擎按当前配置重算（而不是记录历史观测值）：
// 计薪是"时间 -> 钱"的确定函数，所以换职业 / 改工资后整月整年的口径会一起变，
// 永远是自洽的，也不会出现"账本和当前设置对不上"的怪现象。
#include "pch.h"
#include "StatsDlg.h"
#include "Earnings.h"
#include "Format.h"
#include "Encoding.h"
#include <cstdio>

IMPLEMENT_DYNAMIC(CStatsDlg, CDialogEx)

BEGIN_MESSAGE_MAP(CStatsDlg, CDialogEx)
END_MESSAGE_MAP()

static CRect Dlu(HWND hDlg, int l, int t, int r, int b)
{
    CRect rc(l, t, r, b);
    ::MapDialogRect(hDlg, &rc);
    return rc;
}

static int DluW(HWND hDlg, int w)
{
    CRect rc = Dlu(hDlg, 0, 0, w, 0);
    return rc.right - rc.left;
}

static LocalTime nowLocalTime()
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    return sched::MakeLocalTime(st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

// 把某天往前推 n 天
static void shiftDay(int& y, int& m, int& d, int back)
{
    d -= back;
    while (d < 1)
    {
        --m;
        if (m < 1)
        {
            m = 12;
            --y;
        }
        d += sched::DaysInMonth(y, m);
    }
}

CStatsDlg::CStatsDlg(const SalaryConfig& cfg, CWnd* parent)
    : CDialogEx(IDD, parent), m_cfg(cfg)
{
}

CStatsDlg::~CStatsDlg()
{
}

BOOL CStatsDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    HWND h = GetSafeHwnd();

    CRect full = Dlu(h, 0, 0, 300, 252);
    RECT wr = { 0, 0, full.right - full.left, full.bottom - full.top };
    AdjustWindowRectEx(&wr, GetStyle(), FALSE, GetExStyle());
    SetWindowPos(nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
                 SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow();

    m_font.CreatePointFont(90, L"MS Shell Dlg");

    m_lbl_summary.Create(L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                         Dlu(h, 10, 10, 290, 10 + 60), this, IDC_STATS_SUMMARY);
    m_lbl_summary.SetFont(&m_font);

    m_list.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | LVS_REPORT |
                      LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                  Dlu(h, 10, 74, 290, 218), this, IDC_STATS_LIST);
    m_list.SetFont(&m_font);
    m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_list.InsertColumn(0, L"日期", LVCFMT_LEFT, DluW(h, 46));
    m_list.InsertColumn(1, L"星期", LVCFMT_LEFT, DluW(h, 30));
    m_list.InsertColumn(2, L"排班时长", LVCFMT_RIGHT, DluW(h, 52));
    m_list.InsertColumn(3, L"当日收入", LVCFMT_RIGHT, DluW(h, 70));
    m_list.InsertColumn(4, L"状态", LVCFMT_LEFT, DluW(h, 52));

    fillSummary();
    fillList();
    return TRUE;
}

void CStatsDlg::fillSummary()
{
    LocalTime now = nowLocalTime();
    earnings::DayResult today = earnings::ComputeDay(m_cfg, now);
    earnings::Aggregates agg = earnings::ComputeAggregates(m_cfg, now);

    const double month_earned = agg.month_past + today.earned;
    const double week_earned = agg.week_past + today.earned;
    const double year_earned = agg.year_past + today.earned;

    CString text;
    text.Format(
        L"今日：%s / %s　（已工作 %s，平均时薪 %s）\r\n"
        L"本周：%s\r\n"
        L"本月：%s / %s　（%d 个出勤日，共 %.1f 小时）\r\n"
        L"本年：%s / %s　（距发薪日 %d 天）",
        Utf8ToCString(fmt::Money(today.earned, m_cfg)).GetString(),
        Utf8ToCString(fmt::Money(today.day_total, m_cfg)).GetString(),
        Utf8ToCString(fmt::Duration(today.worked_sec)).GetString(),
        Utf8ToCString(fmt::MoneyEx(today.avg_hourly, m_cfg, 2)).GetString(),
        Utf8ToCString(fmt::Money(week_earned, m_cfg)).GetString(),
        Utf8ToCString(fmt::Money(month_earned, m_cfg)).GetString(),
        Utf8ToCString(fmt::Money(agg.month_total, m_cfg)).GetString(),
        agg.month_work_days,
        agg.month_shift_sec / 3600.0,
        Utf8ToCString(fmt::Money(year_earned, m_cfg)).GetString(),
        Utf8ToCString(fmt::Money(agg.year_total, m_cfg)).GetString(),
        sched::DaysToPayday(m_cfg, now));
    m_lbl_summary.SetWindowText(text);
}

void CStatsDlg::fillList()
{
    LocalTime now = nowLocalTime();

    // 从 20 天前到今天，最近的在最下面（和日历习惯一致）
    const int kDays = 21;
    for (int back = kDays - 1; back >= 0; --back)
    {
        int y = now.year, m = now.month, d = now.day;
        shiftDay(y, m, d, back);
        const int wday = sched::WeekdayOf(y, m, d);
        const earnings::PayContext ctx = earnings::MakeContext(m_cfg, y, m);
        const double total = earnings::DayTotal(m_cfg, ctx, wday);

        const bool is_today = (back == 0);

        CString date_text;
        date_text.Format(L"%04d-%02d-%02d", y, m, d);

        CString dur;
        if (total > 0.0)
        {
            const double shift_sec = sched::DayShiftSeconds(m_cfg, wday);
            dur = Utf8ToCString(fmt::Duration(shift_sec));
        }
        else
        {
            dur = L"—";
        }

        double earned = total;
        CString state;
        if (is_today)
        {
            const earnings::DayResult today = earnings::ComputeDay(m_cfg, now);
            earned = today.earned;
            if (!today.is_workday)
                state = L"今天休息";
            else if (today.phase == earnings::PHASE_AFTER)
                state = L"已下班";
            else if (today.phase == earnings::PHASE_WORKING)
                state = L"进行中";
            else
                state = L"未开始";
        }
        else
        {
            state = (total > 0.0) ? L"已完成" : L"休息";
        }

        const int row = m_list.InsertItem(m_list.GetItemCount(), date_text);
        m_list.SetItemText(row, 1, sched::WeekdayName(wday));
        m_list.SetItemText(row, 2, dur);
        m_list.SetItemText(row, 3, Utf8ToCString(fmt::Money(earned, m_cfg)));
        m_list.SetItemText(row, 4, state);
    }
}
