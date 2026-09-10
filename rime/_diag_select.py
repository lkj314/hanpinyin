#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""端到端诊断工具：用 librime 原生 API 验证「打得出来、且选得中能上屏」。

为什么需要它：候选「能显示」≠「能选中上屏」。本脚本模拟按键后按 1 选词，
再读 RimeGetCommit，用「有没有真的上屏」做判据，而不是靠猜。

安全设计（铁律）：
  * user_data_dir 用**临时目录**，绝不写用户的 %APPDATA%\\Rime；
  * 只读拷贝用户已编译的 build/ 到临时目录，免去重新部署（也不触发任何部署动作）；
  * 全程不触碰运行中的小狼毫、不改动任何输入法状态。

用法：
    python _diag_select.py                      # 跑内置样例（用当前已部署的编译产物）
    python _diag_select.py zmb kanlai           # 测指定码（空格分隔）
    python _diag_select.py --fresh kanlai       # 用仓库最新词典重新编译后再测（验证未部署的新词）
判定：
    上屏=【韩文】      -> 正常
    上屏=（无）且 preedit 变成 "zmb1" -> 该拼写没被识别成可选段（缺陷，需查 schema/词典）
"""
import ctypes
import os
import shutil
import sys
import tempfile

DLL = r"C:\Program Files\Rime\weasel-0.17.4\rime.dll"
SHARED = r"C:\Program Files\Rime\weasel-0.17.4\data"
REAL_USER = r"C:\Users\Administrator\AppData\Roaming\Rime"
SRC = os.path.dirname(os.path.abspath(__file__))


class RimeTraits(ctypes.Structure):
    _fields_ = [("data_size", ctypes.c_int), ("shared_data_dir", ctypes.c_char_p),
                ("user_data_dir", ctypes.c_char_p), ("distribution_name", ctypes.c_char_p),
                ("distribution_code_name", ctypes.c_char_p), ("distribution_version", ctypes.c_char_p),
                ("app_name", ctypes.c_char_p), ("modules", ctypes.POINTER(ctypes.c_char_p)),
                ("min_log_level", ctypes.c_int), ("log_dir", ctypes.c_char_p),
                ("prebuilt_data_dir", ctypes.c_char_p), ("staging_dir", ctypes.c_char_p)]


class RimeCandidate(ctypes.Structure):
    _fields_ = [("text", ctypes.c_char_p), ("comment", ctypes.c_char_p), ("reserved", ctypes.c_void_p)]


class RimeMenu(ctypes.Structure):
    _fields_ = [("page_size", ctypes.c_int), ("page_no", ctypes.c_int), ("is_last_page", ctypes.c_int),
                ("highlighted_candidate_index", ctypes.c_int), ("num_candidates", ctypes.c_int),
                ("candidates", ctypes.POINTER(RimeCandidate)), ("select_keys", ctypes.c_char_p)]


class RimeComposition(ctypes.Structure):
    _fields_ = [("length", ctypes.c_int), ("cursor_pos", ctypes.c_int), ("sel_start", ctypes.c_int),
                ("sel_end", ctypes.c_int), ("preedit", ctypes.c_char_p)]


class RimeContext(ctypes.Structure):
    _fields_ = [("data_size", ctypes.c_int), ("composition", RimeComposition), ("menu", RimeMenu),
                ("commit_text_preview", ctypes.c_char_p), ("select_labels", ctypes.POINTER(ctypes.c_char_p))]


class RimeCommit(ctypes.Structure):
    _fields_ = [("data_size", ctypes.c_int), ("text", ctypes.c_char_p)]


def prepare_user_dir(fresh=False):
    """建临时用户目录：放当前仓库的 schema/词典 + 只读拷贝用户已编译的 build/。

    fresh=False（默认）：直接复用用户已编译的 build/ —— 诊断「当前部署状态」。
    fresh=True：删掉拷贝来的 hanpinyin/sino_mix 编译产物，强制用仓库最新 YAML 重新编译 ——
               可在「用户还没重新部署」的情况下验证新词库（编译只在临时目录进行）。
    """
    d = tempfile.mkdtemp(prefix="rimediag_")
    for f in ("sino_mix.schema.yaml", "hanpinyin.dict.yaml"):
        shutil.copy(os.path.join(SRC, f), os.path.join(d, f))
    shutil.copy(os.path.join(REAL_USER, "installation.yaml"), os.path.join(d, "installation.yaml"))
    # 部署器只编译 schema_list 里登记的方案：必须带上用户这份登记 sino_mix 的补丁
    if os.path.exists(os.path.join(REAL_USER, "default.custom.yaml")):
        shutil.copy(os.path.join(REAL_USER, "default.custom.yaml"), os.path.join(d, "default.custom.yaml"))
    if os.path.isdir(os.path.join(REAL_USER, "build")):
        shutil.copytree(os.path.join(REAL_USER, "build"), os.path.join(d, "build"))
        if fresh:
            for f in os.listdir(os.path.join(d, "build")):
                if f.startswith(("hanpinyin.", "sino_mix.")):
                    os.remove(os.path.join(d, "build", f))
    return d


def main(codes, fresh=False):
    user_dir = prepare_user_dir(fresh)
    dll = ctypes.CDLL(DLL)
    dll.RimeInitialize.argtypes = [ctypes.POINTER(RimeTraits)]
    dll.RimeCreateSession.restype = ctypes.c_uint64
    dll.RimeSelectSchema.argtypes = [ctypes.c_uint64, ctypes.c_char_p]
    dll.RimeSimulateKeySequence.argtypes = [ctypes.c_uint64, ctypes.c_char_p]
    dll.RimeGetContext.argtypes = [ctypes.c_uint64, ctypes.POINTER(RimeContext)]
    dll.RimeFreeContext.argtypes = [ctypes.POINTER(RimeContext)]
    dll.RimeGetCommit.argtypes = [ctypes.c_uint64, ctypes.POINTER(RimeCommit)]
    dll.RimeFreeCommit.argtypes = [ctypes.POINTER(RimeCommit)]
    dll.RimeClearComposition.argtypes = [ctypes.c_uint64]
    dll.RimeStartMaintenance.argtypes = [ctypes.c_bool]
    dll.RimeJoinMaintenanceThread.argtypes = []

    tr = RimeTraits()
    tr.data_size = ctypes.sizeof(RimeTraits) - ctypes.sizeof(ctypes.c_int)
    tr.shared_data_dir = SHARED.encode("utf-8")
    tr.user_data_dir = user_dir.encode("utf-8")
    tr.app_name = b"rime.diag"
    tr.distribution_name = b"Rime"
    tr.distribution_code_name = b"weasel"
    tr.distribution_version = b"0.17.4"
    dll.RimeInitialize(ctypes.byref(tr))          # 新版 librime 返回 void
    dll.RimeStartMaintenance(True)
    dll.RimeJoinMaintenanceThread()
    sid = dll.RimeCreateSession()
    if not dll.RimeSelectSchema(sid, b"sino_mix"):
        print("[FATAL] 无法选中 sino_mix 方案（临时目录部署失败？）")
        shutil.rmtree(user_dir, ignore_errors=True)
        return 1

    def snapshot():
        ctx = RimeContext()
        ctx.data_size = ctypes.sizeof(RimeContext) - ctypes.sizeof(ctypes.c_int)
        dll.RimeGetContext(sid, ctypes.byref(ctx))
        pre = ctx.composition.preedit.decode("utf-8", "replace") if ctx.composition.preedit else ""
        n = ctx.menu.num_candidates
        cands = []
        if ctx.menu.candidates and n > 0:
            arr = ctypes.cast(ctx.menu.candidates, ctypes.POINTER(RimeCandidate * n))[0]
            for i in range(n):
                cands.append((arr[i].text or b"").decode("utf-8", "replace"))
        dll.RimeFreeContext(ctypes.byref(ctx))
        return pre, cands

    bad = 0
    for seq in codes:
        dll.RimeSimulateKeySequence(sid, seq.encode("utf-8"))
        pre, cands = snapshot()
        dll.RimeSimulateKeySequence(sid, b"1")
        cm = RimeCommit()
        cm.data_size = ctypes.sizeof(RimeCommit) - ctypes.sizeof(ctypes.c_int)
        has = dll.RimeGetCommit(sid, ctypes.byref(cm))
        committed = (cm.text or b"").decode("utf-8", "replace") if has else ""
        if has:
            dll.RimeFreeCommit(ctypes.byref(cm))
        pre2, _ = snapshot()
        ok = bool(committed)
        if not ok:
            bad += 1
        print(f"[{seq}] preedit='{pre}' 候选: " + " | ".join(cands[:5]))
        print(f"    按1 -> 上屏={'【'+committed+'】' if ok else '（无）'}  preedit='{pre2}'  {'OK' if ok else '<<< 无法选中'}")
        dll.RimeClearComposition(sid)

    dll.RimeFinalize()
    shutil.rmtree(user_dir, ignore_errors=True)
    print("\n结论:", "全部可选中上屏 ✅" if bad == 0 else f"{bad} 个码无法上屏 ❌")
    return 1 if bad else 0


DEFAULT = ["nihao", "zmb", "zenmeban", "dbq", "nh", "kanlai", "guoran", "taijiannanle"]

if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--fresh"]
    sys.exit(main(args or DEFAULT, fresh=("--fresh" in sys.argv)))
