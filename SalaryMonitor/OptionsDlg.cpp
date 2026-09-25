// OptionsDlg.cpp : 工资与排班设置实现
//
// 布局约定同本项目的其它对话框：坐标一律用对话框单位(DLU)，客户区尺寸由模板
// DLU 换算（不用固定像素尺寸，否则高 DPI 下模板按钮会被裁掉）；标签列宽由
// 实际字体测量决定，绝不写死 —— 中文标签比英文宽，写死会让右对齐标签从左侧
// 溢出、表现为"字段名整个消失"。
#include "pch.h"
#include "OptionsDlg.h"
#include "DlgLayout.h"
#include "Professions.h"
#include "Earnings.h"
#include "Format.h"
#include "Encoding.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>

IMPLEMENT_DYNAMIC(COptionsDlg, CDialogEx)

BEGIN_MESSAGE_MAP(COptionsDlg, CDialogEx)
    ON_CBN_SELCHANGE(IDC_OPT_PROFESSION, &COptionsDlg::OnProfessionChanged)
    ON_CBN_SELCHANGE(IDC_OPT_MODE, &COptionsDlg::OnModeChanged)
    ON_BN_CLICKED(IDC_OPT_APPLY_PRESET, &COptionsDlg::OnApplyPreset)
    ON_BN_CLICKED(IDC_OPT_OT_ENABLED, &COptionsDlg::OnOtToggle)
    // 周六 / 周日的半天勾选框：范围 1034~1043
    ON_CONTROL_RANGE(BN_CLICKED, IDC_OPT_SAT_AM_ON, IDC_OPT_SUN_PM_ON, &COptionsDlg::OnTimeCheckChanged)
    // 其余勾选框（金额带符号 / 千分位）
    ON_CONTROL_RANGE(BN_CLICKED, IDC_OPT_SHOW_CURRENCY, IDC_OPT_THOUSANDS, &COptionsDlg::OnCheckChanged)
    // 所有输入框改完就重算预览
    ON_CONTROL_RANGE(EN_CHANGE, IDC_OPT_MONTHLY, IDC_OPT_BUSINESS, &COptionsDlg::OnFieldChanged)
    ON_CONTROL_RANGE(EN_CHANGE, IDC_OPT_OT_ENABLED, IDC_OPT_OT_WEEKEND, &COptionsDlg::OnFieldChanged)
    ON_CONTROL_RANGE(EN_CHANGE, IDC_OPT_WD_AM_S, IDC_OPT_SHIFT_NOTE, &COptionsDlg::OnFieldChanged)
    ON_CONTROL_RANGE(EN_CHANGE, IDC_OPT_DECIMALS, IDC_OPT_CURRENCY, &COptionsDlg::OnFieldChanged)
END_MESSAGE_MAP()

// ---------------------------------------------------------------- 布局常量（DLU）
static const int kDlgW = 300;
static const int kDlgH = 362;

// 时间输入对：[上午起, 上午止] / [下午起, 下午止]
static const int kTimeIds[3][2][2] = {
    { { IDC_OPT_WD_AM_S, IDC_OPT_WD_AM_E },   { IDC_OPT_WD_PM_S, IDC_OPT_WD_PM_E } },
    { { IDC_OPT_SAT_AM_S, IDC_OPT_SAT_AM_E }, { IDC_OPT_SAT_PM_S, IDC_OPT_SAT_PM_E } },
    { { IDC_OPT_SUN_AM_S, IDC_OPT_SUN_AM_E }, { IDC_OPT_SUN_PM_S, IDC_OPT_SUN_PM_E } },
};
static const int kDayOnIds[3][2] = {
    { 0, 0 },
    { IDC_OPT_SAT_AM_ON, IDC_OPT_SAT_PM_ON },
    { IDC_OPT_SUN_AM_ON, IDC_OPT_SUN_PM_ON },
};

// ---------------------------------------------------------------- 小工具
static CRect Dlu(HWND hDlg, int l, int t, int r, int b)
{
    CRect rc(l, t, r, b);
    ::MapDialogRect(hDlg, &rc);
    return rc;
}

static CString num(double v, const char* fmt_text)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), fmt_text, v);
    return CString(buf);
}

static double readDouble(CWnd* wnd, double fallback)
{
    if (wnd == nullptr || wnd->GetSafeHwnd() == nullptr)
        return fallback;
    CString s;
    wnd->GetWindowText(s);
    s.Trim();
    if (s.IsEmpty())
        return fallback;
    const wchar_t* begin = s.GetString();
    wchar_t* end = nullptr;
    double v = wcstod(begin, &end);
    if (end == begin)
        return fallback;
    return v;
}

