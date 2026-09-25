// dialog_probe.cpp : SalaryMonitor 对话框可观测性探针
//
// 目的：把"字段到底有没有被创建 / 有没有显示 / 文字有没有被裁掉 / 保存有没有生效"
// 从猜测变成事实。
// 做法：LoadLibrary 加载 SalaryMonitor.dll，在工作线程上以模态方式打开对话框，
//       主线程抓取该 HWND，枚举全部子控件，检查 类名 / 控件ID / 可见性 / 客户区坐标 / 文字，
//       并做几轮真实的交互（切职业 -> 确定保存 -> 重开回显 -> 统计窗口 -> 不可写路径报错）。
//
// 用法：DlgProbe.exe <SalaryMonitor.dll> [config_dir] [png_out_dir]
#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <gdiplus.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "include/PluginInterface.h"

typedef ITMPlugin* (*PFN_GetInstance)();

// 截图输出目录（命令行第 3 个参数，空则不截图）
static std::wstring g_png_dir;

// ---- 与 resource.h 保持一致 ----
static const int IDD_OPTIONS_ID = 120;
static const int IDD_STATS_ID = 122;

static const int IDC_OPT_PROFESSION_ID = 1000;
static const int IDC_OPT_PROF_SUMMARY_ID = 1001;
static const int IDC_OPT_MODE_ID = 1002;
static const int IDC_OPT_APPLY_PRESET_ID = 1004;
static const int IDC_OPT_MONTHLY_ID = 1010;
static const int IDC_OPT_HOURLY_ID = 1011;
static const int IDC_OPT_DAILY_ID = 1012;
static const int IDC_OPT_UNIT_PRICE_ID = 1013;
static const int IDC_OPT_UNIT_COUNT_ID = 1014;
static const int IDC_OPT_BASE_MONTHLY_ID = 1015;
static const int IDC_OPT_COMMISSION_ID = 1016;
static const int IDC_OPT_BUSINESS_ID = 1017;
static const int IDC_OPT_OT_ENABLED_ID = 1020;
static const int IDC_OPT_STD_HOURS_ID = 1021;
static const int IDC_OPT_WD_AM_S_ID = 1030;
static const int IDC_OPT_SAT_AM_ON_ID = 1034;
static const int IDC_OPT_SUN_PM_E_ID = 1045;
static const int IDC_OPT_PAYDAY_ID = 1046;
static const int IDC_OPT_DECIMALS_ID = 1050;
static const int IDC_OPT_CURRENCY_ID = 1051;
static const int IDC_OPT_SHOW_CURRENCY_ID = 1052;
static const int IDC_OPT_THOUSANDS_ID = 1053;
static const int IDC_OPT_PREVIEW_ID = 1060;
static const int IDC_STATS_SUMMARY_ID = 1200;
static const int IDC_STATS_LIST_ID = 1201;

// 命令索引（与 SalaryMonitor.cpp 一致）
static const int CMD_OPTIONS = 0;
static const int CMD_STATS = 2;

struct CtrlInfo
{
    HWND hwnd = nullptr;
    std::wstring cls;
    int id = 0;
    bool visible = false;
    bool inside = false;
    RECT rc{};
};

static int g_fail = 0;
static void Check(bool ok, const char* what, const char* detail = "")
{
    std::printf("  %s  %s%s%s\n", ok ? "[ OK ]" : "[FAIL]", what,
                ok || *detail == 0 ? "" : " | ", detail);
    if (!ok)
        ++g_fail;
}

static std::string ToUtf8(const std::wstring& w)
{
    if (w.empty())
        return std::string();
    int len = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)len, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], len, nullptr, nullptr);
    return s;
}

static std::wstring WindowText(HWND h)
{
    if (h == nullptr)
        return std::wstring();
    int n = ::GetWindowTextLengthW(h);
    if (n <= 0)
        return std::wstring();
    std::wstring s((size_t)n, L'\0');
    ::GetWindowTextW(h, &s[0], n + 1);
    return s;
}

// ------------------------------------------------------------------ 文字是否放得下
// 用控件自身的字体测量文字的实际渲染尺寸，与控件客户区比较。
// 这是"控件存在、可见、位置正确，但屏幕上看着被截断"这类问题的判定依据。
struct TextFit
{
    int need_w = 0, need_h = 0;
    int have_w = 0, have_h = 0;
    int font_h = 0;
    int dpi = 0;
    bool multiline = false;
    bool ok = true;
};

