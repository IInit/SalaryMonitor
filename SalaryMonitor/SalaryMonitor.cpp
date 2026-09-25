// SalaryMonitor.cpp : 插件主类实现
#include "pch.h"
#include "SalaryMonitor.h"
#include "SalaryItem.h"
#include "Fields.h"
#include "Format.h"
#include "Professions.h"
#include "Encoding.h"
#include "FileUtil.h"
#include "OptionsDlg.h"
#include "StatsDlg.h"
#include "resource.h"
#include <shellapi.h>
#include <cstdio>

// ------------------------------------------------------------------ 项目标识
// 单一来源：所有对外署名 / 入口链接都从这里取，避免各处硬编码不一致。
static const wchar_t* const kProductName = L"SalaryMonitor 工资监测";
static const wchar_t* const kAuthor = L"init";
static const wchar_t* const kHomepage = L"https://github.com/IInit/SalaryMonitor";
static const wchar_t* const kVersion = L"1.0.0";
static const wchar_t* const kLicense = L"MIT License (c) 2026 init";

// 右键命令索引
enum
{
    CMD_OPTIONS = 0,
    CMD_NEXT_PROFESSION = 1,
    CMD_STATS = 2,
    CMD_ABOUT = 3,
    CMD_HOMEPAGE = 4,
    CMD_COUNT
};

CSalaryMonitor::CSalaryMonitor()
{
}

CSalaryMonitor::~CSalaryMonitor()
{
}

// ----------------------------------------------------------------- 时间
static LocalTime nowLocal()
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    return sched::MakeLocalTime(st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

// 兜底配置目录：插件 DLL 自身所在目录
static std::wstring pluginOwnDir()
{
    wchar_t buf[MAX_PATH] = { 0 };
    HMODULE hMod = nullptr;
    if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              (LPCWSTR)&pluginOwnDir, &hMod))
        return std::wstring();
    if (::GetModuleFileNameW(hMod, buf, MAX_PATH) == 0)
        return std::wstring();
    std::wstring p(buf);
    size_t pos = p.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        p.resize(pos);
    return p;
}

// 递归确保目录存在。
// 注意 CreateDirectoryW 只创建最后一级，父目录缺失时会失败——
// 而配置写盘失败若是静默的，表现就是"改了没生效"，所以必须先把目录补全。
static bool ensureDirExists(const std::wstring& dir)
{
    if (dir.empty())
        return false;
    DWORD attr = ::GetFileAttributesW(dir.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES)
        return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;

    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos && pos > 0)
    {
        std::wstring parent = dir.substr(0, pos);
        if (!(parent.size() == 2 && parent[1] == L':'))   // 跳过 "D:" 这类卷标
            ensureDirExists(parent);
    }
    return ::CreateDirectoryW(dir.c_str(), nullptr) != FALSE ||
           ::GetFileAttributesW(dir.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// ----------------------------------------------------------------- 初始化
bool CSalaryMonitor::ensureConfigDir()
{
    if (m_config_dir.empty() && m_app != nullptr)
    {
        const wchar_t* p = m_app->GetPluginConfigDir();
        if (p != nullptr)
            m_config_dir = p;
    }
    if (m_config_dir.empty())
        m_config_dir = pluginOwnDir();
    ensureDirExists(m_config_dir);      // 目录缺失会让写盘静默失败

    CString dir(m_config_dir.c_str());
    if (!dir.IsEmpty() && dir.Right(1) != L"\\")
        dir += L"\\";
    m_config_path = CStringToUtf8(dir + L"SalaryMonitor_config.json");
    return !m_config_path.empty();
}

void CSalaryMonitor::initialize()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    ensureConfigDir();
    m_cfg = SalaryConfig::load(m_config_path);
    if (!FileUtil::exists(m_config_path))
        m_cfg.save(m_config_path);

    rebuildItems();

    m_rev = 0;
    m_agg_key.clear();

    m_inited = true;

    // 先出一帧快照，避免首秒为空
    refresh();
}