static int readInt(CWnd* wnd, int fallback)
{
    return (int)readDouble(wnd, (double)fallback);
}

static bool readHM(CWnd* wnd, std::string& out)
{
    if (wnd == nullptr || wnd->GetSafeHwnd() == nullptr)
        return false;
    CString s;
    wnd->GetWindowText(s);
    s.Trim();
    CStringA a(s);                       // 时间文本是纯 ASCII，直接转窄字符
    out = a.GetString();
    return sched::IsValidHM(out);
}

static LocalTime nowLocalTime()
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    return sched::MakeLocalTime(st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

// ---------------------------------------------------------------- 构造
COptionsDlg::COptionsDlg(const SalaryConfig& cfg, CWnd* parent)
    : CDialogEx(IDD, parent), m_cfg(cfg)
{
}

COptionsDlg::~COptionsDlg()
{
    for (CStatic* p : m_labels)
        delete p;
    for (CButton* p : m_buttons)
        delete p;
}

// ---------------------------------------------------------------- 控件工厂
// 注意：所有动态控件都由本对话框持有（成员或 m_labels / m_buttons 容器），
// 绝不用局部 CStatic + Detach()。Detach() 会让控件字体丢失（WM_GETFONT 变 NULL），
// 控件回退到系统默认字体渲染，文字比按 m_font 算出来的宽约 23%，右对齐标签会被裁掉。
CStatic* COptionsDlg::addLabel(const CString& text, int x, int y, int w, int h, bool right_align)
{
    CStatic* s = new CStatic();
    s->Create(text, WS_CHILD | WS_VISIBLE | (right_align ? SS_RIGHT : SS_LEFT) | SS_CENTERIMAGE,
              Dlu(GetSafeHwnd(), x, y, x + w, y + h), this);
    s->SetFont(&m_font);
    m_labels.push_back(s);
    if (right_align)
    {
        // 兜底自校正：DLU <-> 像素的换算在不同 DPI/字体下有偏差，
        // 用控件自身的字体再测一次，放不下就向左加宽（右对齐标签是从左侧溢出的）。
        if (dlglayout::MeasureSelfTextWidth(s->GetSafeHwnd()) > 0)
            dlglayout::WidenToFitText(s->GetSafeHwnd());
    }
    return s;
}

CButton* COptionsDlg::addCheck(const CString& text, int x, int y, int w, int nid, bool enabled)
{
    CButton* b = new CButton();
    DWORD style = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX;
    if (enabled && nid > 0)
        style |= WS_TABSTOP;
    b->Create(text, style, Dlu(GetSafeHwnd(), x, y, x + w, y + 13), this, nid);
    b->SetFont(&m_font);
    m_buttons.push_back(b);
    return b;
}

CButton* COptionsDlg::addGroup(const CString& text, int l, int t, int r, int b)
{
    // 组框必须是 CButton：BS_GROUPBOX 是按钮类样式。
    // 用 CStatic 创建会得到 "STATIC" 类窗口，BS_GROUPBOX 的位模式在 STATIC 里
    // 等价于 SS_BLACKFRAME —— 结果只画一个空矩形、标题永远不显示。
    CButton* box = new CButton();
    box->Create(text, WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                Dlu(GetSafeHwnd(), l, t, r, b), this, -1);
    box->SetFont(&m_font);
    m_buttons.push_back(box);
    return box;
}

void COptionsDlg::addEdit(CEdit& e, int x, int y, int w, int nid, bool number_only)
{
    DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
    if (number_only)
        style |= ES_NUMBER;
    e.Create(style, Dlu(GetSafeHwnd(), x, y, x + w, y + 13), this, nid);
    e.SetFont(&m_font);
}

// 两个时间输入框 + 中间的 "~"
void COptionsDlg::addTimePair(int x, int y, int idx_day, int idx_half)
{
    const int w = 26;
    addEdit(m_tm[idx_day][idx_half][0], x, y, w, kTimeIds[idx_day][idx_half][0]);
    addLabel(L"~", x + w + 1, y, 4, 13, false);
    addEdit(m_tm[idx_day][idx_half][1], x + w + 6, y, w, kTimeIds[idx_day][idx_half][1]);
}

// 按控件当前宽度折行，并把控件高度调到刚好放下（多行说明文字用）
void COptionsDlg::wrapLabel(CStatic& lbl, const std::wstring& text, int max_lines)
{
    CRect rc;
    lbl.GetWindowRect(&rc);
    ScreenToClient(&rc);
    const int w = rc.Width();
    if (w <= 0)
        return;

    std::wstring wrapped = dlglayout::WrapToWidth(lbl.GetSafeHwnd(),
                                                  (HFONT)m_font.GetSafeHandle(), text, w, max_lines);
    lbl.SetWindowText(wrapped.c_str());

    int need = dlglayout::MeasureWrappedHeight(lbl.GetSafeHwnd(),
                                               (HFONT)m_font.GetSafeHandle(), wrapped, w);
    if (need > rc.Height())
    {
        // 允许向下长一点（说明区下方预留了空间），但不超过预留范围
        lbl.SetWindowPos(nullptr, 0, 0, rc.Width(), need, SWP_NOMOVE | SWP_NOZORDER);
    }
}

// ---------------------------------------------------------------- 初始化
BOOL COptionsDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    HWND h = GetSafeHwnd();

    // 客户区 = 模板尺寸（IDD_OPTIONS = 300x362 DLU）
    CRect full = Dlu(h, 0, 0, kDlgW, kDlgH);
    RECT wr = { 0, 0, full.right - full.left, full.bottom - full.top };
    AdjustWindowRectEx(&wr, GetStyle(), FALSE, GetExStyle());
    SetWindowPos(nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
                 SWP_NOMOVE | SWP_NOZORDER);
    CenterWindow();

    m_font.CreatePointFont(90, L"MS Shell Dlg");
    m_loading = true;

    // ---- 标签列宽：按实际字体测量，左右两列共用，保证左右对齐、不溢出 ----
    static const wchar_t* kLeftLabels[] = {
        L"职业", L"月薪 (元/月)", L"时薪 (元/小时)", L"日薪 (元/天)", L"计件单价 (元/件)",
        L"工作日倍率", L"发薪日",
    };
    static const wchar_t* kRightLabels[] = {
        L"计薪方式", L"当日件数 (件)", L"底薪 (元/月)", L"提成比例 (%)", L"当日业绩 (元)",
        L"周末倍率", L"标准工时 (小时/天)",
    };
    const int kLabelMin = 64;
    const int kLabelPad = 6;
    int labL = dlglayout::MeasureLabelColumnDlu(h, (HFONT)m_font.GetSafeHandle(),
                                                kLeftLabels, 7, kLabelMin, kLabelPad);
    int labR = dlglayout::MeasureLabelColumnDlu(h, (HFONT)m_font.GetSafeHandle(),
                                                kRightLabels, 7, kLabelMin, kLabelPad);
    if (labL < kLabelMin) labL = kLabelMin;
    if (labR < kLabelMin) labR = kLabelMin;

    const int col1_label_x = 10;
    const int col1_edit_x = col1_label_x + labL + 4;
    const int col1_edit_w = 66;
    const int col2_label_x = col1_edit_x + col1_edit_w + 12;
    const int col2_edit_x = col2_label_x + labR + 4;
    int col2_edit_w = kDlgW - 12 - col2_edit_x;
    if (col2_edit_w < 44) col2_edit_w = 44;

    // =============================================== 职业与计薪方式
    addGroup(L"职业与计薪方式", 5, 4, kDlgW - 5, 96);

    addLabel(L"职业", col1_label_x, 12, labL, 13, true);
    m_cb_profession.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                           Dlu(h, col1_edit_x, 12, col1_edit_x + 110, 12 + 140), this, IDC_OPT_PROFESSION);
    m_cb_profession.SetFont(&m_font);
    for (const ProfessionDef& p : professions())
        m_cb_profession.AddString(p.name);

    m_btn_apply_preset.Create(L"套用该职业默认参数", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                              Dlu(h, col1_edit_x + 116, 12, col1_edit_x + 116 + 86, 12 + 16), this,
                              IDC_OPT_APPLY_PRESET);
    m_btn_apply_preset.SetFont(&m_font);

    addLabel(L"计薪方式", col1_label_x, 31, labL, 13, true);
    m_cb_mode.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                     Dlu(h, col1_edit_x, 31, col1_edit_x + 110, 31 + 140), this, IDC_OPT_MODE);
    m_cb_mode.SetFont(&m_font);
    for (int i = 0; i < PAY_MODE_COUNT; ++i)
        m_cb_mode.AddString(payModeName(i));

    // 说明区：4 行容量，文字按宽度自动折行（中文逐字断行）
    m_lbl_summary.Create(L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                         Dlu(h, 10, 48, kDlgW - 12, 48 + 44), this, IDC_OPT_PROF_SUMMARY);
    m_lbl_summary.SetFont(&m_font);

    // =============================================== 工资参数
    addGroup(L"工资参数（只有当前计薪方式用得到的才会参与计算）", 5, 100, kDlgW - 5, 174);

    const int row_y0 = 108;
    const int row_dy = 15;

    addLabel(L"月薪 (元/月)", col1_label_x, row_y0, labL, 13, true);
    addEdit(m_e_monthly, col1_edit_x, row_y0, col1_edit_w, IDC_OPT_MONTHLY, true);

    addLabel(L"时薪 (元/小时)", col1_label_x, row_y0 + row_dy, labL, 13, true);
    addEdit(m_e_hourly, col1_edit_x, row_y0 + row_dy, col1_edit_w, IDC_OPT_HOURLY, true);

    addLabel(L"日薪 (元/天)", col1_label_x, row_y0 + row_dy * 2, labL, 13, true);
    addEdit(m_e_daily, col1_edit_x, row_y0 + row_dy * 2, col1_edit_w, IDC_OPT_DAILY, true);

    addLabel(L"计件单价 (元/件)", col1_label_x, row_y0 + row_dy * 3, labL, 13, true);
    addEdit(m_e_unit_price, col1_edit_x, row_y0 + row_dy * 3, col1_edit_w, IDC_OPT_UNIT_PRICE, true);

    addLabel(L"当日件数 (件)", col2_label_x, row_y0, labR, 13, true);
    addEdit(m_e_unit_count, col2_edit_x, row_y0, col2_edit_w, IDC_OPT_UNIT_COUNT, true);

    addLabel(L"底薪 (元/月)", col2_label_x, row_y0 + row_dy, labR, 13, true);
    addEdit(m_e_base_monthly, col2_edit_x, row_y0 + row_dy, col2_edit_w, IDC_OPT_BASE_MONTHLY, true);

    addLabel(L"提成比例 (%)", col2_label_x, row_y0 + row_dy * 2, labR, 13, true);
    addEdit(m_e_commission, col2_edit_x, row_y0 + row_dy * 2, col2_edit_w, IDC_OPT_COMMISSION, true);

    addLabel(L"当日业绩 (元)", col2_label_x, row_y0 + row_dy * 3, labR, 13, true);
    addEdit(m_e_business, col2_edit_x, row_y0 + row_dy * 3, col2_edit_w, IDC_OPT_BUSINESS, true);

    // =============================================== 加班口径
    addGroup(L"加班口径", 5, 178, kDlgW - 5, 220);

    m_chk_ot.Create(L"计算加班费", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    Dlu(h, 10, 186, 10 + 62, 186 + 13), this, IDC_OPT_OT_ENABLED);
    m_chk_ot.SetFont(&m_font);

    addLabel(L"标准工时 (小时/天)", col2_label_x, 186, labR, 13, true);
    addEdit(m_e_std_hours, col2_edit_x, 186, col2_edit_w, IDC_OPT_STD_HOURS, true);

    addLabel(L"工作日倍率", col1_label_x, 202, labL, 13, true);
    addEdit(m_e_ot_weekday, col1_edit_x, 202, col1_edit_w, IDC_OPT_OT_WEEKDAY, true);

    addLabel(L"周末倍率", col2_label_x, 202, labR, 13, true);
    addEdit(m_e_ot_weekend, col2_edit_x, 202, col2_edit_w, IDC_OPT_OT_WEEKEND, true);

    // =============================================== 工作排班
    addGroup(L"工作排班", 5, 224, kDlgW - 5, 292);

    // 三行：工作日 / 周六 / 周日；每行 = 上午勾选框 + 上午时间 + 下午勾选框 + 下午时间
    const int pair_x[2] = { 74, 196 };
    const int chk_x = 10;
    const int chk_w = 56;

    for (int d = 0; d < 3; ++d)
    {
        const int y = 234 + d * 15;

        if (d == 0)
        {
            // 工作日恒上班：勾选 + 禁用，既表明"固定上班"，又让三行左边缘对齐
            CButton* fixed_box = addCheck(L"工作日", chk_x, y, chk_w, -1, false);
            fixed_box->SetCheck(BST_CHECKED);
            fixed_box->EnableWindow(FALSE);
        }
        else
        {
            m_tm_on[d][0].Create((d == 1 ? L"周六上午" : L"周日上午"),
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                 Dlu(h, chk_x, y, chk_x + chk_w, y + 13), this, kDayOnIds[d][0]);
            m_tm_on[d][0].SetFont(&m_font);
            m_tm_on[d][1].Create((d == 1 ? L"周六下午" : L"周日下午"),
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                 Dlu(h, 138, y, 138 + chk_w, y + 13), this, kDayOnIds[d][1]);
            m_tm_on[d][1].SetFont(&m_font);
        }

        for (int half = 0; half < 2; ++half)
            addTimePair(pair_x[half], y, d, half);
    }

    // 发薪日 + 说明
    addLabel(L"发薪日", chk_x, 279, 36, 13, true);
    addEdit(m_e_payday, chk_x + 40, 279, 24, IDC_OPT_PAYDAY, true);
    addLabel(L"号", chk_x + 68, 279, 14, 13, false);
    addLabel(L"时间格式 HH:MM；未勾选的半天按休息处理、不计薪。",
             chk_x + 88, 279, kDlgW - 12 - (chk_x + 88), 13, false);

    // =============================================== 显示
    addGroup(L"显示", 5, 296, kDlgW - 5, 332);

    addLabel(L"金额小数位", 10, 304, 46, 13, true);
    addEdit(m_e_decimals, 60, 304, 22, IDC_OPT_DECIMALS, true);

    addLabel(L"货币符号", 90, 304, 38, 13, true);
    addEdit(m_e_currency, 132, 304, 22, IDC_OPT_CURRENCY, false);

    m_chk_show_currency.Create(L"金额带符号", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                               Dlu(h, 162, 304, 162 + 58, 304 + 13), this, IDC_OPT_SHOW_CURRENCY);
    m_chk_show_currency.SetFont(&m_font);

    m_chk_thousands.Create(L"千分位", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                           Dlu(h, 226, 304, 226 + 44, 304 + 13), this, IDC_OPT_THOUSANDS);
    m_chk_thousands.SetFont(&m_font);

    m_lbl_preview.Create(L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                         Dlu(h, 10, 318, kDlgW - 12, 318 + 13), this, IDC_OPT_PREVIEW);
    m_lbl_preview.SetFont(&m_font);

    // ---- 载入当前配置 ----
    loadFormFrom(m_cfg);
    m_loading = false;

    updateSummary();
    updateParamEnable();
    updateShiftEnable();
    updatePreview();
    return TRUE;
}

