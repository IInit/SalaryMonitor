#!/usr/bin/env bash
# SalaryMonitor.dll 构建脚本 (x64 / Release)
#
# 设计目标：既能在本机（VS 安装不完整、直接调用 cl.exe）跑，也能在
# CI（GitHub Actions windows runner）跑，因此工具链路径全部可用环境变量覆盖：
#
#   PM_TC       MSVC 工具集目录（含 bin/Hostx64/x64/cl.exe）
#   PM_MSVC     本机已安装的 MSVC 目录（提供 ATL 头与 CRT 库）
#   PM_SDK      Windows SDK 根目录
#   PM_SDKVER   Windows SDK 版本号
#   PM_PYTHON   Python 解释器（用于生成图标、拉取依赖）
#   PM_SKIP_DIALOG_PROBE=1   跳过对话框探针（CI 上避免依赖交互式桌面）
#
# 产物：x64/Release/SalaryMonitor.dll
set -u

# 统一编码环境：脚本内含中文提示，而 Windows runner 的控制台/管道默认可能是
# cp1252 之类的窄编码，输出非 ASCII 字符会乱码甚至报错。这里显式切到 UTF-8，
# 并让所有 Python 子进程也使用 UTF-8。
export PYTHONIOENCODING="utf-8:backslashreplace"
export PYTHONUTF8=1
export LC_ALL="${LC_ALL:-C.UTF-8}"
export LANG="${LANG:-C.UTF-8}"

_here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Git Bash 的 pwd 返回 /d/... 形式，cl.exe / rc.exe 会把它当成选项；
# 优先用 pwd -W 取到 D:/... 形式。
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -W 2>/dev/null || echo "$_here")"
PROJ="$ROOT/SalaryMonitor"
OUT="$ROOT/x64/Release"
OBJ="$OUT/obj"

# 在 CI 上，下面这些默认值（本机路径）一定是错的。若检测到运行在 GitHub
# Actions 却没有显式传入工具链路径，直接报错退出，避免拿本机路径去编译、
# 最后以一堆 C1083 的形式浪费一轮 CI。
# 注意：必须在套用 :- 默认值 **之前** 判断，否则变量已被填上默认值。
if [ -n "${GITHUB_ACTIONS:-}" ]; then
    _missing=""
    for _v in PM_TC PM_MSVC PM_SDK PM_SDKVER; do
        eval "_cur=\${$_v:-}"
        [ -n "$_cur" ] || _missing="$_missing $_v"
    done
    if [ -n "$_missing" ]; then
        echo "!! CI 环境下缺少工具链变量:$_missing（应由 workflow 的 Locate 步骤写入 GITHUB_ENV）"
        exit 1
    fi
fi

PM_TC="${PM_TC:-D:/MyFile/SalaryMonitorPlugin/.pmtoolchain/VC/Tools/MSVC/14.51.36231}"
PM_MSVC="${PM_MSVC:-D:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231}"
PM_SDK="${PM_SDK:-D:/Windows Kits/10}"
PM_SDKVER="${PM_SDKVER:-10.0.26100.0}"

CL="$PM_TC/bin/Hostx64/x64/cl.exe"
LINK="$PM_TC/bin/Hostx64/x64/link.exe"
RC="$PM_SDK/bin/$PM_SDKVER/x64/rc.exe"

# 工具链完整性自检：路径写错时立刻给出可读的原因，而不是等编译报错
for tool in "$CL" "$LINK" "$RC"; do
    if [ ! -f "$tool" ]; then
        echo "!! 工具链缺失: $tool"
        echo "   PM_TC=$PM_TC"
        echo "   PM_SDK=$PM_SDK  PM_SDKVER=$PM_SDKVER"
        exit 1
    fi
done

# SDK 关键头必须存在（历史上这里出过 C1083: new.h / crtdbg.h）。
# 注意这里是"库定位错误"的典型症状，必须显式检查。
for hdr in \
    "$PM_SDK/Include/$PM_SDKVER/ucrt/new.h" \
    "$PM_SDK/Include/$PM_SDKVER/ucrt/crtdbg.h" \
    "$PM_SDK/Include/$PM_SDKVER/um/windows.h"
