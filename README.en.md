<div align="center">

<img src="docs/images/icon.png" width="80" alt="SalaryMonitor icon">

# SalaryMonitor

**A TrafficMonitor plugin that puts "how much money I've earned today" on your taskbar, ticking up second by second.**

20 occupation presets × 6 pay models, computed to the second from your shift schedule, lunch break and overtime multipliers.

[中文](README.md) · **English**

</div>

---

## The problem it solves

For salaried workers, "how much have I earned today" has no physical reality — a monthly
salary is just a number, and it only lands on payday. SalaryMonitor turns that into
**something you can watch grow**:

```
今日  ¥412.36        (earned today)
应得  ¥1,136.36      (expected for the full day)
时薪  ¥143.68        (current hourly rate)
进度  ████████░░░░ 36.2%
```

It accrues per second using the pay rules of your actual occupation — the number stops
during lunch, speeds up once you're on overtime, freezes after you clock out, and weekend
shifts are paid at the weekend multiplier. This is arithmetic, not encouragement.

## Screenshots

**Live in the taskbar** — sitting next to CPU / memory / network items:

<img src="docs/images/taskbar.png" width="560" alt="taskbar">

> A real taskbar capture. At the moment of the screenshot it was 00:19 on a Saturday in
> China, and the selected preset was *Teacher*, which is a two-day-weekend occupation —
> hence `¥0.00` for today. Rest days earn nothing by design, so this is correct behavior.

**Salary & shift settings** — with a live preview line at the bottom that recalcs as you type:

<img src="docs/images/settings.png" width="560" alt="settings">

**Earnings statistics** — today / this week / this month / this year, plus a per-day table:

<img src="docs/images/stats.png" width="560" alt="stats">

## Install