static bool AllowsWrapping(HWND h)
{
    wchar_t cls[64] = { 0 };
    ::GetClassNameW(h, cls, 64);
    if (::wcscmp(cls, L"Static") != 0)
        return false;
    LONG style = ::GetWindowLongW(h, GWL_STYLE);
    if (style & SS_SIMPLE)
        return false;
    LONG align = style & SS_TYPEMASK;
    if (align == SS_RIGHT || align == SS_CENTER)
        return false;
    return true;
}

static TextFit MeasureTextFit(HWND h)
{
    TextFit f;
    RECT rc{};
    ::GetClientRect(h, &rc);
    f.have_w = rc.right - rc.left;
    f.have_h = rc.bottom - rc.top;

    std::wstring t = WindowText(h);
    if (t.empty())
        return f;

    HDC dc = ::GetDC(h);
    if (dc == nullptr)
        return f;

    HFONT font = (HFONT)::SendMessageW(h, WM_GETFONT, 0, 0);
    HGDIOBJ old = nullptr;
    if (font != nullptr)
        old = ::SelectObject(dc, font);

    LOGFONTW lf{};
    if (font)
        ::GetObjectW(font, sizeof(lf), &lf);
    f.font_h = lf.lfHeight;
    f.dpi = ::GetDeviceCaps(dc, LOGPIXELSY);

    f.multiline = AllowsWrapping(h) && f.have_w > 0;
    if (f.multiline)
    {
        // 多行控件：按控件真实宽度排版，看需要的行高是否放得下
        RECT calc{ 0, 0, f.have_w, 0 };
        ::DrawTextW(dc, t.c_str(), (int)t.size(), &calc, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        f.need_w = calc.right - calc.left;
        f.need_h = calc.bottom - calc.top;
    }
    else
    {
        RECT calc{ 0, 0, 0, 0 };
        ::DrawTextW(dc, t.c_str(), (int)t.size(), &calc, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
        f.need_w = calc.right - calc.left;
        f.need_h = calc.bottom - calc.top;
    }

    if (old != nullptr)
        ::SelectObject(dc, old);
    ::ReleaseDC(h, dc);

    if (f.multiline)
        f.ok = (f.need_h <= f.have_h);
    else
        f.ok = (f.need_w <= f.have_w) && (f.need_h <= f.have_h);
    return f;
}

static BOOL CALLBACK FindDlgProc(HWND h, LPARAM lp)
{
    DWORD pid = 0;
    ::GetWindowThreadProcessId(h, &pid);
    if (pid != ::GetCurrentProcessId())
        return TRUE;
    wchar_t cls[64] = { 0 };
    ::GetClassNameW(h, cls, 64);
    if (::wcscmp(cls, L"#32770") != 0)
        return TRUE;
    *reinterpret_cast<HWND*>(lp) = h;
    return FALSE;
}

static HWND FindOurDialog()
{
    HWND found = nullptr;
    ::EnumWindows(FindDlgProc, (LPARAM)&found);
    return found;
}

struct TopWnd { HWND h; std::wstring cls; std::wstring title; };
static std::vector<TopWnd>* g_top = nullptr;

static BOOL CALLBACK EnumTopProc(HWND h, LPARAM)
{
    DWORD pid = 0;
    ::GetWindowThreadProcessId(h, &pid);
    if (pid != ::GetCurrentProcessId() || g_top == nullptr)
        return TRUE;
    TopWnd t;
    t.h = h;
    wchar_t cls[64] = { 0 };
    ::GetClassNameW(h, cls, 64);
    t.cls = cls;
    t.title = WindowText(h);
    g_top->push_back(t);
    return TRUE;
}

static std::vector<TopWnd> TopWindows()
{
    std::vector<TopWnd> v;
    g_top = &v;
    ::EnumWindows(EnumTopProc, 0);
    g_top = nullptr;
    return v;
}

// 关掉不属于 keep 的 #32770（即弹出的模态提示框），返回处理个数
static int DismissStrayMessageBoxes(HWND keep)
{
    int n = 0;
    for (const TopWnd& t : TopWindows())
    {
        if (t.cls != L"#32770" || t.h == keep)
            continue;
        ::PostMessageW(t.h, WM_COMMAND, IDOK, 0);
        ::PostMessageW(t.h, WM_CLOSE, 0, 0);
        ++n;
    }
    return n;
}

static std::vector<CtrlInfo> EnumChildren(HWND dlg)
{
    RECT cr{};
    ::GetClientRect(dlg, &cr);
    std::vector<CtrlInfo> out;
    for (HWND h = ::GetWindow(dlg, GW_CHILD); h != nullptr; h = ::GetWindow(h, GW_HWNDNEXT))
    {
        CtrlInfo c;
        c.hwnd = h;
        wchar_t cls[64] = { 0 };
        ::GetClassNameW(h, cls, 64);
        c.cls = cls;
        c.id = ::GetDlgCtrlID(h);
        c.visible = ::IsWindowVisible(h) != FALSE;
        RECT r{};
        ::GetWindowRect(h, &r);
        POINT pt{ r.left, r.top };
        ::ScreenToClient(dlg, &pt);
        c.rc = { pt.x, pt.y, pt.x + (r.right - r.left), pt.y + (r.bottom - r.top) };
        c.inside = c.rc.left >= 0 && c.rc.top >= 0 &&
                   c.rc.right <= (cr.right - cr.left) && c.rc.bottom <= (cr.bottom - cr.top);
        out.push_back(c);
    }
    return out;
}

static bool SaveDialogPng(HWND dlg, const wchar_t* path)
{
    RECT rc{};
    if (!::GetWindowRect(dlg, &rc))
        return false;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
        return false;

    ::RedrawWindow(dlg, nullptr, nullptr,
                   RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME);
    ::UpdateWindow(dlg);
    ::Sleep(400);

    HDC screen = ::GetDC(nullptr);
    HDC mem = ::CreateCompatibleDC(screen);
    HBITMAP bmp = ::CreateCompatibleBitmap(screen, w, h);
    HGDIOBJ old = ::SelectObject(mem, bmp);

    ::SendMessageW(dlg, WM_PRINT, (WPARAM)mem,
                   PRF_CLIENT | PRF_NONCLIENT | PRF_CHILDREN | PRF_ERASEBKGND);

    Gdiplus::Bitmap out(bmp, nullptr);
    CLSID clsid;
    bool ok = false;
    if (::CLSIDFromString(L"{557cf406-1a04-11d3-9a73-0000f81ef32e}", &clsid) == S_OK)
        ok = (out.Save(path, &clsid, nullptr) == Gdiplus::Ok);

    ::SelectObject(mem, old);
    ::DeleteObject(bmp);
    ::DeleteDC(mem);
    ::ReleaseDC(nullptr, screen);
    return ok;
}

static void DumpChildren(HWND dlg, const char* title)
{
    std::vector<CtrlInfo> kids = EnumChildren(dlg);
    RECT cr{};
    ::GetClientRect(dlg, &cr);
    std::printf("\n---- %s：子控件 %d 个（客户区 %ldx%ld）----\n",
                title, (int)kids.size(), (long)(cr.right - cr.left), (long)(cr.bottom - cr.top));
    int vis = 0, inarea = 0, overflow = 0;
    for (const CtrlInfo& c : kids)
    {
        std::wstring t = WindowText(c.hwnd);
        std::wstring shown = t;
        if (shown.size() > 46)
            shown = shown.substr(0, 46) + L"…";
        for (wchar_t& ch : shown)
            if (ch == L'\n' || ch == L'\r')
                ch = L'|';
        if (c.visible) ++vis;
        if (c.visible && c.inside) ++inarea;

        TextFit fit = MeasureTextFit(c.hwnd);
        char fitNote[160] = "";
        const char* fitFlag = "   ";
        if (!fit.ok && !t.empty())
        {
            ++overflow;
            fitFlag = "TXT";
            std::snprintf(fitNote, sizeof(fitNote),
                          "  [文字溢出 need=%dx%d have=%dx%d%s fontH=%d dpi=%d]",
                          fit.need_w, fit.need_h, fit.have_w, fit.have_h,
                          fit.multiline ? " ML" : "", fit.font_h, fit.dpi);
        }

        std::printf("  id=%-5d %-12s vis=%d rect=(%4ld,%4ld,%4ld,%4ld) %s %s fontH=%-3d text=%s%s\n",
                    c.id, ToUtf8(c.cls).c_str(), c.visible ? 1 : 0,
                    (long)c.rc.left, (long)c.rc.top, (long)c.rc.right, (long)c.rc.bottom,
                    c.inside ? "  " : "OUT", fitFlag, fit.font_h, ToUtf8(shown).c_str(), fitNote);
    }
    std::printf("  可见 %d / 总 %d ；可见且在客户区内 %d ；文字放不下 %d\n",
                vis, (int)kids.size(), inarea, overflow);
    Check(vis == (int)kids.size(), "所有子控件都可见");
    Check(inarea == (int)kids.size(), "所有子控件都落在客户区内（没有被裁剪）");
    Check(overflow == 0, "所有控件的文字都放得下（未被截断）");
}

static bool CheckCtrl(HWND dlg, int id, const char* name, bool require_text)
{
    HWND h = ::GetDlgItem(dlg, id);
    if (h == nullptr)
    {
        Check(false, name, "控件不存在");
        return false;
    }
    std::vector<CtrlInfo> all = EnumChildren(dlg);
    bool inside = false;
    for (const CtrlInfo& c : all)
        if (c.hwnd == h)
            inside = c.inside;
    std::wstring t = WindowText(h);
    if (t.size() > 30)
        t = t.substr(0, 30) + L"…";
    bool visible = ::IsWindowVisible(h) != FALSE;
    bool text_ok = !require_text || !t.empty();
    char detail[256];
    std::snprintf(detail, sizeof(detail), "visible=%d inside=%d text=\"%s\"",
                  visible ? 1 : 0, inside ? 1 : 0, ToUtf8(t).c_str());
    Check(visible && inside && text_ok, name, detail);
    return visible && inside;
}

static ITMPlugin::OptionReturn g_opt_ret = ITMPlugin::OR_OPTION_NOT_PROVIDED;
static DWORD g_opt_err = 0;
static bool g_opt_called = false;

// 在工作线程上打开对话框（模态），主线程等待窗口出现
static HWND OpenDialog(ITMPlugin* plugin, std::thread& worker, int cmd, int timeout_ms)
{
    worker = std::thread([plugin, cmd]() {
        if (cmd >= 0)
        {
            plugin->OnPluginCommand(cmd, nullptr, nullptr);
        }
        else
        {
            g_opt_called = true;
            g_opt_ret = plugin->ShowOptionsDialog(nullptr);
            g_opt_err = ::GetLastError();
        }
    });

    HWND dlg = nullptr;
    for (int waited = 0; waited < timeout_ms && dlg == nullptr; waited += 50)
    {
        dlg = FindOurDialog();
        if (dlg == nullptr)
            ::Sleep(50);
    }
    if (dlg == nullptr && cmd < 0)
        std::printf("  [INFO] ShowOptionsDialog 已调用=%d 返回=%d GetLastError=%lu\n",
                    g_opt_called ? 1 : 0, (int)g_opt_ret, g_opt_err);
    return dlg;
}

static bool CloseDialog(HWND dlg, std::thread& worker, int cmd, const char* tag)
{
    ::PostMessageW(dlg, WM_COMMAND, (WPARAM)cmd, 0);
    for (int i = 0; i < 100 && ::IsWindow(dlg); ++i)
    {
        DismissStrayMessageBoxes(dlg);
        ::Sleep(50);
    }
    if (::IsWindow(dlg))
    {
        std::printf("  [WARN] %s：对话框 5s 内未关闭（可能被模态提示框挡住）\n", tag);
        Check(false, tag, "对话框未能关闭");
        std::printf("\n==== FAIL：探针提前退出 ====\n");
        std::exit(3);
    }
    if (worker.joinable())
        worker.join();
    return true;
}

// 通知父窗口"下拉框选中项变了"（CB_SETCURSEL 不会触发 CBN_SELCHANGE）
static void NotifyComboSelChange(HWND dlg, int combo_id)
{
    HWND combo = ::GetDlgItem(dlg, combo_id);
    if (combo == nullptr)
        return;
    LRESULT sel = ::SendMessageW(combo, CB_GETCURSEL, 0, 0);
    ::SendMessageW(dlg, WM_COMMAND, MAKEWPARAM(combo_id, CBN_SELCHANGE), (LPARAM)combo);
    (void)sel;
}

static std::string ReadAll(const std::wstring& path)
{
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path.c_str(), L"rb") != 0 || fp == nullptr)
        return std::string();
    std::string s;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0)
        s.append(buf, n);
    std::fclose(fp);
    return s;
}

