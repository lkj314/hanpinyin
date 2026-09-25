# -*- coding: utf-8 -*-
"""候选面板体检工具：量化「打字流畅度」——每个输入的候选窗里，有多少是韩文、多少是噪音。

为什么需要它：体感上的「不流畅」必须拆成可测量的量。本脚本用 librime 原生 API
拉取**完整候选列表**，按 韩文/中文/日文/英文 分类统计，输出：
  - 候选位总数、韩文占比、中文占比
  - 韩文首次出现的位置（>page_size 意味着必须翻页）

安全设计（铁律）：只用临时 user_data_dir，只读拷贝用户已编译的 build/，
不触碰运行中的小狼毫、不改动任何输入法状态、不触发部署。

用法：
    python _bench_panel.py              # 跑内置样例（诊断当前已部署的方案）
    python _bench_panel.py nihao gank   # 测指定输入
典型产出（2026-09-25 实测）：
    当前方案：135 候选位里韩文 21%、中文 75%
    去掉 script_translator@cn 后：57 候选位里韩文 74%、中文 12%
"""
import ctypes
import os
import re
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _diag_select import (DLL, SHARED, REAL_USER, SRC, RimeTraits, RimeContext,  # noqa: E402
                          RimeCandidate, prepare_user_dir)

HANGUL = re.compile(r"[\uAC00-\uD7A3\u3130-\u318F\u1100-\u11FF]")
HAN = re.compile(r"[\u3400-\u4DBF\u4E00-\u9FFF\uF900-\uFAFF]")
KANA = re.compile(r"[\u3040-\u30FF]")


def kind(t):
    if HANGUL.search(t):
        return "K"          # 韩文（含 초성어）
    if KANA.search(t):
        return "J"          # 日文假名
    if HAN.search(t):
        return "C"          # 中文汉字
    return "E"              # 英文/数字


if __name__ == "__main__":
    pass


TESTS = [
    # 短码 / 单音节（最容易被音译层与中文冲洗）
    "ni", "wo", "hao", "bu", "ma", "jie", "si", "zai",
    # 日常高频
    "nihao", "xiexie", "zaijian", "duibuqi", "qingshaodeng",
    # 游戏核心
    "paiwei", "zenmeban", "wenzhu", "gank", "jiayou",
    # 情绪批次（长码）
    "beizhendui", "fansile", "weiqu", "haolei", "anweiwo",
    # 前缀（观测补全噪音）
    "zh", "we", "bie",
]


def main(fresh=False):
    user_dir = prepare_user_dir(fresh)
    dll = ctypes.CDLL(DLL)
    dll.RimeInitialize.argtypes = [ctypes.POINTER(RimeTraits)]
    dll.RimeCreateSession.restype = ctypes.c_uint64
    dll.RimeSelectSchema.argtypes = [ctypes.c_uint64, ctypes.c_char_p]
    dll.RimeSimulateKeySequence.argtypes = [ctypes.c_uint64, ctypes.c_char_p]
    dll.RimeGetContext.argtypes = [ctypes.c_uint64, ctypes.POINTER(RimeContext)]
    dll.RimeFreeContext.argtypes = [ctypes.POINTER(RimeContext)]
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
    dll.RimeInitialize(ctypes.byref(tr))
    dll.RimeStartMaintenance(True)
    dll.RimeJoinMaintenanceThread()
    sid = dll.RimeCreateSession()
    dll.RimeSelectSchema(sid, b"sino_mix")

    def snapshot():
        ctx = RimeContext()
        ctx.data_size = ctypes.sizeof(RimeContext) - ctypes.sizeof(ctypes.c_int)
        dll.RimeGetContext(sid, ctypes.byref(ctx))
        n = ctx.menu.num_candidates
        ps = ctx.menu.page_size
        cands = []
        if ctx.menu.candidates and n > 0:
            arr = ctypes.cast(ctx.menu.candidates, ctypes.POINTER(RimeCandidate * n))[0]
            for i in range(n):
                cands.append((arr[i].text or b"").decode("utf-8", "replace"))
        dll.RimeFreeContext(ctypes.byref(ctx))
        return cands, ps

    print("%-14s %5s %5s %-42s" % ("输入", "候选", "页大小", "韩文首次出现位置 / 前 8 个候选"))
    print("-" * 108)
    rows = []
    for seq in TESTS:
        dll.RimeSimulateKeySequence(sid, seq.encode("utf-8"))
        cands, ps = snapshot()
        dll.RimeClearComposition(sid)
        ks = [kind(c) for c in cands]
        first = ks.index("K") + 1 if "K" in ks else None
        head = [("%s:%s" % (k, t)) for k, t in list(zip(ks, cands))[:8]]
        print("%-14s %5d %5d  K#%-3s %s" % (seq, len(cands), ps, first, " ".join(head)))
        rows.append((seq, len(cands), ps, first, ks.count("C"), ks.count("J"), ks.count("K"), ks.count("E")))

    dll.RimeFinalize()
    shutil.rmtree(user_dir, ignore_errors=True)

    # 汇总
    print("\n" + "=" * 108)
    print("%-14s %6s %6s %6s %6s %8s %10s" % ("输入", "中文", "日文", "韩文", "英文", "韩文位次", "首屏是否有韩文"))
    print("-" * 108)
    tot_k_delay, n_ok, n_bad = 0, 0, 0
    for seq, tot, ps, first, nc, nj, nk, ne in rows:
        in_page = (first is not None and first <= ps)
        ps_eff = ps if ps > 0 else 5
        if in_page:
            n_ok += 1
        else:
            n_bad += 1
        if first:
            tot_k_delay += first - 1
        print("%-14s %6d %6d %6d %6d %8s %10s" % (seq, nc, nj, nk, ne, first or "-", "是" if in_page else "★★ 需翻页"))
    print("-" * 108)
    print("首屏能看到韩文: %d/%d ；韩文平均被挤到第 %.1f 位（前面 %.1f 条非韩文）"
          % (n_ok, len(rows), (tot_k_delay / max(n_ok, 1)) + 1, tot_k_delay / max(n_ok, 1)))

if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--fresh"]
    main(fresh=("--fresh" in sys.argv)) or None
    sys.exit(0)