1. Download `SalaryMonitor-x64.zip` and extract `SalaryMonitor.dll`.
2. Drop `SalaryMonitor.dll` into the **`plugins\` folder inside your TrafficMonitor
   program directory** (create the folder if it doesn't exist).
3. Restart TrafficMonitor → right-click the taskbar window → **Display settings** →
   tick the SalaryMonitor items you want.

Settings are stored in `%APPDATA%\TrafficMonitor\plugins\SalaryMonitor_config.json`.
Delete that file to remove every trace of the plugin's configuration.

> Requires TrafficMonitor 1.84+ (plugin interface version ≥ 7), x64 build.

## Getting started in three steps

| Step | What to do |
|---|---|
| 1 | Right-click the taskbar window → Plugin commands → **Salary & shift settings…** |
| 2 | Pick your occupation (e.g. **Programmer**) — pay figures and shifts fill in automatically |
| 3 | Check the salary and working hours, then click OK |

The settings window has a **live preview** at the bottom: change anything and it immediately
shows the expected daily pay, the current hourly rate and the projected monthly total —
no need to save first.

## Display items (pick any combination)

| Item | Meaning | Example |
|---|---|---|
| Earned today | Accrued since midnight | `¥412.36` |
| Expected today | Full day's pay, planned overtime included | `¥1,136.36` |
| Current hourly rate | Current accrual speed expressed per hour; 0 during lunch/after work | `¥143.68` |
| Work progress today | Paid hours ÷ scheduled hours | `36.2%` |
| Payment progress today | Earned ÷ expected | `36.2%` |
| Progress bar | **Custom-drawn bar** with a percentage label | `████░░ 36%` |
| Hours worked today | Excludes lunch and other gaps | `2h42m` |
| Time to clock-off | Until the last shift of the day ends | `4h18m` |
| Overtime today | Time beyond the standard hours | `1h30m` |
| Attendance status | Off duty / Working / On break / Clocked out / Rest day | `Working` |
| Earned per hour | Current rate × 3600 | `¥143.68` |
| Earned per minute | Current rate × 60 | `¥2.3946` |
| Earned per second | Current rate (CNY/s) | `¥0.0399` |
| Earned this month | 1st of the month → now | `¥16,842.10` |
| Expected this month | Projected whole month, weekend shifts and overtime included | `¥25,000.00` |
| Month progress | Earned ÷ expected | `67.4%` |
| Earned this year | January 1 → now | `¥188,420.55` |
| Days to payday | Until the next payday | `12 days` |
| Current occupation | The selected preset | `Programmer` |
| Pay model | The pay rules in use | `Monthly + overtime` |

Hovering over the plugin area on the taskbar shows a full breakdown (occupation, pay model,
today, this month, status).

## Occupation presets (20)

Selecting one applies that occupation's default pay model, default amounts and default
shifts — **every field remains editable afterwards**.

| Occupation | Pay model | Default figures | Default shifts |
|---|---|---|---|
| Programmer | Monthly + overtime | 20000 | 09:30-12:00 / 13:30-18:30 · weekends off |
| Programmer (995/996) | Monthly + overtime | 25000 | 09:30-12:00 / 13:30-21:00 · works Saturdays |
| Teacher | Fixed monthly | 9000 | 07:50-11:50 / 14:00-17:30 · weekends off |
| Civil servant / public sector | Fixed monthly | 8000 | 09:00-12:00 / 13:30-17:30 · weekends off |
| Doctor / nurse | Monthly + overtime | 15000 | 08:00-12:00 / 14:00-17:30 · works Saturdays |
| Accountant / finance | Fixed monthly | 9000 | 09:00-12:00 / 13:30-17:30 · weekends off |
| Bank teller | Fixed monthly | 10000 | 08:30-12:00 / 13:30-17:30 · weekends off |
| Customer service / clerk | Fixed monthly | 6000 | 09:00-12:00 / 13:30-18:00 · weekends off |
| Sales | Base + commission | base 4000 + 5% × 5000 daily business | 09:00-12:00 / 13:30-18:00 · works Saturdays |
| Hairdresser / nail artist | Base + commission | base 3000 + 30% × 800 daily business | 10:00-14:00 / 15:00-21:00 · all year |
| Food delivery rider | Piece rate | 4 per order × 60 orders | 10:00-14:00 / 17:00-21:00 · all year |
| Courier | Piece rate | 1.5 per parcel × 200 parcels | 07:30-12:00 / 13:30-18:30 · works Saturdays |
| Factory piece work | Piece rate | 1.2 per unit × 300 units | 08:00-12:00 / 13:00-17:30 · works Saturdays |
| Ride-hail / truck driver | Hourly | 45 per hour | 07:00-11:00 / 16:00-21:00 · all year |
| Tutor / part-time | Hourly | 80 per hour | 18:00-20:00 · weekends off |
| Lawyer | Hourly | 800 per hour | 09:00-12:00 / 13:30-18:00 · weekends off |
| Streamer / creator | Hourly | 150 per hour | 20:00-23:00 · all year |
| Construction / daily wage | Daily | 350 per day | 07:00-11:30 / 13:30-18:00 · all year |
| Freelancer | Daily | 500 per day | 09:00-12:00 / 14:00-18:00 · all year |
| Custom | Your choice | keeps everything you had | you decide |

> Occupation missing? Pick the closest one and edit the numbers, or choose **Custom** and
> fill it in from scratch.

## The six pay models (formulas)

Let **R** = current rate (CNY per second). Then **earned today = ∫₀^now R dt**, integrated
piecewise — which is why lunch is never counted.

| Model | Current rate R | Notes |
|---|---|---|
| **Fixed monthly** | monthly salary ÷ total scheduled seconds this month | Accrues evenly inside shifts; weekend shifts at normal rate. Whole month = salary |
| **Monthly + overtime** | overtime on: salary ÷ **standard hours** this month<br>overtime off: salary ÷ total scheduled seconds | Time beyond standard hours ×weekday multiplier; whole weekend shifts ×weekend multiplier. So even a monthly salary floats with overtime |
| **Hourly** | hourly wage ÷ 3600 | Only scheduled time; with overtime enabled, hours past the standard get the multiplier |
| **Daily** | daily wage ÷ that day's scheduled seconds | Overtime past standard hours is multiplied, so a heavy-overtime day **exceeds the daily wage** |
| **Piece rate** | (unit price × units today) ÷ that day's scheduled seconds | Independent of hours — spread evenly across the day's shift (a live version of "today's income ÷ today's duration") |
| **Base + commission** | (base ÷ working days this month + daily business × rate) ÷ that day's scheduled seconds | Base is spread over the month's working days; commission accrues live |

**How overtime is determined**: within each day, hours accumulate in shift order; the first
"standard hours" (default 8) are standard time and anything beyond is overtime at the
*weekday multiplier* (default 1.5). **Weekends are paid entirely at the weekend multiplier**
(default 2). Fixed-monthly, piece-rate and commission models are not tied to hours, so they
have no overtime bonus.

**Lunch and clock-off**: morning and afternoon are two independent intervals; the gap between
them (e.g. 12:00-13:30) is unpaid and the status shows "On break" with a rate of 0. Once the
last shift of the day ends, the amount freezes.

**This month / this year**: derived from the schedule model — past days count their full
scheduled pay, today uses the live accrual. The numbers therefore always reconcile with the
monthly total, and nothing is lost when the computer restarts.

## Right-click menu

| Command | Effect |
|---|---|
| Salary & shift settings… | Opens the settings window |
| Next occupation | Cycles through the 20 presets — handy for "what would I earn as…" |
| Earnings statistics… | Today / week / month / year summary + the last three weeks, day by day |
| About… | Version, author, algorithm notes |
| Project homepage (GitHub)… | Opens the repository |

## FAQ

**The number never moves.**
Check the attendance status first — `Off duty / On break / Clocked out / Rest day` are all
unpaid by design. Then verify the shift times against your system clock.

**Overtime pay isn't counted.**
① Look at **standard hours** under the overtime settings: if the day's shift doesn't exceed
standard hours, there is no overtime period. ② If you actually worked late but the shift
doesn't say so, move the end time to the real one — nothing is paid past the scheduled end.

**Why is the "per day" value different each month on a monthly salary?**
The salary is spread over the **actual working days of that month**, and February and March
have different counts. That's the real-world rule, not a bug.

**How do I enter piece-rate / commission work?**
Fill in "units today" or "daily business" and the plugin spreads that money evenly across
the day's shift. When the numbers change (say 80 orders today), just edit the value.

**What about school holidays or a whole month off?**
Untick all four shift halves for that month; on save it warns "scheduled hours are 0" and
asks for confirmation. After that, earnings show 0.

**My occupation isn't listed.**
Pick the closest and edit, or choose Custom and start from blank. Presets are **only
defaults** — every field can be changed after selection.

**Will my boss find out?**
It only reads the system clock and does arithmetic. It never accesses the network and writes
nothing outside its own config file.

## Build from source

```bash
# Requires MSVC (with ATL/MFC) + Windows SDK, in a Git Bash environment.
bash build_x64.sh                          # compile + link + smoke tests + dialog probe
PM_SKIP_DIALOG_PROBE=1 bash build_x64.sh   # skip the probe that needs an interactive desktop (CI)
bash tools/package.sh v1.0.0               # package into dist/SalaryMonitor-v1.0.0-x64.zip
```

Toolchain paths can be overridden with the environment variables `PM_TC` / `PM_MSVC` /
`PM_SDK` / `PM_SDKVER`; CI (GitHub Actions, `windows-2022`) detects them automatically.
No binaries are committed: the icon is deterministically redrawn by `tools/make_icon.py`
and yyjson is fetched at a pinned version with a SHA-256 check by `tools/fetch_deps.py`.

If MFC is missing locally, `python3 tools/fetch_toolchain.py` restores it from the Visual
Studio package cache.

### Tests

`build_x64.sh` performs two real verification passes and only reports success when both are
green:

* **Smoke test** (`tests/windows/smoke_host.cpp`): compiles the pay engine directly into a
  test host and asserts against a fixed date and fixed times — all six pay models, lunch
  exclusion, overtime multipliers, the whole-month monthly-salary rule, payday, thousands
  separators and rounding, config serialisation round-trip — plus the plugin interface
  contract (exported symbol, display item enumeration, non-empty values, config persisted,
  no leftover branding from other projects).
* **Dialog probe** (`tests/windows/dialog_probe.cpp`): actually loads the DLL and opens both
  dialogs, enumerates every child control and asserts "all visible / not clipped / text
  fits", then walks the full interaction chain: switch occupation → figures update
  automatically → OK to save → reopen and verify → statistics window has 21 detail rows →
  nested directory fallback creation → a non-writable path must raise an error (silent
  failure is not allowed). The probe also saves the dialog screenshots used above.

### Where the screenshots above come from

None of them are drawn or mocked — all were captured from a real run on a real desktop:

* `docs/images/settings.png` and `docs/images/stats.png` are captured automatically by the
  dialog probe **before any test rewrites the configuration** (phases 0 / 0b), so they show
  the genuine factory defaults (Programmer / 25000 / ¥). Later phases deliberately change it
  to "Teacher / 8888 / $" to verify saving and redisplay, which is why the documented shots
  are taken in their own phases.
* `docs/images/taskbar.png` is a **real taskbar capture** taken with
  `python3 tools/shot_taskbar.py`. TrafficMonitor embeds its taskbar window inside
  `Shell_TrayWnd` with `SetParent`, so ordinary top-level window enumeration cannot see it —
  the script therefore walks `EnumChildWindows` and marks itself per-monitor DPI aware first,
  otherwise the coordinates and the bitmap disagree on a scaled display.

## License

MIT License © 2026 init
