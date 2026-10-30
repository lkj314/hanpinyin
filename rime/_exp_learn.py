#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""自学习机制实验：在隔离环境实测 librime 的用户词典学习与 ☯ 长句编码行为。

验证三件事（全部只读用户数据、临时目录实验，不碰真实输入法）：
  A. 选词记忆重排：选过一次的候选，再次输入同码时是否排到首位；
  B. ☯ 长句编码：连续上屏后是否生成编码词组；用什么码能召回
     （整码拼接？前缀？首字母？）——验证 HANDOVER 里"敲开头字母整句调出"的说法；
  C. userdb 实际写入内容（含 \x7fenc 编码条目的真实格式）。

复用 _diag_select 的隔离设计：临时 user_data_dir + 只读拷贝 build/。
"""
import ctypes
import os
import shutil
import struct
import subprocess
import sys

SRC = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SRC)
import _diag_select as ds  # noqa: E402  复用 Rime 结构体与隔离目录准备


def snapshot(dll, sid):
    ctx = ds.RimeContext()
    ctx.data_size = ctypes.sizeof(ds.RimeContext) - ctypes.sizeof(ctypes.c_int)
    dll.RimeGetContext(sid, ctypes.byref(ctx))
    pre = ctx.composition.preedit.decode("utf-8", "replace") if ctx.composition.preedit else ""
    n = ctx.menu.num_candidates
    cands = []
    if ctx.menu.candidates and n > 0:
        arr = ctypes.cast(ctx.menu.candidates, ctypes.POINTER(ds.RimeCandidate * n))[0]
        for i in range(n):
            t = (arr[i].text or b"").decode("utf-8", "replace")
            c = (arr[i].comment or b"").decode("utf-8", "replace")
            cands.append(t + ("☯" if "☯" in c or "☯" in t else ""))
    dll.RimeFreeContext(ctypes.byref(ctx))
    return pre, cands


def commit_one(dll, sid, code, pick):
    """输入 code，按 pick 键选词上屏，返回上屏文本。"""
    dll.RimeSimulateKeySequence(sid, code.encode("utf-8"))
    pre, cands = snapshot(dll, sid)
    dll.RimeSimulateKeySequence(sid, pick.encode("utf-8"))
    cm = ds.RimeCommit()
    cm.data_size = ctypes.sizeof(ds.RimeCommit) - ctypes.sizeof(ctypes.c_int)
    has = dll.RimeGetCommit(sid, ctypes.byref(cm))
    committed = (cm.text or b"").decode("utf-8", "replace") if has else ""
    if has:
        dll.RimeFreeCommit(ctypes.byref(cm))
    return pre, cands, committed


def peek(dll, sid, code):
    dll.RimeSimulateKeySequence(sid, code.encode("utf-8"))
    pre, cands = snapshot(dll, sid)
    dll.RimeClearComposition(sid)
    return pre, cands


def main():
    user_dir = ds.prepare_user_dir(fresh=True)  # 仓库最新 v2.4 YAML 在临时目录重新编译
    dll = ctypes.CDLL(ds.DLL)
    dll.RimeInitialize.argtypes = [ctypes.POINTER(ds.RimeTraits)]
    dll.RimeCreateSession.restype = ctypes.c_uint64
    # 坑（09-11 记录在案）：所有带 session id 的函数必须显式 c_uint64，否则 OverflowError
    S = ctypes.c_uint64
    dll.RimeSelectSchema.argtypes = [S, ctypes.c_char_p]
    dll.RimeSimulateKeySequence.argtypes = [S, ctypes.c_char_p]
    dll.RimeGetContext.argtypes = [S, ctypes.POINTER(ds.RimeContext)]
    dll.RimeFreeContext.argtypes = [ctypes.POINTER(ds.RimeContext)]
    dll.RimeGetCommit.argtypes = [S, ctypes.POINTER(ds.RimeCommit)]
    dll.RimeFreeCommit.argtypes = [ctypes.POINTER(ds.RimeCommit)]
    dll.RimeClearComposition.argtypes = [S]
    dll.RimeStartMaintenance.argtypes = [ctypes.c_bool]
    dll.RimeJoinMaintenanceThread.argtypes = []
    tr = ds.RimeTraits()
    tr.data_size = ctypes.sizeof(ds.RimeTraits) - ctypes.sizeof(ctypes.c_int)
    tr.shared_data_dir = ds.SHARED.encode("utf-8")
    tr.user_data_dir = user_dir.encode("utf-8")
    tr.app_name = b"rime.experiment"
    dll.RimeInitialize(ctypes.byref(tr))
    dll.RimeStartMaintenance(True)
    dll.RimeJoinMaintenanceThread()
    sid = dll.RimeCreateSession()
    if not dll.RimeSelectSchema(sid, b"sino_mix"):
        print("[FATAL] 隔离目录无法选中 sino_mix")
        return 1

    print("=" * 62)
    print("A. 选词记忆重排实验（zmb 有两个候选：어떡해 / 어떡해요）")
    pre, cands, committed = commit_one(dll, sid, "zmb", "2")  # 故意选第二候选 어떡해요
    print(f"   输入 zmb 候选序: {cands[:4]}  按2上屏=【{committed}】")
    pre, cands = peek(dll, sid, "zmb")
    print(f"   再输 zmb 候选序: {cands[:4]}   <- 学到的词是否跳到首位？")

    print("=" * 62)
    print("B. ☯ 长句编码实验：连续上屏 안녕하세요(nihao) + 어떡해(zmb)")
    for code in ("nihao", "zmb"):
        pre, cands, committed = commit_one(dll, sid, code, "1")
        print(f"   上屏 {code} -> 【{committed}】")
    print("   -- 召回探测（编码后缀链：어떡해요 / 안녕하세요어떡해요；码=reverse 反查规范码拼接）--")
    for probe, tag in (("nihaozmb", "实际敲的码拼接"),
                       ("nihaozenmeban", "规范码拼接(全链)"),
                       ("zenmeban", "规范码(单词后缀)"),
                       ("nihaoz", "前缀")):
        pre, cands = peek(dll, sid, probe)
        hit = [c for c in cands if "안녕" in c and ("어떡" in c or "☯" in c)]
        print(f"   输入 {probe:14s}({tag}) -> {'命中: ' + ' | '.join(hit) if hit else '无整句候选'} 候选序: {cands[:3]}")

    print("=" * 62)
    print("C. 三连上屏构造后缀链：잘자(wanan) + 졌어(shule) + 또 졌어(youshule)")
    for code in ("wanan", "shule", "youshule"):
        pre, cands, committed = commit_one(dll, sid, code, "1")
        print(f"   上屏 {code} -> 【{committed}】")
    for probe, tag in (("wananshuleyoushule", "全链规范码"),
                       ("shuleyoushule", "后两段"),
                       ("youshule", "最后一段")):
        pre, cands = peek(dll, sid, probe)
        hit = [c for c in cands if ("잘자" in c or "졌어" in c) and ("☯" in c or "또" in c)]
        print(f"   输入 {probe:20s}({tag}) -> {'命中: ' + ' | '.join(hit) if hit else '无'} 候选序: {cands[:3]}")

    dll.RimeFinalize()

    print("=" * 62)
    print("D. 解析临时 userdb（隔离目录学习数据的真实落库格式，直接读 WAL/ldb）")
    udb = os.path.join(user_dir, "hanpinyin.userdb")
    if os.path.isdir(udb):
        files = sorted(os.listdir(udb))
        print("   文件:", files)
        parser = os.path.join(os.environ.get("TEMP", "."), "hpdb_inspect")
        import glob
        wal = os.path.join(parser, "parse_wal.py")
        targets = [os.path.join(udb, f) for f in files if f.endswith((".log", ".ldb"))]
        if os.path.exists(wal) and targets:
            subprocess.run([sys.executable, wal] + targets, check=False)
        else:
            print("   解析器缺失")
    shutil.rmtree(user_dir, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