// ---------------------------------------------------------------- 数据绑定
void COptionsDlg::loadFormFrom(const SalaryConfig& cfg)
{
    m_cfg = cfg;

    m_cb_profession.SetCurSel(professionIndex(cfg.profession));
    m_cb_mode.SetCurSel(cfg.mode >= 0 && cfg.mode < PAY_MODE_COUNT ? cfg.mode : 0);

    m_e_monthly.SetWindowText(num(cfg.monthly_salary, "%.2f"));
    m_e_hourly.SetWindowText(num(cfg.hourly_wage, "%.2f"));
    m_e_daily.SetWindowText(num(cfg.daily_wage, "%.2f"));
    m_e_unit_price.SetWindowText(num(cfg.unit_price, "%.2f"));
    m_e_unit_count.SetWindowText(num(cfg.unit_count, "%.0f"));
    m_e_base_monthly.SetWindowText(num(cfg.base_monthly, "%.2f"));
    m_e_commission.SetWindowText(num(cfg.commission_rate * 100.0, "%.2f"));
    m_e_business.SetWindowText(num(cfg.business_amount, "%.2f"));

    m_chk_ot.SetCheck(cfg.ot_enabled ? BST_CHECKED : BST_UNCHECKED);
    m_e_std_hours.SetWindowText(num(cfg.standard_hours, "%.1f"));
    m_e_ot_weekday.SetWindowText(num(cfg.ot_weekday, "%.2f"));
    m_e_ot_weekend.SetWindowText(num(cfg.ot_weekend, "%.2f"));

    const DayRule* rules[3] = { &cfg.weekday, &cfg.sat, &cfg.sun };
    for (int d = 0; d < 3; ++d)
    {
        for (int half = 0; half < 2; ++half)
        {
            bool on = (half == 0) ? rules[d]->am_on : rules[d]->pm_on;
            if (m_tm_on[d][half].GetSafeHwnd())
                m_tm_on[d][half].SetCheck(on ? BST_CHECKED : BST_UNCHECKED);
        }
        m_tm[d][0][0].SetWindowText(Utf8ToCString(rules[d]->am_start));
        m_tm[d][0][1].SetWindowText(Utf8ToCString(rules[d]->am_end));
        m_tm[d][1][0].SetWindowText(Utf8ToCString(rules[d]->pm_start));
        m_tm[d][1][1].SetWindowText(Utf8ToCString(rules[d]->pm_end));
    }

    m_e_decimals.SetWindowText(num(cfg.decimals, "%.0f"));
    m_e_currency.SetWindowText(Utf8ToCString(cfg.currency));
    m_chk_show_currency.SetCheck(cfg.show_currency ? BST_CHECKED : BST_UNCHECKED);
    m_chk_thousands.SetCheck(cfg.thousands ? BST_CHECKED : BST_UNCHECKED);
    m_e_payday.SetWindowText(num(cfg.payday, "%.0f"));
}