void CSalaryMonitor::rebuildItems()
{
    m_items.clear();
    for (const FieldDef& d : fieldDefs())
        m_items.emplace_back(new CSalaryItem(this, d.key));
}

void CSalaryMonitor::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    if (index == EI_CONFIG_DIR && data != nullptr)
    {
        m_config_dir = data;
        if (m_inited && !m_config_dir.empty())
            ensureConfigDir();
    }
}

void CSalaryMonitor::OnInitialize(ITrafficMonitor* pApp)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    m_app = pApp;
    initialize();
}

// ----------------------------------------------------------------- 数据刷新
void CSalaryMonitor::refreshAggregates()
{
    // 缓存键 = 日期 + 配置版本。日期没变、配置没改时不重算（整年 365 天要遍历一遍）
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s#%d", m_snapshot.now.date.c_str(), m_rev);
    std::string key(buf);
    if (key == m_agg_key)
        return;

    m_agg = earnings::ComputeAggregates(m_cfg, m_snapshot.now);
    m_agg_key = key;
}

void CSalaryMonitor::refresh()
{
    m_snapshot.now = nowLocal();
    m_snapshot.day = earnings::ComputeDay(m_cfg, m_snapshot.now);
    refreshAggregates();

    m_snapshot.agg = m_agg;
    m_snapshot.profession_id = m_cfg.profession;
    m_snapshot.days_to_payday = sched::DaysToPayday(m_cfg, m_snapshot.now);
    m_snapshot.month_earned = m_agg.month_past + m_snapshot.day.earned;
    m_snapshot.week_earned = m_agg.week_past + m_snapshot.day.earned;
    m_snapshot.year_earned = m_agg.year_past + m_snapshot.day.earned;
}

void CSalaryMonitor::DataRequired()
{
    if (!m_inited)
        initialize();
    refresh();
}

// ----------------------------------------------------------------- 显示项
IPluginItem* CSalaryMonitor::GetItem(int index)
{
    if (!m_inited)
        initialize();
    if (index < 0 || index >= (int)m_items.size())
        return nullptr;   // 越界返回空指针，主程序据此结束枚举
    return m_items[(size_t)index].get();
}

std::string CSalaryMonitor::itemLabelUtf8(const std::string& key) const
{
    return fieldLabelUtf8(key);
}

std::string CSalaryMonitor::itemShortLabelUtf8(const std::string& key) const
{
    return fieldShortLabelUtf8(key);
}

std::string CSalaryMonitor::itemValueUtf8(const std::string& key) const
{
    return fieldValueUtf8(key, m_snapshot, m_cfg);
}

std::string CSalaryMonitor::itemSampleUtf8(const std::string& key) const
{
    return fieldSampleUtf8(key);
}

bool CSalaryMonitor::itemIsCustomDraw(const std::string& key) const
{
    return isCustomDrawField(key);
}

int CSalaryMonitor::todayProgressPermille() const
{
    return m_snapshot.day.money_permille;
}

// ----------------------------------------------------------------- 元信息
const wchar_t* CSalaryMonitor::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return kProductName;
    case TMI_DESCRIPTION:
        return L"实时显示今天上班已经赚了多少钱：支持 20 种职业预设与 6 种计薪方式（月薪 / 月薪+加班 / 时薪 / 日薪 / 计件 / 底薪+提成），可按排班、午休、加班倍率精确到秒";
    case TMI_AUTHOR:
        return kAuthor;
    case TMI_COPYRIGHT:
        return kLicense;
    case TMI_VERSION:
        return kVersion;
    case TMI_URL:
        return kHomepage;
    default:
        return L"";
    }
}

void* CSalaryMonitor::GetPluginIcon()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    int size = ::GetSystemMetrics(SM_CXSMICON);
    if (size <= 0)
        size = 16;
    HICON h = (HICON)::LoadImageW(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_SALARYMONITOR),
                                  IMAGE_ICON, size, size, LR_SHARED);
    if (h != nullptr)
        m_h_icon = h;
    return (void*)h;
}