do
    if [ ! -f "$hdr" ]; then
        echo "!! 缺少 SDK 头文件: $hdr"
        echo "   PM_SDK=$PM_SDK  PM_SDKVER=$PM_SDKVER（请确认该版本已安装）"
        exit 1
    fi
done

# MFC 头可能分散在两处（一种常见布局是 PM_TC 提供 MFC、PM_MSVC 只提供 ATL），
# 因此只要二者之一含有 afxwin.h 即视为满足 —— 与下面 INCLUDE 的拼法一致。
if [ ! -f "$PM_TC/atlmfc/include/afxwin.h" ] && [ ! -f "$PM_MSVC/atlmfc/include/afxwin.h" ]; then
    echo "!! 找不到 MFC 头 afxwin.h，已检查："
    echo "   $PM_TC/atlmfc/include/"
    echo "   $PM_MSVC/atlmfc/include/"
    exit 1
fi

echo "=== toolchain OK (MSVC=$PM_TC, SDK=$PM_SDKVER) ==="

export MSYS2_ARG_CONV_EXCL='*'

# --------------------------------------------------------------- Python
find_python() {
    if [ -n "${PM_PYTHON:-}" ]; then echo "$PM_PYTHON"; return; fi
    for c in python3 python py; do
        if command -v "$c" >/dev/null 2>&1; then echo "$c"; return; fi
    done
    echo ""
}
PY="$(find_python)"

# --------------------------------------------------------------- 依赖与图标
# 图标不随仓库分发二进制，由脚本确定性重绘（仓库保持纯文本）。
if [ ! -f "$PROJ/res/salary_monitor.ico" ]; then
    echo "=== generate icon ==="
    if [ -z "$PY" ]; then
        echo "!! 缺少 salary_monitor.ico 且未找到 Python，请安装 Python 后重试"
        exit 1
    fi
    "$PY" "$ROOT/tools/make_icon.py" || exit 1
fi

# 第三方依赖 yyjson：缺失时按固定版本 + SHA-256 拉取
if [ ! -f "$PROJ/yyjson/yyjson.c" ] || [ ! -f "$PROJ/yyjson/yyjson.h" ]; then
    echo "=== fetch deps (yyjson) ==="
    if [ -z "$PY" ]; then
        echo "!! 缺少 SalaryMonitor/yyjson，且未找到 Python，无法自动拉取"
        exit 1
    fi
    "$PY" "$ROOT/tools/fetch_deps.py" || exit 1
fi

mkdir -p "$OBJ"

# atlmfc/include 说明：
#   $PM_TC/atlmfc/include   —— 还原 / 安装的 MFC 头（afx*.h）
#   $PM_MSVC/atlmfc/include —— 同版本 ATL 头（__atlmfc_core.h 等），与上面互补
export INCLUDE="$PM_TC/atlmfc/include;$PM_MSVC/atlmfc/include;$PM_MSVC/include;$PM_SDK/Include/$PM_SDKVER/ucrt;$PM_SDK/Include/$PM_SDKVER/shared;$PM_SDK/Include/$PM_SDKVER/um;$PM_SDK/Include/$PM_SDKVER/winrt;$PM_SDK/Include/$PM_SDKVER/cppwinrt"
export LIB="$PM_MSVC/lib/x64;$PM_MSVC/atlmfc/lib/x64;$PM_TC/atlmfc/lib/x64;$PM_SDK/Lib/$PM_SDKVER/ucrt/x64;$PM_SDK/Lib/$PM_SDKVER/um/x64"

CFLAGS="/nologo /c /utf-8 /EHsc /std:c++17 /MD /O2 /Oi /Gy /W3 /WX- /permissive- /DNDEBUG /D_WINDOWS /D_USRDLL /D_AFXDLL /DUNICODE /D_UNICODE /I$PROJ /I$PROJ/include${PM_EXTRA_CDEFS:-}"