SalaryConfig COptionsDlg::formToConfig() const
{
    SalaryConfig c = m_cfg;   // 以当前配置为底，只覆盖界面上能改的字段

    int sel = m_cb_mode.GetCurSel();
    if (sel >= 0 && sel < PAY_MODE_COUNT)
        c.mode = sel;
    const ProfessionDef* pd = findProfession(m_cb_profession.GetCurSel());
    if (pd != nullptr)
        c.profession = pd->id;

    c.monthly_salary = readDouble(const_cast<CEdit*>(&m_e_monthly), c.monthly_salary);
    c.hourly_wage = readDouble(const_cast<CEdit*>(&m_e_hourly), c.hourly_wage);
    c.daily_wage = readDouble(const_cast<CEdit*>(&m_e_daily), c.daily_wage);
    c.unit_price = readDouble(const_cast<CEdit*>(&m_e_unit_price), c.unit_price);
    c.unit_count = readDouble(const_cast<CEdit*>(&m_e_unit_count), c.unit_count);
    c.base_monthly = readDouble(const_cast<CEdit*>(&m_e_base_monthly), c.base_monthly);
    c.commission_rate = readDouble(const_cast<CEdit*>(&m_e_commission), c.commission_rate * 100.0) / 100.0;
    c.business_amount = readDouble(const_cast<CEdit*>(&m_e_business), c.business_amount);

    c.ot_enabled = const_cast<CButton*>(&m_chk_ot)->GetCheck() == BST_CHECKED;
    c.standard_hours = readDouble(const_cast<CEdit*>(&m_e_std_hours), c.standard_hours);
    c.ot_weekday = readDouble(const_cast<CEdit*>(&m_e_ot_weekday), c.ot_weekday);
    c.ot_weekend = readDouble(const_cast<CEdit*>(&m_e_ot_weekend), c.ot_weekend);

    DayRule* rules[3] = { &c.weekday, &c.sat, &c.sun };
    for (int d = 0; d < 3; ++d)
    {
        std::string t;
        if (readHM(const_cast<CEdit*>(&m_tm[d][0][0]), t)) rules[d]->am_start = t;
        if (readHM(const_cast<CEdit*>(&m_tm[d][0][1]), t)) rules[d]->am_end = t;
        if (readHM(const_cast<CEdit*>(&m_tm[d][1][0]), t)) rules[d]->pm_start = t;
        if (readHM(const_cast<CEdit*>(&m_tm[d][1][1]), t)) rules[d]->pm_end = t;

        if (d > 0)
        {
            rules[d]->am_on = const_cast<CButton*>(&m_tm_on[d][0])->GetCheck() == BST_CHECKED;
            rules[d]->pm_on = const_cast<CButton*>(&m_tm_on[d][1])->GetCheck() == BST_CHECKED;
        }
        else
        {
            rules[d]->am_on = true;
            rules[d]->pm_on = true;
        }
    }

    c.decimals = readInt(const_cast<CEdit*>(&m_e_decimals), c.decimals);
    CString cur;
    const_cast<CEdit*>(&m_e_currency)->GetWindowText(cur);
    cur.Trim();
    if (!cur.IsEmpty())
        c.currency = CStringToUtf8(cur);
    c.show_currency = const_cast<CButton*>(&m_chk_show_currency)->GetCheck() == BST_CHECKED;
    c.thousands = const_cast<CButton*>(&m_chk_thousands)->GetCheck() == BST_CHECKED;
    c.payday = readInt(const_cast<CEdit*>(&m_e_payday), c.payday);
    return c;
}