// ----------------------------------------------------------------- 选项
ITMPlugin::OptionReturn CSalaryMonitor::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CWnd* parent = hParent != nullptr ? CWnd::FromHandle((HWND)hParent) : nullptr;
    COptionsDlg dlg(m_cfg, parent);
    if (dlg.DoModal() == IDOK)
    {
        m_cfg = dlg.config();
        std::wstring err;
        if (!applyConfigChanged(&err))
            ::MessageBoxW((HWND)hParent, err.c_str(), L"工资与排班设置", MB_OK | MB_ICONERROR);
        return OR_OPTION_CHANGED;
    }
    return OR_OPTION_UNCHANGED;
}

// ----------------------------------------------------------------- 命令
int CSalaryMonitor::GetCommandCount()
{
    return CMD_COUNT;
}

const wchar_t* CSalaryMonitor::GetCommandName(int command_index)
{
    switch (command_index)
    {
    case CMD_OPTIONS:
        return L"工资与排班设置…";
    case CMD_NEXT_PROFESSION:
        return L"切换职业（下一个）";
    case CMD_STATS:
        return L"收益统计…";
    case CMD_ABOUT:
        return L"关于…";
    case CMD_HOMEPAGE:
        return L"项目主页（GitHub）…";
    default:
        return nullptr;
    }
}

void* CSalaryMonitor::GetCommandIcon(int command_index)
{
    (void)command_index;
    return nullptr;
}

int CSalaryMonitor::IsCommandChecked(int command_index)
{
    (void)command_index;
    return 0;
}

void CSalaryMonitor::OnPluginCommand(int command_index, void* hWnd, void* para)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    (void)para;
    HWND hwnd = (HWND)hWnd;
    CWnd* parent = hwnd != nullptr ? CWnd::FromHandle(hwnd) : nullptr;

    switch (command_index)
    {
    case CMD_OPTIONS:
    {
        std::wstring err;
        COptionsDlg dlg(m_cfg, parent);
        if (dlg.DoModal() == IDOK)
        {
            m_cfg = dlg.config();
            if (!applyConfigChanged(&err))
                ::MessageBoxW(hwnd, err.c_str(), L"工资与排班设置", MB_OK | MB_ICONERROR);
        }
        break;
    }
    case CMD_NEXT_PROFESSION:
    {
        std::wstring applied;
        switchToNextProfession(&applied);
        std::wstring msg = L"已切换到职业：" + applied;
        if (m_app != nullptr)
            m_app->ShowNotifyMessage(msg.c_str());
        else
            ::MessageBoxW(hwnd, msg.c_str(), L"切换职业", MB_OK | MB_ICONINFORMATION);
        break;
    }
    case CMD_STATS:
    {
        CStatsDlg dlg(m_cfg, parent);
        dlg.DoModal();
        break;
    }
    case CMD_ABOUT:
    {
        CString text;
        text.Format(L"%s —— TrafficMonitor 插件 v%s\r\n"
                    L"作者：%s\r\n"
                    L"许可：%s\r\n"
                    L"项目主页：%s\r\n\r\n"
                    L"· 今日已赚：按当前职业的计薪方式逐秒累计\r\n"
                    L"· 午休、下班后自动停止计薪；加班按倍率加速\r\n"
                    L"· 6 种计薪方式：月薪 / 月薪+加班 / 时薪 / 日薪 / 计件 / 底薪+提成\r\n"
                    L"· 20 种职业预设，选中即可用，全部参数仍可手工微调\r\n"
                    L"· 上班族专属：右下角看钱一秒一秒跳，比盯着时钟有动力\r\n\r\n"
                    L"是否现在打开项目主页？",
                    kProductName, kVersion, kAuthor, kLicense, kHomepage);
        if (::MessageBoxW(hwnd, text, L"关于", MB_YESNO | MB_ICONINFORMATION) == IDYES)
            ::ShellExecuteW(hwnd, L"open", kHomepage, nullptr, nullptr, SW_SHOWNORMAL);
        break;
    }
    case CMD_HOMEPAGE:
    {
        HINSTANCE r = ::ShellExecuteW(hwnd, L"open", kHomepage, nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(r) <= 32)
        {
            CString msg;
            msg.Format(L"未能打开浏览器，请手动访问：\r\n%s", kHomepage);
            ::MessageBoxW(hwnd, msg, L"项目主页", MB_OK | MB_ICONINFORMATION);
        }
        break;
    }
    }
}

