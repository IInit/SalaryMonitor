// SalaryItem.h : 插件显示项（除"今日进度条"外，均由主程序绘制 标签 + 数值）
#pragma once

#include "include/PluginInterface.h"
#include <string>

class CSalaryMonitor;

class CSalaryItem : public IPluginItem
{
public:
    CSalaryItem(CSalaryMonitor* owner, const std::string& key);
    virtual ~CSalaryItem() = default;

    // IPluginItem
    virtual const wchar_t* GetItemName() const override;          // 显示项名称（选择列表）
    virtual const wchar_t* GetItemId() const override;
    virtual const wchar_t* GetItemLableText() const override;     // 数值前的短标签
    virtual const wchar_t* GetItemValueText() const override;
    virtual const wchar_t* GetItemValueSampleText() const override;

    // 自绘（仅"今日进度条"）
    virtual bool IsCustomDraw() const override;
    virtual int GetItemWidth() const override;
    virtual int GetItemWidthEx(void* hDC) const override;
    virtual void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;

    const std::string& Key() const { return m_key; }

private:
    CSalaryMonitor* m_owner;
    std::string m_key;
    std::wstring m_id;
    mutable std::wstring m_name;
    mutable std::wstring m_label;
    mutable std::wstring m_value;
    mutable std::wstring m_sample;
};