// ---------------------------------------------------------------- 说明 / 启用态
void COptionsDlg::updateSummary()
{
    SalaryConfig c = formToConfig();
    std::wstring text = professionSummary(c.profession);
    text += L"\r\n计薪方式【" + std::wstring(payModeName(c.mode)) + L"】" + payModeHint(c.mode);
    wrapLabel(m_lbl_summary, text, 4);
}

void COptionsDlg::updateParamEnable()
{
    int mode = m_cb_mode.GetCurSel();
    if (mode < 0)
        mode = 0;

    auto enabling = [](CEdit& e, bool on) { e.EnableWindow(on ? TRUE : FALSE); };

    const bool monthly = (mode == PAY_MONTHLY_FIXED || mode == PAY_MONTHLY_OT);
    enabling(m_e_monthly, monthly);
    enabling(m_e_hourly, mode == PAY_HOURLY);
    enabling(m_e_daily, mode == PAY_DAILY);
    enabling(m_e_unit_price, mode == PAY_PIECE);
    enabling(m_e_unit_count, mode == PAY_PIECE);
    enabling(m_e_base_monthly, mode == PAY_COMMISSION);
    enabling(m_e_commission, mode == PAY_COMMISSION);
    enabling(m_e_business, mode == PAY_COMMISSION);

    // 月薪固定制与工时无关，计件/提成也不按工时算，因此加班费对它们无效
    const bool ot_useful = (mode != PAY_MONTHLY_FIXED && mode != PAY_PIECE && mode != PAY_COMMISSION);
    m_chk_ot.EnableWindow(ot_useful ? TRUE : FALSE);
    const bool ot_on = ot_useful && m_chk_ot.GetCheck() == BST_CHECKED;
    enabling(m_e_std_hours, ot_on);
    enabling(m_e_ot_weekday, ot_on);
    enabling(m_e_ot_weekend, ot_on);
}