# 计薪核心（SalaryConfig / Schedule / Professions / Earnings / Format）刻意不依赖
# Windows / MFC，单元测试宿主可以直接把它们编译进去跑断言。
SOURCES=(
  pch.cpp
  SalaryConfig.cpp
  Schedule.cpp
  Professions.cpp
  Earnings.cpp
  Format.cpp
  Fields.cpp
  FileUtil.cpp
  Encoding.cpp
  SalaryMonitor.cpp
  SalaryItem.cpp
  OptionsDlg.cpp
  StatsDlg.cpp
  DlgLayout.cpp
)
CSOURCES=( yyjson/yyjson.c )

fail=0

# 带重试地执行一条命令。
#
# 为什么要重试：cl.exe / link.exe 在本机（以及装了实时防护、开索引的开发机上）
# 偶发出现
#   fatal error C1083: 无法打开编译器生成的文件: "...\xxx.obj": Permission denied
#   LINK : fatal error LNK1104: 无法打开文件 "...\xxx.obj"
# 这不是代码问题——文件本身可写、可删，只是刚被写出来的一瞬间被实时防护 /
# 索引服务短暂独占。一次瞬时失败就让整轮构建报废代价太大，所以失败后清理
# 残留产物再试；重试仍失败才算真失败。
#
# 参数：<描述> <失败时清理的残留文件（可为空）> <命令...>
retry_cmd()
{
    local _desc="$1"; shift
    local _clean="$1"; shift
    local _try=1
    while :; do
        if "$@"; then
            return 0
        fi
        if [ "$_try" -ge 3 ]; then
            echo "!! $_desc 失败（已重试 $((_try - 1)) 次）"
            return 1
        fi
        echo "   (重试 $_desc…)"
        [ -n "$_clean" ] && rm -f "$_clean" 2>/dev/null
        sleep 1
        _try=$((_try + 1))
    done
}

# 编译单个源文件（带重试）
compile_one()
{
    local _desc="$1"; shift
    local _obj="$1"; shift
    retry_cmd "$_desc" "$_obj" "$CL" "$@"
}

echo "=== compile C++ sources ==="
for s in "${SOURCES[@]}"; do
    o="$OBJ/$(basename "$s" .cpp).obj"
    echo "--- $s"
    compile_one "$s" "$o" $CFLAGS "/Fo$o" "$PROJ/$s" || fail=1
done

echo "=== compile C sources ==="
for s in "${CSOURCES[@]}"; do
    o="$OBJ/$(basename "$s" .c).obj"
    echo "--- $s"
    compile_one "$s" "$o" /nologo /c /utf-8 /MD /O2 /W3 /WX- /DNDEBUG "/Fo$o" "$PROJ/$s" || fail=1
done

echo "=== compile resources ==="
retry_cmd "rc SalaryMonitor.rc" "$OBJ/SalaryMonitor.res" \
    "$RC" /nologo /fo "$OBJ/SalaryMonitor.res" "$PROJ/SalaryMonitor.rc" || fail=1
ls -la "$OBJ/SalaryMonitor.res" 2>/dev/null || fail=1

if [ "$fail" != "0" ]; then
    echo "!! compile failed, skip link"
    exit 1
fi

echo "=== link ==="
OBJS="$OBJ/pch.obj $OBJ/SalaryConfig.obj $OBJ/Schedule.obj $OBJ/Professions.obj $OBJ/Earnings.obj $OBJ/Format.obj $OBJ/Fields.obj $OBJ/FileUtil.obj $OBJ/Encoding.obj $OBJ/SalaryMonitor.obj $OBJ/SalaryItem.obj $OBJ/OptionsDlg.obj $OBJ/StatsDlg.obj $OBJ/DlgLayout.obj $OBJ/yyjson.obj $OBJ/SalaryMonitor.res"