// ----------------------------------------------------------------- Tooltip
const wchar_t* CSalaryMonitor::GetTooltipInfo()
{
    const Snapshot& s = m_snapshot;
    const earnings::DayResult& d = s.day;

    std::string utf8;
    char line[192];

    {
        // 第一行：职业 + 计薪方式
        utf8 += "工资监测 · " + WStringToUtf8(professionName(s.profession_id))
              + " · " + WStringToUtf8(payModeName(m_cfg.mode)) + "\n";
    }

    std::snprintf(line, sizeof(line), "今日 %s / %s（%d%%）\n",
                  fmt::Money(d.earned, m_cfg).c_str(),
                  fmt::Money(d.day_total, m_cfg).c_str(),
                  (d.money_permille + 5) / 10);
    utf8 += line;

    if (d.is_workday)
    {
        std::snprintf(line, sizeof(line), "已工作 %s · 距下班 %s · 当前时薪 %s\n",
                      fmt::Duration(d.worked_sec).c_str(),
                      fmt::Duration(d.remain_sec).c_str(),
                      fmt::MoneyEx(d.rate_per_sec * 3600.0, m_cfg, 2).c_str());
        utf8 += line;
    }
    else
    {
        utf8 += "今天排班休息，不计薪\n";
    }

    std::snprintf(line, sizeof(line), "本月 %s / %s · 距发薪 %d 天\n",
                  fmt::Money(s.month_earned, m_cfg).c_str(),
                  fmt::Money(s.agg.month_total, m_cfg).c_str(),
                  s.days_to_payday);
    utf8 += line;

    utf8 += "状态：" + WStringToUtf8(earnings::PhaseName(d.phase));

    m_tooltip_cache = Utf8ToWString(utf8);
    return m_tooltip_cache.c_str();
}

// ----------------------------------------------------------------- 配置应用
bool CSalaryMonitor::applyConfigChanged(std::wstring* err)
{
    ensureConfigDir();

    bool ok = m_cfg.save(m_config_path);
    ++m_rev;                // 配置变了 -> 累计缓存失效
    m_agg_key.clear();
    refresh();

    if (!ok && err != nullptr)
    {
        CString path = Utf8ToCString(m_config_path);
        CString msg;
        msg.Format(L"配置未能写入磁盘：\r\n%s\r\n\r\n请确认该目录存在且可写。", path.GetString());
        *err = (LPCWSTR)msg;
    }
    return ok;
}

void CSalaryMonitor::switchToNextProfession(std::wstring* applied)
{
    const std::vector<ProfessionDef>& t = professions();
    if (t.empty())
        return;
    int idx = professionIndex(m_cfg.profession);
    int next = (idx + 1) % (int)t.size();
    applyProfession(m_cfg, t[(size_t)next].id);

    std::wstring err;
    applyConfigChanged(&err);       // 落盘失败不阻塞切换，下次刷新仍会重算
    if (applied != nullptr)
        *applied = professionName(m_cfg.profession).empty() ? std::wstring(L"自定义")
                                                            : professionName(m_cfg.profession);
}

// ----------------------------------------------------------------- 导出入口
static CSalaryMonitor g_salaryMonitorPlugin;

extern "C" __declspec(dllexport)
ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &g_salaryMonitorPlugin;
}