void COptionsDlg::updateShiftEnable()
{
    for (int d = 0; d < 3; ++d)
    {
        bool am = true, pm = true;
        if (d > 0)
        {
            am = m_tm_on[d][0].GetCheck() == BST_CHECKED;
            pm = m_tm_on[d][1].GetCheck() == BST_CHECKED;
        }
        m_tm[d][0][0].EnableWindow(am);
        m_tm[d][0][1].EnableWindow(am);
        m_tm[d][1][0].EnableWindow(pm);
        m_tm[d][1][1].EnableWindow(pm);
    }
}

void COptionsDlg::updatePreview()
{
    if (m_loading)
        return;

    SalaryConfig c = formToConfig();
    LocalTime now = nowLocalTime();

    earnings::DayResult d = earnings::ComputeDay(c, now);
    earnings::Aggregates a = earnings::ComputeAggregates(c, now);

    CString text;
    text.Format(L"预览：今日应得 %s · 当前时薪 %s · 本月预计 %s（%d 个出勤日）",
                Utf8ToCString(fmt::Money(d.day_total, c)).GetString(),
                Utf8ToCString(fmt::MoneyEx(d.rate_per_sec * 3600.0, c, 2)).GetString(),
                Utf8ToCString(fmt::Money(a.month_total, c)).GetString(),
                a.month_work_days);
    m_lbl_preview.SetWindowText(text);
}

