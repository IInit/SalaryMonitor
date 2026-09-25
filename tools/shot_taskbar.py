# -*- coding: utf-8 -*-
"""抓取 TrafficMonitor 任务栏窗口的真实截图（供 README 使用）。

为什么不用 PowerShell：本机安全策略拦截了 Add-Type / Assembly.Load，
无法在 PowerShell 里做截图与 P/Invoke，所以改用 Python + ctypes + PIL。

用法：
    python tools/shot_taskbar.py --list                 # 只列出顶层窗口与坐标
    python tools/shot_taskbar.py --out shot.png         # 抓全屏
    python tools/shot_taskbar.py --out shot.png --crop L,T,R,B --scale 2
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import os
import sys

u = ctypes.windll.user32
# 让本进程成为 DPI 感知进程：否则在缩放 125%/150% 的显示器上，GetWindowRect 拿到的是
# 被系统虚拟化过的逻辑坐标，而位图是物理像素，两者对不上，裁出来的位置会整体偏移。
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)      # PROCESS_PER_MONITOR_DPI_AWARE
except Exception:
    try:
        u.SetProcessDPIAware()
    except Exception:
        pass


class RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                ("right", ctypes.c_long), ("bottom", ctypes.c_long)]


ENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)


def list_windows(keyword="traffic"):
    """返回 [(hwnd, class, title, l, t, r, b, visible)]，只保留 class/title 命中关键字的。"""
    out = []

    def _cb(hwnd, _lparam):
        cls = ctypes.create_unicode_buffer(512)
        u.GetClassNameW(hwnd, cls, 512)
        txt = ctypes.create_unicode_buffer(512)
        u.GetWindowTextW(hwnd, txt, 512)
        if keyword.lower() in cls.value.lower() or keyword.lower() in txt.value.lower():
            r = RECT()
            u.GetWindowRect(hwnd, ctypes.byref(r))
            out.append((hwnd, cls.value, txt.value, r.left, r.top, r.right, r.bottom,
                        bool(u.IsWindowVisible(hwnd))))
        return True

    u.EnumWindows(ENUMPROC(_cb), 0)
    return out


def named_window(cls_name):
    """按类名精确取窗口矩形（不存在返回 None）。"""
    hwnd = u.FindWindowW(cls_name, None)
    if not hwnd:
        return None
    r = RECT()
    u.GetWindowRect(hwnd, ctypes.byref(r))
    return (r.left, r.top, r.right, r.bottom)


def child_windows(parent_cls, keyword="traffic"):
    """枚举某个顶层窗口的**子窗口**。

    任务栏窗口是 TrafficMonitor 用 SetParent 嵌进 Shell_TrayWnd 里的子窗口，
    EnumWindows（只走顶层窗口）看不到它 —— 必须用 EnumChildWindows 才找得到。
    """
    parent = u.FindWindowW(parent_cls, None)
    if not parent:
        return []
    out = []

    def _cb(hwnd, _lparam):
        cls = ctypes.create_unicode_buffer(512)
        u.GetClassNameW(hwnd, cls, 512)
        txt = ctypes.create_unicode_buffer(512)
        u.GetWindowTextW(hwnd, txt, 512)
        if keyword.lower() in cls.value.lower() or keyword.lower() in txt.value.lower():
            r = RECT()
            u.GetWindowRect(hwnd, ctypes.byref(r))
            out.append((hwnd, cls.value, txt.value, r.left, r.top, r.right, r.bottom,
                        bool(u.IsWindowVisible(hwnd))))
        return True

    u.EnumChildWindows(parent, ENUMPROC(_cb), 0)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true", help="只列出窗口，不截图")
    ap.add_argument("--out", help="输出 PNG 路径")
    ap.add_argument("--crop", help="裁剪区域 L,T,R,B（屏幕物理像素）")
    ap.add_argument("--scale", type=float, default=1.0, help="裁剪后放大倍数（便于文档查看）")
    ap.add_argument("--keyword", default="traffic")
    args = ap.parse_args()

    if args.list or not args.out:
        print("== 命中关键字的顶层窗口 ==")
        for hwnd, cls, title, l, t, r, b, vis in list_windows(args.keyword):
            print("hwnd=%-10s class=%-34s vis=%-5s rect=(%d,%d,%d,%d) %dx%d  title=%r"
                  % (hwnd, cls, vis, l, t, r, b, r - l, b - t, title))
        for cn in ("Shell_TrayWnd", "Shell_SecondaryTrayWnd"):
            r = named_window(cn)
            if r:
                print("%s rect=(%d,%d,%d,%d) %dx%d" % (cn, r[0], r[1], r[2], r[3],
                                                       r[2] - r[0], r[3] - r[1]))
                kids = child_windows(cn, args.keyword)
                if kids:
                    print("  -- 子窗口 --")
                    for hwnd, cls, title, l, t, rr, b, vis in kids:
                        print("  hwnd=%-10s class=%-34s vis=%-5s rect=(%d,%d,%d,%d) %dx%d"
                              % (hwnd, cls, vis, l, t, rr, b, rr - l, b - t))
        if not args.out:
            return 0

    from PIL import ImageGrab

    img = ImageGrab.grab(all_screens=True)
    print("grabbed %dx%d" % img.size)

    if args.crop:
        box = tuple(int(x) for x in args.crop.split(","))
        img = img.crop(box)
        print("cropped -> %dx%d" % img.size)

    if args.scale and args.scale != 1.0:
        w, h = img.size
        img = img.resize((int(w * args.scale), int(h * args.scale)), 1)   # 1 = LANCZOS
        print("scaled -> %dx%d" % img.size)

    out = os.path.abspath(args.out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    img.save(out)
    print("saved %s (%d bytes)" % (out, os.path.getsize(out)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
