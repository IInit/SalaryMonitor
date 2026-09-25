//{{NO_DEPENDENCIES}}
// resource.h : 资源与控件 ID
// 注意：注释行必须以 ASCII 字符结尾。
// 本文件会被 rc.exe 按 ANSI(GBK) 解码，若某行末尾是非 ASCII 字节（如中文），
// 该字节会与换行符组成一个双字节字符，把下一行的 #define 吞进注释，
// 导致该 ID 未定义、资源被存成字符串名而运行时找不到。
//
#define IDI_SALARYMONITOR               101

// ---- 对话框 ----
#define IDD_OPTIONS                     120
#define IDD_STATS                       122

// ---- OptionsDlg: 职业与计薪方式 ----
#define IDC_OPT_PROFESSION              1000
#define IDC_OPT_PROF_SUMMARY            1001
#define IDC_OPT_MODE                    1002
#define IDC_OPT_MODE_HINT               1003
#define IDC_OPT_APPLY_PRESET            1004

// ---- OptionsDlg: 工资参数 ----
#define IDC_OPT_MONTHLY                 1010
#define IDC_OPT_HOURLY                  1011
#define IDC_OPT_DAILY                   1012
#define IDC_OPT_UNIT_PRICE              1013
#define IDC_OPT_UNIT_COUNT              1014
#define IDC_OPT_BASE_MONTHLY            1015
#define IDC_OPT_COMMISSION              1016
#define IDC_OPT_BUSINESS                1017

// ---- OptionsDlg: 加班口径 ----
#define IDC_OPT_OT_ENABLED              1020
#define IDC_OPT_STD_HOURS               1021
#define IDC_OPT_OT_WEEKDAY              1022
#define IDC_OPT_OT_WEEKEND              1023

// ---- OptionsDlg: 排班 ----
#define IDC_OPT_WD_AM_S                 1030
#define IDC_OPT_WD_AM_E                 1031
#define IDC_OPT_WD_PM_S                 1032
#define IDC_OPT_WD_PM_E                 1033
#define IDC_OPT_SAT_AM_ON               1034
#define IDC_OPT_SAT_AM_S                1035
#define IDC_OPT_SAT_AM_E                1036
#define IDC_OPT_SAT_PM_ON               1037
#define IDC_OPT_SAT_PM_S                1038
#define IDC_OPT_SAT_PM_E                1039
#define IDC_OPT_SUN_AM_ON               1040
#define IDC_OPT_SUN_AM_S                1041
#define IDC_OPT_SUN_AM_E                1042
#define IDC_OPT_SUN_PM_ON               1043
#define IDC_OPT_SUN_PM_S                1044
#define IDC_OPT_SUN_PM_E                1045
#define IDC_OPT_PAYDAY                  1046
#define IDC_OPT_SHIFT_NOTE              1047

// ---- OptionsDlg: 显示与预览 ----
#define IDC_OPT_DECIMALS                1050
#define IDC_OPT_CURRENCY                1051
#define IDC_OPT_SHOW_CURRENCY           1052
#define IDC_OPT_THOUSANDS               1053
#define IDC_OPT_PREVIEW                 1060

// ---- StatsDlg ----
#define IDC_STATS_SUMMARY               1200
#define IDC_STATS_LIST                  1201

// ---- 版本信息 ----
#define VS_VERSION_INFO                 1

// Next default values for new objects
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        130
#define _APS_NEXT_COMMAND_VALUE         32771
#define _APS_NEXT_CONTROL_VALUE         1310
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif
