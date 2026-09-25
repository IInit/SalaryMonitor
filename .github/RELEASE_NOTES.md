# SalaryMonitor v1.0.0

TrafficMonitor 插件：把「今天上班已经赚了多少钱」实时挂在任务栏上。

## 亮点

- **6 种计薪方式**：月薪固定 / 月薪+加班费 / 时薪 / 日薪 / 计件 / 底薪+提成
- **20 种职业预设**：程序员、教师、医生、公务员、销售、外卖骑手、快递员、
  网约车司机、家教、律师、主播、建筑日结工、理发师……选中即用，参数仍可手改
- **精确到秒的真实口径**：午休不计薪、下班冻结、加班按倍率加速、周末班按周末倍率
- **20 个显示项**：今日已赚 / 当前时薪 / 到账进度 / 自绘进度条 / 本月已赚 / 距发薪日……
- **收益统计窗口**：今日 / 本周 / 本月 / 本年汇总 + 最近三周逐日明细
- **纯本地计算**：不联网，配置只写在 `%APPDATA%\TrafficMonitor\plugins\`

## 安装

1. 解压后把 `SalaryMonitor.dll` 放进 TrafficMonitor 程序目录下的 `plugins\` 文件夹；
2. 重启 TrafficMonitor，右键任务栏窗口 → 显示设置 → 勾选需要的显示项；
3. 右键 → 插件命令 → **工资与排班设置…**，选职业、核对月薪与上下班时间即可。

## 计薪口径速查

| 方式 | 当前秒薪 | 加班 |
|---|---|---|
| 月薪固定 | 月薪 ÷ 当月排班总秒数 | 无（与工时无关） |
| 月薪 + 加班费 | 月薪 ÷ 当月标准工时秒数 | 超标准工时 ×1.5，周末 ×2 |
| 时薪 | 时薪 ÷ 3600 | 可选，同上 |
| 日薪 | 日薪 ÷ 当天排班秒数 | 可选，超过标准工时另乘倍率 |
| 计件 | (单价 × 当日件数) ÷ 当天排班秒数 | 无（按天摊平） |
| 底薪 + 提成 | (底薪÷当月出勤天数 + 日业绩×提成) ÷ 当天排班秒数 | 无（按天摊平） |

今日已赚 = 从 0 点到现在的费率曲线积分；本月/本年由排班模型推导，
所以重启、休眠、时钟跳变都不会把数字算歪，也不需要任何累计状态。

## 质量

本版通过了零失败的自动化验证：

- 计薪引擎确定性断言（6 种方式 / 午休剔除 / 加班倍率 / 整月口径 / 发薪日 / 格式化 / 配置往返）
- 插件接口契约（导出符号、20 个显示项取值、鼠标提示、5 个命令、配置落盘）
- 对话框探针（真实打开两个窗口：控件全可见、未裁剪、文字不截断；切换职业→保存→回显→统计；
  不可写路径必须明确报错）

## 已知边界

- 缺勤 / 请假 / 迟到早退按排班模型估算，不会自动识别（插件只读系统时间）；
- 实发工资与税前口径、社保公积金无关，这里是"按你填的月薪/单价算的名义收入"；
- 需要 TrafficMonitor 1.84+（插件接口版本 ≥ 7）的 x64 版本。

---

# SalaryMonitor v1.0.0 (English)

A TrafficMonitor plugin that puts **"how much money I've earned today"** on your taskbar,
ticking up second by second.

> Screenshots and a full English manual:
> [README.en.md](https://github.com/IInit/SalaryMonitor/blob/main/README.en.md)

## Highlights

- **6 pay models** — fixed monthly / monthly + overtime / hourly / daily / piece rate /
  base + commission
- **20 occupation presets** — programmer, teacher, doctor, civil servant, sales, delivery
  rider, courier, ride-hail driver, tutor, lawyer, streamer, construction day labourer,
  hairdresser and more. Selecting one applies sensible defaults; every field stays editable
- **Second-accurate rules** — lunch is unpaid, the amount freezes after clock-off, overtime
  accelerates at a multiplier, weekend shifts are paid at the weekend multiplier
- **20 display items** — earned today, current hourly rate, payment progress, a custom-drawn
  progress bar, earned this month, days to payday, and more
- **Earnings statistics window** — today / week / month / year summary plus a day-by-day table
- **Fully offline** — no network access; settings live only in
  `%APPDATA%\TrafficMonitor\plugins\`

## Install

1. Extract `SalaryMonitor.dll` into the `plugins\` folder inside your TrafficMonitor
   program directory.
2. Restart TrafficMonitor, right-click the taskbar window → Display settings → tick the
   items you want.
3. Right-click → Plugin commands → **Salary & shift settings…**, pick your occupation and
   confirm the salary and working hours.

## Pay models at a glance

| Model | Current rate (per second) | Overtime |
|---|---|---|
| Fixed monthly | salary ÷ total scheduled seconds this month | none (not tied to hours) |
| Monthly + overtime | salary ÷ standard-hours seconds this month | past standard ×1.5, weekends ×2 |
| Hourly | hourly wage ÷ 3600 | optional, same multipliers |
| Daily | daily wage ÷ that day's scheduled seconds | optional; past standard hours multiplied |
| Piece rate | (unit price × units today) ÷ that day's scheduled seconds | none (spread over the day) |
| Base + commission | (base ÷ working days + daily business × rate) ÷ that day's scheduled seconds | none (spread over the day) |

Earned today is the integral of the rate curve from midnight to now; month and year figures
are derived from the schedule model — so restarts, sleep and clock jumps can never
corrupt the numbers, and no accumulated state is needed.

## Quality

This release passes a zero-failure automated verification suite:

- Deterministic pay-engine assertions (6 models / lunch exclusion / overtime multipliers /
  whole-month rule / payday / formatting / config round-trip)
- Plugin interface contract (exported symbol, 20 display items return values, tooltip,
  5 commands, config persisted)
- Dialog probe (both windows really opened: every control visible, not clipped, text fits;
  switch occupation → save → reopen and verify → statistics; a non-writable path must
  report an error rather than fail silently)

## Known limitations

- Absence, leave and lateness are estimated from the schedule — the plugin only reads the
  system clock, it cannot detect real attendance;
- Figures are nominal gross pay based on the salary you enter; they ignore tax, social
  insurance and housing fund contributions;
- Requires the x64 build of TrafficMonitor 1.84+ (plugin interface version ≥ 7).
