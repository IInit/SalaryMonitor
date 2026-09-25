// OptionsDlg.h : 工资与排班设置（职业选择 + 计薪参数 + 加班口径 + 排班 + 显示）
#pragma once

#include "resource.h"
#include "SalaryConfig.h"
#include <string>
#include <vector>

class COptionsDlg : public CDialogEx
{
    DECLARE_DYNAMIC(COptionsDlg)
public:
    COptionsDlg(const SalaryConfig& cfg, CWnd* parent = nullptr);
    virtual ~COptionsDlg();
    enum { IDD = IDD_OPTIONS };

    const SalaryConfig& config() const { return m_cfg; }

protected:
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    virtual BOOL PreTranslateMessage(MSG* pMsg);

    afx_msg void OnProfessionChanged();
    afx_msg void OnModeChanged();
    afx_msg void OnApplyPreset();
    afx_msg void OnOtToggle();
    afx_msg void OnTimeCheckChanged(UINT nID);
    afx_msg void OnFieldChanged(UINT nID);
    afx_msg void OnCheckChanged(UINT nID);
    DECLARE_MESSAGE_MAP()

private:
    // 用给定配置刷新所有控件
    void loadFormFrom(const SalaryConfig& cfg);
    // 把控件内容读回配置（不做校验）
    SalaryConfig formToConfig() const;
    // 校验；失败时把原因写入 err 并返回 false
    bool validate(std::wstring& err);
    // 刷新职业 / 计薪方式说明文字
    void updateSummary();
    // 按计薪方式启用/禁用参数输入框
    void updateParamEnable();
    // 刷新底部预览（今日应得 / 当前时薪 / 本月预计）
    void updatePreview();
    // 勾选框 -> 对应时段输入框的启用状态
    void updateShiftEnable();

    // 控件工厂（坐标均为 DLU）
    CStatic* addLabel(const CString& text, int x, int y, int w, int h, bool right_align);
    void addEdit(CEdit& e, int x, int y, int w, int nid, bool number_only = false);
    CButton* addCheck(const CString& text, int x, int y, int w, int nid, bool enabled);
    CButton* addGroup(const CString& text, int l, int t, int r, int b);
    void addTimePair(int x, int y, int idx_day, int idx_half);
    void wrapLabel(CStatic& lbl, const std::wstring& text, int max_lines);

    SalaryConfig m_cfg;
    CFont m_font;

    // 动态创建的控件由本对话框持有（成员或下面两个容器）。
    // 绝不用局部 CStatic + Detach()：Detach() 会让控件字体丢失（WM_GETFONT 变 NULL），
    // 渲染回退到系统默认字体，文字比测量值宽约 23%，右对齐标签会被裁掉。
    std::vector<CStatic*> m_labels;
    std::vector<CButton*> m_buttons;

    CComboBox m_cb_profession;
    CComboBox m_cb_mode;
    CButton m_btn_apply_preset;
    CStatic m_lbl_summary;

    CEdit m_e_monthly;
    CEdit m_e_hourly;
    CEdit m_e_daily;
    CEdit m_e_unit_price;
    CEdit m_e_unit_count;
    CEdit m_e_base_monthly;
    CEdit m_e_commission;
    CEdit m_e_business;

    CButton m_chk_ot;
    CEdit m_e_std_hours;
    CEdit m_e_ot_weekday;
    CEdit m_e_ot_weekend;

    CEdit m_tm[3][2][2];        // [工作日/周六/周日][上午/下午][起/止]
    CButton m_tm_on[3][2];      // 只有周六、周日的半天有勾选框

    CEdit m_e_payday;
    CEdit m_e_decimals;
    CEdit m_e_currency;
    CButton m_chk_show_currency;
    CButton m_chk_thousands;
    CStatic m_lbl_preview;

    bool m_loading = false;      // 程序化刷新控件时抑制联动重算
};
