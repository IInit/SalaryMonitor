#!/usr/bin/env bash
# 打包发布产物：dist/SalaryMonitor-<版本>-x64.zip
#
# 内容：SalaryMonitor.dll（插件本体）+ 插件图标 + README + LICENSE + 校验值
# 前置：已执行 bash build_x64.sh
set -eu

# 统一编码环境（脚本内含中文提示，Windows 窄编码控制台下会乱码）
export PYTHONIOENCODING="utf-8:backslashreplace"
export PYTHONUTF8=1
export LC_ALL="${LC_ALL:-C.UTF-8}"
export LANG="${LANG:-C.UTF-8}"

VERSION="${1:-v1.0.0}"

_here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -W 2>/dev/null || echo "$_here")"
DLL="$ROOT/x64/Release/SalaryMonitor.dll"

if [ ! -f "$DLL" ]; then
    echo "!! 未找到 $DLL，请先执行 bash build_x64.sh"
    exit 1
fi

if [ ! -f "$ROOT/SalaryMonitor/res/salary_monitor.ico" ]; then
    python "$ROOT/tools/make_icon.py"
fi

NAME="SalaryMonitor-$VERSION-x64"
STAGE="$ROOT/dist/$NAME"
rm -rf "$STAGE"
mkdir -p "$STAGE"

cp "$DLL" "$STAGE/"
cp "$ROOT/SalaryMonitor/res/salary_monitor.ico" "$STAGE/"
cp "$ROOT/README.md" "$ROOT/LICENSE" "$STAGE/"

# 计算校验值，随包提供
( cd "$STAGE" && sha256sum SalaryMonitor.dll > SalaryMonitor.dll.sha256 )

echo "staged: $STAGE"
ls -la "$STAGE"

# 打成 zip（优先用 Python，保证在 Git Bash / CI 上行为一致）
"${PM_PYTHON:-python}" - "$ROOT" "$NAME" <<'PY'
import os, sys, zipfile
root, name = sys.argv[1], sys.argv[2]
stage = os.path.join(root, "dist", name)
out = os.path.join(root, "dist", name + ".zip")
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for fn in sorted(os.listdir(stage)):
        z.write(os.path.join(stage, fn), os.path.join(name, fn))
print("wrote %s (%d bytes)" % (out, os.path.getsize(out)))
PY
