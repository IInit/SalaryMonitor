// StatsDlg.h : 收益统计（今日 / 本周 / 本月 / 本年 + 最近三周逐日明细）
#pragma once

#include "resource.h"
#include "SalaryConfig.h"
#include <string>
#include <vector>

class CStatsDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CStatsDlg)
public:
    CStatsDlg(const SalaryConfig& cfg, CWnd* parent = nullptr);
    virtual ~CStatsDlg();
    enum { IDD = IDD_STATS };

protected:
    virtual BOOL OnInitDialog();
    DECLARE_MESSAGE_MAP()

private:
    void fillSummary();
    void fillList();

    SalaryConfig m_cfg;
    CFont m_font;
    CStatic m_lbl_summary;
    CListCtrl m_list;
};
