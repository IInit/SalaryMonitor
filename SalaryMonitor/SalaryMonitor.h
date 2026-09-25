// SalaryMonitor.h : 插件主类（ITMPlugin），把配置 / 计薪引擎 / 显示项 / 界面串起来
#pragma once

#include "include/PluginInterface.h"
#include "SalaryConfig.h"
#include "Earnings.h"
#include "Fields.h"
#include <memory>
#include <string>
#include <vector>

class CSalaryItem;

class CSalaryMonitor : public ITMPlugin
{
public:
    CSalaryMonitor();
    virtual ~CSalaryMonitor();

    // ---------------- ITMPlugin ----------------
    // 注意：ITMPlugin 没有 GetItemCount，主程序会从 index=0 开始调用
    // GetItem()，直到返回 nullptr 为止。
    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void* GetPluginIcon() override;

    virtual int GetCommandCount() override;
    virtual const wchar_t* GetCommandName(int command_index) override;
    virtual void* GetCommandIcon(int command_index) override;
    virtual void OnPluginCommand(int command_index, void* hWnd, void* para) override;
    virtual int IsCommandChecked(int command_index) override;

    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual void OnInitialize(ITrafficMonitor* pApp) override;

    // ---------------- 供显示项查询（UTF-8） ----------------
    std::string itemLabelUtf8(const std::string& key) const;
    std::string itemShortLabelUtf8(const std::string& key) const;
    std::string itemValueUtf8(const std::string& key) const;
    std::string itemSampleUtf8(const std::string& key) const;
    bool itemIsCustomDraw(const std::string& key) const;
    int todayProgressPermille() const;

    // ---------------- 供对话框使用 ----------------
    SalaryConfig& editableConfig() { return m_cfg; }
    const SalaryConfig& config() const { return m_cfg; }
    const Snapshot& snapshot() const { return m_snapshot; }
    const LocalTime& now() const { return m_snapshot.now; }

    // 应用配置：落盘 + 让下一次刷新立刻重算。返回 false 时 err 为可直接展示的提示。
    bool applyConfigChanged(std::wstring* err = nullptr);
    // 切到职业列表里的下一个（右键菜单用）
    void switchToNextProfession(std::wstring* applied = nullptr);

private:
    void initialize();
    bool ensureConfigDir();
    void rebuildItems();
    void refresh();
    void refreshAggregates();

    SalaryConfig m_cfg;
    Snapshot m_snapshot;
    std::vector<std::unique_ptr<CSalaryItem>> m_items;

    ITrafficMonitor* m_app = nullptr;
    std::wstring m_config_dir;
    std::string m_config_path;    // UTF-8

    // 月度 / 年度累计只跟"日期 + 配置"有关，跟秒无关，因此单独缓存
    earnings::Aggregates m_agg;
    std::string m_agg_key;
    int m_rev = 0;                // 配置版本号，配置一变累计就失效

    bool m_inited = false;
    mutable std::wstring m_info_cache;
    mutable std::wstring m_tooltip_cache;
    HICON m_h_icon = nullptr;
};