// ---------------------------------------------------------------- 事件
void COptionsDlg::OnProfessionChanged()
{
    if (m_loading)
        return;
    const ProfessionDef* pd = findProfession(m_cb_profession.GetCurSel());
    if (pd == nullptr)
        return;

    // 选择职业 = 套用该职业的默认参数（显示偏好与发薪日不受影响）
    SalaryConfig c = m_cfg;
    applyProfession(c, pd->id);

    m_loading = true;
    loadFormFrom(c);
    m_loading = false;

    updateSummary();
    updateParamEnable();
    updateShiftEnable();
    updatePreview();
}

void COptionsDlg::OnApplyPreset()
{
    OnProfessionChanged();
}

void COptionsDlg::OnModeChanged()
{
    if (m_loading)
        return;
    updateSummary();
    updateParamEnable();
    updatePreview();
}

void COptionsDlg::OnTimeCheckChanged(UINT nID)
{
    (void)nID;
    if (m_loading)
        return;
    updateShiftEnable();
    updatePreview();
}

void COptionsDlg::OnFieldChanged(UINT nID)
{
    (void)nID;
    if (m_loading)
        return;
    updatePreview();
}

void COptionsDlg::OnCheckChanged(UINT nID)
{
    (void)nID;
    if (m_loading)
        return;
    updatePreview();
}

void COptionsDlg::OnOtToggle()
{
    if (m_loading)
        return;
    updateParamEnable();
    updatePreview();
}

BOOL COptionsDlg::PreTranslateMessage(MSG* pMsg)
{
    // 输入框里按回车不应关闭对话框（避免"改完参数顺手回车 = 直接保存退出"）
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    {
        CWnd* focus = GetFocus();
        if (focus != nullptr && focus->GetSafeHwnd() != nullptr)
        {
            wchar_t cls[32] = { 0 };
            ::GetClassNameW(focus->GetSafeHwnd(), cls, 32);
            if (_wcsicmp(cls, L"Edit") == 0 || _wcsicmp(cls, L"ComboBox") == 0)
                return TRUE;
        }
    }
    return CDialogEx::PreTranslateMessage(pMsg);
}