static bool Contains(const std::string& hay, const char* needle)
{
    return hay.find(needle) != std::string::npos;
}

int wmain(int argc, wchar_t** argv)
{
    ::SetConsoleOutputCP(65001);
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 2)
    {
        std::printf("usage: DlgProbe.exe <SalaryMonitor.dll> [config_dir] [png_out_dir]\n");
        return 2;
    }
    const wchar_t* dll_path = argv[1];
    std::wstring cfg_dir = argc >= 3 ? argv[2] : L"";
    g_png_dir = argc >= 4 ? argv[3] : L"";
    if (cfg_dir.empty())
    {
        cfg_dir = dll_path;
        size_t pos = cfg_dir.find_last_of(L"\\/");
        cfg_dir = (pos == std::wstring::npos ? std::wstring(L".") : cfg_dir.substr(0, pos)) + L"\\_probecfg";
    }
    std::wstring cfg_dir_deep = cfg_dir + L"\\nested\\dir";   // 故意用不存在的目录测兜底创建
    ::CreateDirectoryW(cfg_dir.c_str(), nullptr);
    std::wstring cfg_file = cfg_dir + L"\\SalaryMonitor_config.json";
    ::DeleteFileW(cfg_file.c_str());

    std::printf("DLL: %s\nCFG: %s\n\n", ToUtf8(dll_path).c_str(), ToUtf8(cfg_dir).c_str());

    Gdiplus::GdiplusStartupInput gdi_in;
    ULONG_PTR gdi_token = 0;
    if (!g_png_dir.empty())
        Gdiplus::GdiplusStartup(&gdi_token, &gdi_in, nullptr);

    HMODULE mod = ::LoadLibraryW(dll_path);
    if (mod == nullptr)
    {
        std::printf("[FAIL] LoadLibrary failed, err=%lu\n", ::GetLastError());
        return 1;
    }
    PFN_GetInstance get_instance = (PFN_GetInstance)::GetProcAddress(mod, "TMPluginGetInstance");
    if (get_instance == nullptr)
    {
        std::printf("[FAIL] TMPluginGetInstance not found\n");
        return 1;
    }
    ITMPlugin* plugin = get_instance();
    plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, cfg_dir.c_str());
    plugin->OnInitialize(nullptr);
    std::printf("[ OK ] plugin initialized\n");

    // ---- 模板资源齐备性（DoModal 找不到模板会直接返回 -1，表现为"窗口根本不出现"） ----
    std::printf("\n---- RT_DIALOG 模板资源检查 ----\n");
    {
        HRSRC r1 = ::FindResourceW(mod, MAKEINTRESOURCEW(IDD_OPTIONS_ID), RT_DIALOG);
        HRSRC r2 = ::FindResourceW(mod, MAKEINTRESOURCEW(IDD_STATS_ID), RT_DIALOG);
        std::printf("  #%-4d %s\n", IDD_OPTIONS_ID, r1 != nullptr ? "存在" : "缺失");
        std::printf("  #%-4d %s\n", IDD_STATS_ID, r2 != nullptr ? "存在" : "缺失");
        Check(r1 != nullptr, "资源 IDD_OPTIONS 存在");
        Check(r2 != nullptr, "资源 IDD_STATS 存在");
    }

    // ============================================ Phase 1 主设置对话框渲染
    std::printf("\n============ Phase 1：工资与排班设置 —— 控件渲染 ============\n");
    std::thread worker;
    HWND dlg = OpenDialog(plugin, worker, CMD_OPTIONS, 8000);
    Check(dlg != nullptr, "设置对话框已创建");
    if (dlg == nullptr)
    {
        if (worker.joinable())
            worker.detach();
        return 1;
    }
    Check(WindowText(dlg) == L"工资与排班设置", "标题 == 工资与排班设置（确认加载的是本 DLL 的模板）");
    ::Sleep(400);
    DumpChildren(dlg, "工资与排班设置 控件树");
    if (!g_png_dir.empty())
    {
        std::wstring p = g_png_dir + L"\\dialog_options.png";
        std::printf("  截图：%s %s\n", ToUtf8(p).c_str(),
                    SaveDialogPng(dlg, p.c_str()) ? "已保存" : "失败");
    }

    std::printf("\n---- 字段级检查 ----\n");
    CheckCtrl(dlg, IDOK, "「确定」按钮可见且未被裁剪", false);
    CheckCtrl(dlg, IDCANCEL, "「取消」按钮可见且未被裁剪", false);
    CheckCtrl(dlg, IDC_OPT_PROFESSION_ID, "职业下拉：有选中项", true);
    CheckCtrl(dlg, IDC_OPT_MODE_ID, "计薪方式下拉：有选中项", true);
    CheckCtrl(dlg, IDC_OPT_PROF_SUMMARY_ID, "职业说明：已创建且非空", true);
    CheckCtrl(dlg, IDC_OPT_APPLY_PRESET_ID, "「套用该职业默认参数」按钮可见", true);
    CheckCtrl(dlg, IDC_OPT_MONTHLY_ID, "月薪：有值", true);
    CheckCtrl(dlg, IDC_OPT_HOURLY_ID, "时薪：有值", true);
    CheckCtrl(dlg, IDC_OPT_DAILY_ID, "日薪：有值", true);
    CheckCtrl(dlg, IDC_OPT_UNIT_PRICE_ID, "计件单价：有值", true);
    CheckCtrl(dlg, IDC_OPT_UNIT_COUNT_ID, "当日件数：有值", true);
    CheckCtrl(dlg, IDC_OPT_BASE_MONTHLY_ID, "底薪：有值", true);
    CheckCtrl(dlg, IDC_OPT_COMMISSION_ID, "提成比例：有值", true);
    CheckCtrl(dlg, IDC_OPT_BUSINESS_ID, "当日业绩：有值", true);
    CheckCtrl(dlg, IDC_OPT_OT_ENABLED_ID, "加班费勾选框可见", false);
    CheckCtrl(dlg, IDC_OPT_STD_HOURS_ID, "标准工时：有值", true);
    CheckCtrl(dlg, IDC_OPT_WD_AM_S_ID, "工作日上班时间：有值", true);
    CheckCtrl(dlg, IDC_OPT_SAT_AM_ON_ID, "周六上午勾选框可见", false);
    CheckCtrl(dlg, IDC_OPT_SUN_PM_E_ID, "周日下午下班时间：有值", true);
    CheckCtrl(dlg, IDC_OPT_PAYDAY_ID, "发薪日：有值", true);
    CheckCtrl(dlg, IDC_OPT_DECIMALS_ID, "金额小数位：有值", true);
    CheckCtrl(dlg, IDC_OPT_CURRENCY_ID, "货币符号：有值", true);
    CheckCtrl(dlg, IDC_OPT_SHOW_CURRENCY_ID, "「金额带符号」勾选框可见", false);
    CheckCtrl(dlg, IDC_OPT_THOUSANDS_ID, "「千分位」勾选框可见", false);
    CheckCtrl(dlg, IDC_OPT_PREVIEW_ID, "实时预览行：已创建且非空", true);

    // 每段上班时间都必须是合法 HH:MM（1030~1045 里夹着 4 个勾选框，它们的文本为空，跳过）
    {
        int bad = 0;
        for (int id = IDC_OPT_WD_AM_S_ID; id <= IDC_OPT_SUN_PM_E_ID; ++id)
        {
            HWND c = ::GetDlgItem(dlg, id);
            if (c == nullptr)
                continue;
            wchar_t cls[32] = { 0 };
            ::GetClassNameW(c, cls, 32);
            if (::wcscmp(cls, L"Edit") != 0)
                continue;                       // 勾选框（周六上午 / 周日下午）不是时间输入框
            std::wstring t = WindowText(c);
            if (t.empty())
                continue;
            if (t.size() != 5 || t[2] != L':')
            {
                ++bad;
                std::printf("     时间格式异常 id=%d text=%s\n", id, ToUtf8(t).c_str());
            }
        }
        Check(bad == 0, "所有上班时间都是 HH:MM 形式");
    }

    // ============================================ Phase 2 切换职业
    std::printf("\n============ Phase 2：切换职业 -> 参数自动变成该职业的默认值 ============\n");
    {
        std::wstring before = WindowText(::GetDlgItem(dlg, IDC_OPT_MONTHLY_ID));
        // 组合框里第 3 项是「教师」（月薪 9000，月薪固定制）
        HWND combo = ::GetDlgItem(dlg, IDC_OPT_PROFESSION_ID);
        int count = (int)::SendMessageW(combo, CB_GETCOUNT, 0, 0);
        std::printf("  职业下拉项数 = %d\n", count);
        Check(count >= 10, "职业预设数量 >= 10");

        ::SendMessageW(combo, CB_SETCURSEL, 2, 0);
        NotifyComboSelChange(dlg, IDC_OPT_PROFESSION_ID);
        ::Sleep(300);

        std::wstring after = WindowText(::GetDlgItem(dlg, IDC_OPT_MONTHLY_ID));
        std::wstring mode = WindowText(::GetDlgItem(dlg, IDC_OPT_MODE_ID));
        std::wstring summary = WindowText(::GetDlgItem(dlg, IDC_OPT_PROF_SUMMARY_ID));
        std::printf("  月薪：%s -> %s ；计薪方式=%s\n",
                    ToUtf8(before).c_str(), ToUtf8(after).c_str(), ToUtf8(mode).c_str());
        Check(after == L"9000.00", "切到「教师」后月薪变为预设值 9000.00");
        Check(mode == L"月薪固定", "切到「教师」后计薪方式变为「月薪固定」");
        Check(Contains(ToUtf8(summary), "月薪"), "职业说明文字已更新");

        TextFit fit = MeasureTextFit(::GetDlgItem(dlg, IDC_OPT_PROF_SUMMARY_ID));
        Check(fit.ok, "切换职业后说明文字仍然放得下");
    }

    // ============================================ Phase 3 「确定」保存
    std::printf("\n============ Phase 3：「确定」保存并落盘 ============\n");
    {
        ::SetDlgItemTextW(dlg, IDC_OPT_MONTHLY_ID, L"8888");
        ::SetDlgItemTextW(dlg, IDC_OPT_PAYDAY_ID, L"15");
        ::SetDlgItemTextW(dlg, IDC_OPT_CURRENCY_ID, L"$");
        ::Sleep(200);
        CloseDialog(dlg, worker, IDOK, "Phase 3 确定按钮");
        ::Sleep(300);
        std::string json = ReadAll(cfg_file);
        std::printf("  配置文件大小 = %d 字节\n", (int)json.size());
        Check(Contains(json, "8888"), "「确定」已把月薪 8888 写入磁盘");
        Check(Contains(json, "\"payday\": 15"), "「确定」已把发薪日 15 写入磁盘");
        Check(Contains(json, "\"currency\": \"$\""), "「确定」已把货币符号 $ 写入磁盘");
        Check(Contains(json, "\"profession\": \"teacher\""), "职业已保存为 teacher");
    }

    // ============================================ Phase 4 重开回显
    std::printf("\n============ Phase 4：重新打开，检查回显 ============\n");
    {
        std::thread w2;
        HWND d2 = OpenDialog(plugin, w2, CMD_OPTIONS, 8000);
        Check(d2 != nullptr, "对话框可再次打开");
        if (d2 != nullptr)
        {
            ::Sleep(400);
            std::wstring monthly = WindowText(::GetDlgItem(d2, IDC_OPT_MONTHLY_ID));
            std::wstring payday = WindowText(::GetDlgItem(d2, IDC_OPT_PAYDAY_ID));
            std::wstring currency = WindowText(::GetDlgItem(d2, IDC_OPT_CURRENCY_ID));
            int sel = (int)::SendMessageW(::GetDlgItem(d2, IDC_OPT_PROFESSION_ID), CB_GETCURSEL, 0, 0);
            std::printf("  月薪=%s 发薪日=%s 货币=%s 职业下标=%d\n",
                        ToUtf8(monthly).c_str(), ToUtf8(payday).c_str(),
                        ToUtf8(currency).c_str(), sel);
            Check(monthly == L"8888.00", "月薪回显 == 8888.00");
            Check(payday == L"15", "发薪日回显 == 15");
            Check(currency == L"$", "货币符号回显 == $");
            Check(sel == 2, "职业下拉回显到「教师」");
            CloseDialog(d2, w2, IDCANCEL, "Phase 4 取消按钮");
        }
    }

    // ============================================ Phase 5 收益统计对话框
    std::printf("\n============ Phase 5：收益统计对话框 ============\n");
    {
        std::thread w3;
        HWND d3 = OpenDialog(plugin, w3, CMD_STATS, 8000);
        Check(d3 != nullptr, "收益统计对话框已创建");
        if (d3 != nullptr)
        {
            ::Sleep(400);
            Check(WindowText(d3) == L"收益统计", "标题 == 收益统计");
            DumpChildren(d3, "收益统计 控件树");
            if (!g_png_dir.empty())
            {
                std::wstring p = g_png_dir + L"\\dialog_stats.png";
                std::printf("  截图：%s %s\n", ToUtf8(p).c_str(),
                            SaveDialogPng(d3, p.c_str()) ? "已保存" : "失败");
            }
            CheckCtrl(d3, IDOK, "「关闭」按钮可见且未被裁剪", false);
            CheckCtrl(d3, IDC_STATS_SUMMARY_ID, "汇总区非空", true);
            HWND list = ::GetDlgItem(d3, IDC_STATS_LIST_ID);
            Check(list != nullptr, "明细列表已创建");
            if (list != nullptr)
            {
                int rows = (int)::SendMessageW(list, LVM_GETITEMCOUNT, 0, 0);
                std::printf("  明细行数 = %d\n", rows);
                Check(rows == 21, "明细列表有 21 行（最近三周）");
            }
            CloseDialog(d3, w3, IDOK, "Phase 5 关闭按钮");
        }
    }

    // ============================================ Phase 6 多级目录兜底
    std::printf("\n============ Phase 6：配置目录不存在时自动补全上级目录 ============\n");
    {
        plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, cfg_dir_deep.c_str());
        ::RemoveDirectoryW((cfg_dir + L"\\nested\\dir").c_str());
        ::RemoveDirectoryW((cfg_dir + L"\\nested").c_str());

        std::thread w4;
        HWND d4 = OpenDialog(plugin, w4, CMD_OPTIONS, 8000);
        Check(d4 != nullptr, "对话框已打开");
        if (d4 != nullptr)
        {
            ::Sleep(300);
            ::SetDlgItemTextW(d4, IDC_OPT_MONTHLY_ID, L"7777");
            CloseDialog(d4, w4, IDOK, "Phase 6 确定按钮");
            ::Sleep(400);
            std::string deep = ReadAll(cfg_dir_deep + L"\\SalaryMonitor_config.json");
            Check(Contains(deep, "7777"), "深层目录下也能写出配置（自动补全上级目录）");
        }
    }

    // ============================================ Phase 7 不可写路径必须明确报错
    std::printf("\n============ Phase 7：不可写路径必须明确报错，不能静默 ============\n");
    {
        std::wstring bad_dir = std::wstring(dll_path) + L"\\sub";   // 在"文件"下面建目录，必然失败
        plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, bad_dir.c_str());

        std::thread w5;
        HWND d5 = OpenDialog(plugin, w5, CMD_OPTIONS, 8000);
        Check(d5 != nullptr, "对话框已打开");
        if (d5 != nullptr)
        {
            ::Sleep(300);
            int dismissed = 0;
            ::PostMessageW(d5, WM_COMMAND, IDOK, 0);   // 确定后保存失败 -> 应弹出错误提示框
            for (int i = 0; i < 60; ++i)
            {
                dismissed += DismissStrayMessageBoxes(d5);
                ::Sleep(50);
            }
            std::printf("  捕获并关闭的错误提示框数量 = %d\n", dismissed);
            Check(dismissed > 0, "写盘失败时弹出了可见的错误提示框（没有静默失败）");
            if (::IsWindow(d5))
                CloseDialog(d5, w5, IDCANCEL, "Phase 7 取消按钮");
            else if (w5.joinable())
                w5.join();
        }
    }

    ::FreeLibrary(mod);
    if (gdi_token != 0)
        Gdiplus::GdiplusShutdown(gdi_token);
    std::printf("\n==== %s (%d failure) ====\n", g_fail == 0 ? "PASS" : "FAIL", g_fail);
    return g_fail == 0 ? 0 : 1;
}