retry_cmd "link SalaryMonitor.dll" "$OUT/SalaryMonitor.dll" \
"$LINK" /nologo /DLL /SUBSYSTEM:WINDOWS /MACHINE:X64 \
    /OPT:REF /OPT:ICF /INCREMENTAL:NO /DEBUG:NONE \
    "/OUT:$OUT/SalaryMonitor.dll" /IMPLIB:"$OUT/SalaryMonitor.lib" \
    $OBJS \
    mfc140u.lib mfcs140u.lib \
    kernel32.lib user32.lib gdi32.lib comctl32.lib ole32.lib oleaut32.lib uuid.lib \
    shell32.lib advapi32.lib version.lib psapi.lib winmm.lib gdiplus.lib \
    || fail=1

echo "=== result ==="
ls -la "$OUT"
if [ "$fail" != "0" ]; then
    exit 1
fi

# ---------------------------------------------------------------- 冒烟测试宿主
# 除了验证插件接口契约，还把计薪核心编译进去跑一组确定性断言
# （换职业 / 加班倍率 / 午休剔除 / 月末月初边界等）。
echo "=== build smoke host ==="
rm -rf "$OBJ/smoke"; mkdir -p "$OBJ/smoke"
compile_one "PluginSmokeTest" "$OBJ/smoke/yyjson.obj" \
    /nologo /EHsc /std:c++17 /MD /O2 /utf-8 /DUNICODE /D_UNICODE \
    "/I$PROJ" "/I$PROJ/include" \
    "$ROOT/tests/windows/smoke_host.cpp" \
    "$PROJ/SalaryConfig.cpp" "$PROJ/Schedule.cpp" "$PROJ/Professions.cpp" \
    "$PROJ/Earnings.cpp" "$PROJ/Format.cpp" "$PROJ/Fields.cpp" \
    "$PROJ/FileUtil.cpp" \
    "$PROJ/yyjson/yyjson.c" \
    "/Fo$OBJ/smoke/" "/Fe:$OUT/PluginSmokeTest.exe" || exit 1

echo "=== run smoke test ==="
"$OUT/PluginSmokeTest.exe" "$OUT/SalaryMonitor.dll" "$OUT/_smokecfg"
rc=$?
rm -rf "$OUT/_smokecfg"
if [ "$rc" != "0" ]; then
    echo "!! smoke test failed (rc=$rc)"
    exit 1
fi

# ---------------------------------------------------------------- 对话框探针
# 实际加载 DLL 打开"工资与排班设置 / 收益统计"对话框，枚举控件并验证
#   · 所有控件（含模板按钮）可见、未被裁剪、文字放得下
#   · 「确定」保存后配置确实落盘；不可写路径必须明确报错而不是静默失败
if [ "${PM_SKIP_DIALOG_PROBE:-0}" = "1" ]; then
    echo "=== dialog probe skipped (PM_SKIP_DIALOG_PROBE=1) ==="
    echo "=== ALL OK ==="
    exit 0
fi

echo "=== build dialog probe ==="
rm -rf "$OBJ/probe"; mkdir -p "$OBJ/probe"
compile_one "DlgProbe" "$OBJ/probe/dialog_probe.obj" \
    /nologo /EHsc /std:c++17 /MD /O2 /utf-8 /DUNICODE /D_UNICODE \
    "/I$PROJ" "/I$PROJ/include" \
    "$ROOT/tests/windows/dialog_probe.cpp" \
    "/Fo$OBJ/probe/" "/Fe:$OUT/DlgProbe.exe" \
    user32.lib gdi32.lib gdiplus.lib ole32.lib || exit 1

echo "=== run dialog probe ==="
# 第 3 个参数是截图目录：把对话框渲染成 PNG，便于人工核对文字是否完整
PNG_DIR="$OUT/_shots"
rm -rf "$PNG_DIR"
mkdir -p "$PNG_DIR"
"$OUT/DlgProbe.exe" "$OUT/SalaryMonitor.dll" "$OUT/_probecfg" "$PNG_DIR"
rc=$?
rm -rf "$OUT/_probecfg"
if [ "$rc" != "0" ]; then
    echo "!! dialog probe failed (rc=$rc)"
    exit 1
fi

echo "=== ALL OK ==="
exit 0