// ---------------------------------------------------------------- 校验 / 保存
bool COptionsDlg::validate(std::wstring& err)
{
    SalaryConfig c = formToConfig();

    auto bad = [&err](const wchar_t* msg) {
        err = msg;
        return false;
    };

    switch (c.mode)
    {
    case PAY_MONTHLY_FIXED:
    case PAY_MONTHLY_OT:
        if (!(c.monthly_salary > 0.0))
            return bad(L"月薪要大于 0。");
        break;
    case PAY_HOURLY:
        if (!(c.hourly_wage > 0.0))
            return bad(L"时薪要大于 0。");
        break;
    case PAY_DAILY:
        if (!(c.daily_wage > 0.0))
            return bad(L"日薪要大于 0。");
        break;
    case PAY_PIECE:
        if (!(c.unit_price > 0.0))
            return bad(L"计件单价要大于 0。");
        if (c.unit_count <= 0.0)
            return bad(L"当日件数要大于 0。");
        break;
    case PAY_COMMISSION:
        if (c.base_monthly <= 0.0 && c.business_amount * c.commission_rate <= 0.0)
            return bad(L"底薪与提成至少要让其中一项大于 0。");
        if (c.commission_rate < 0.0 || c.commission_rate > 1.0)
            return bad(L"提成比例请填 0~100 之间的百分数。");
        break;
    default:
        break;
    }

    if (c.standard_hours <= 0.0 || c.standard_hours > 24.0)
        return bad(L"标准工时请填 0~24 小时之间。");
    if (c.ot_weekday < 1.0 || c.ot_weekday > 10.0 || c.ot_weekend < 1.0 || c.ot_weekend > 10.0)
        return bad(L"加班倍率请填 1~10 之间（1 表示不加成）。");
    if (c.decimals < 0 || c.decimals > 4)
        return bad(L"金额小数位请填 0~4。");
    if (c.payday < 1 || c.payday > 31)
        return bad(L"发薪日请填 1~31 之间。");

    // 排班时段
    const DayRule* rules[3] = { &c.weekday, &c.sat, &c.sun };
    const wchar_t* names[3] = { L"工作日", L"周六", L"周日" };
    for (int d = 0; d < 3; ++d)
    {
        const DayRule& r = *rules[d];
        if (r.am_on)
        {
            int s = sched::ParseHM(r.am_start, -1);
            int e = sched::ParseHM(r.am_end, -1);
            if (s < 0 || e < 0)
            {
                err = std::wstring(names[d]) + L"上午的时间格式不对，请用 HH:MM（如 09:00）。";
                return false;
            }
            if (e <= s)
            {
                err = std::wstring(names[d]) + L"上午：结束时间要晚于开始时间。";
                return false;
            }
        }
        if (r.pm_on)
        {
            int s = sched::ParseHM(r.pm_start, -1);
            int e = sched::ParseHM(r.pm_end, -1);
            if (s < 0 || e < 0)
            {
                err = std::wstring(names[d]) + L"下午的时间格式不对，请用 HH:MM（如 13:30）。";
                return false;
            }
            if (e <= s)
            {
                err = std::wstring(names[d]) + L"下午：结束时间要晚于开始时间。";
                return false;
            }
        }
        // 上午 / 下午两段允许重叠甚至完全相同：
        // 计薪引擎的 ShiftsOf() 会先排序再合并重叠段，重复计薪在实现上不可能发生。
        // 早期版本在这里硬性报错"下午不能早于上午结束"，结果把"晚间单时段"
        // （如家教 18:00-20:00、主播 20:00-23:00）这类正当排班也挡在门外，
        // 用户套用预设后一按确定就报错——因此不再拦截，交给合并逻辑处理。
    }

    LocalTime now = nowLocalTime();
    if (sched::MonthShiftSeconds(c, now.year, now.month) <= 0.0)
    {
        // 本月工时为 0 有两种可能：① 排班被误删（应拦截）；② 整月休假 /
        // 寒暑假（教师等职业确实如此，收入就该是 0）。所以这里不直接拒绝，
        // 而是把判断权交回用户。
        CString msg;
        msg.Format(L"按当前排班，%d 年 %d 月的工时为 0 小时。\r\n\r\n"
                   L"如果这是整月休假（如寒暑假），保存后收入会显示为 0，这是正常的；\r\n"
                   L"如果是误操作，请点「否」回去检查排班时段。\r\n\r\n确定要这样保存吗？",
                   now.year, now.month);
        if (::MessageBoxW(GetSafeHwnd(), msg, L"本月工时为 0", MB_YESNO | MB_ICONQUESTION) != IDYES)
        {
            err = L"本月工时为 0 —— 请检查排班时段设置。";
            return false;
        }
    }

    return true;
}

void COptionsDlg::OnOK()
{
    std::wstring err;
    if (!validate(err))
    {
        ::MessageBoxW(GetSafeHwnd(), err.c_str(), L"参数有问题", MB_OK | MB_ICONWARNING);
        return;
    }
    m_cfg = formToConfig();
    CDialogEx::OnOK();
}
