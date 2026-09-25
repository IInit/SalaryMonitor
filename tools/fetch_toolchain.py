# -*- coding: utf-8 -*-
"""从 VS 本地包缓存还原 MSVC 编译器，并从 VS 官方频道下载 MFC 开发包。"""
import json
import os
import glob
import shutil
import sys
import urllib.request
import zipfile

CACHE = r"D:\ProgramData\Microsoft\VisualStudio\Packages"
# 输出目录默认取本脚本所在仓库根目录下的 .pmtoolchain（可用 PM_TOOLCHAIN_OUT 覆盖），
# 不要写死某个项目的绝对路径 —— 换台机器 / 换个项目就失效了。
OUT = os.environ.get("PM_TOOLCHAIN_OUT") or os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), ".pmtoolchain")
MSVC_VER = "14.51.36231"


def log(*a):
    print(*a, flush=True)


def find_cached(pkg_id, version=None):
    """在本地包缓存中查找已下载的 payload.vsix。"""
    for d in glob.glob(os.path.join(CACHE, pkg_id + ",*")):
        p = os.path.join(d, "payload.vsix")
        if os.path.exists(p):
            return p
    return None


def load_vsman():
    for p in (r"C:\Users\init\AppData\Local\Temp\vsman.json",):
        if os.path.exists(p):
            with open(p, encoding="utf-8-sig") as f:
                return json.load(f)
    raise SystemExit("缺少 vsman.json")


def find_payload_in_manifest(vsman, pkg_id):
    cands = []
    for p in vsman["packages"]:
        if p["id"] == pkg_id and p.get("payloads"):
            cands.append(p)
    if not cands:
        return None
    # 优先选择带 payload 的
    return cands[0]


def download(url, dest):
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        log("   已存在，跳过下载")
        return dest
    log("   下载", url.rsplit("/", 1)[-1])
    with urllib.request.urlopen(url, timeout=300) as r, open(dest, "wb") as f:
        shutil.copyfileobj(r, f)
    return dest


def extract_vsix(vsix_path, dest_root, only_prefixes=None):
    n = 0
    with zipfile.ZipFile(vsix_path) as z:
        for info in z.infolist():
            name = info.filename
            if not name.startswith("Contents/"):
                continue
            rel = name[len("Contents/"):]
            if not rel or rel.endswith("/"):
                continue
            if only_prefixes and not any(rel.startswith(p) for p in only_prefixes):
                continue
            target = os.path.join(dest_root, rel.replace("/", os.sep))
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with z.open(info) as src, open(target, "wb") as dst:
                shutil.copyfileobj(src, dst)
            n += 1
    return n


def main():
    dl = os.path.join(OUT, "_dl")
    os.makedirs(dl, exist_ok=True)

    # ---------- 1) 编译器：本地缓存 ----------
    log("[1/4] 还原 MSVC 编译器（本地缓存）")
    tools_id = "Microsoft.VC.14.51.Tools.HostX64.TargetX64.base"
    tools = find_cached(tools_id)
    if not tools:
        raise SystemExit("缓存中未找到 " + tools_id)
    log("   ", tools)
    n = extract_vsix(tools, OUT)
    log("    解出 %d 个文件" % n)

    # 编译器中文资源（CLUI）
    res_id = "Microsoft.VC.14.51.Tools.HostX64.TargetX64.Res.base"
    res = find_cached(res_id)
    if res:
        n = extract_vsix(res, OUT)
        log("    语言资源解出 %d 个文件" % n)

    # ---------- 2) MFC 头文件 / 库：官方频道 ----------
    vsman = load_vsman()
    plan = [
        ("Microsoft.VC.14.51.MFC.Headers.base", "MFC 头文件"),
        ("Microsoft.VC.14.51.MFC.X64.base", "MFC x64 库"),
    ]
    log("[2/4] 获取 MFC 开发包（VS 官方源）")
    for pkg_id, desc in plan:
        local = find_cached(pkg_id)
        if local:
            log("  %s <- 本地缓存" % desc)
            vsix = local
        else:
            p = find_payload_in_manifest(vsman, pkg_id)
            if not p:
                log("  !! 清单中没有 " + pkg_id)
                continue
            pay = p["payloads"][0]
            log("  %s (%.1f MB)" % (desc, pay["size"] / 1048576.0))
            vsix = download(pay["url"], os.path.join(dl, pkg_id + ".vsix"))
        n = extract_vsix(vsix, OUT)
        log("    解出 %d 个文件" % n)

    # ---------- 3) 汇总 ----------
    log("[3/4] 工具链布局")
    base = os.path.join(OUT, "VC", "Tools", "MSVC", MSVC_VER)
    for sub in ("bin/Hostx64/x64", "atlmfc/include", "atlmfc/lib/x64", "include", "lib/x64"):
        p = os.path.join(base, sub.replace("/", os.sep))
        cnt = len(os.listdir(p)) if os.path.isdir(p) else -1
        log("   %-20s %s" % (sub, cnt))
    log("[4/4] 完成 ->", base)


if __name__ == "__main__":
    main()
