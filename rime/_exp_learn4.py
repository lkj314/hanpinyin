#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""判别性实验（第四轮·终局）：生产形态（enable_sentence: false）+ encoder.rules，
6 次连续单词提交 → 重开引擎 → 探测 ☯ 词组是否跨会话持久化。

判读：
  - WAL/ldb 出现 \x7fenc 条目，或重开后 probe 出 ☯ ⇒ 历史编码+持久化链路可用，
    根因 = 生成的词典缺 encoder.rules（可修，路线 A 成立）；
  - 依旧零条目且重开无 ☯ ⇒ 编码器拆词/落库环节损坏（路线 A 存疑，需再挖 encoder.cc）。
"""
import ctypes
import os
import shutil
import subprocess
import sys
import tempfile

SRC = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SRC)
import _diag_select as ds  # noqa: E402

SCHEMA = """# 判别实验 D（隔离目录）：生产形态 + rules
schema:
  schema_id: testenc
  name: "encoder test D"
  version: "1"

engine:
  processors:
    - ascii_composer
    - recognizer
    - key_binder
    - speller
    - punctuator
    - selector
    - navigator
    - express_editor
  segmentors:
    - ascii_segmentor
    - abc_segmentor
    - fallback_segmentor
  translators:
    - table_translator
  filters:
    - uniquifier

translator:
  dictionary: testenc
  enable_user_dict: true
  enable_encoder: true
  encode_commit_history: true
  enable_sentence: false
  max_phrase_length: 20
"""

DICT = """---
name: testenc
version: "1"
sort: by_weight
encoder:
  rules:
    - length_equal: 2
      formula: "AB"
    - length_equal: 3
      formula: "ABC"
...
가\tna\t10
나다\tde\t10
가나다\tnd\t10
"""


def main():
    d = tempfile.mkdtemp(prefix="rimetestD_")
    open(os.path.join(d, "testenc.schema.yaml"), "w", encoding="utf-8").write(SCHEMA)
    open(os.path.join(d, "testenc.dict.yaml"), "w", encoding="utf-8").write(DICT)
    open(os.path.join(d, "default.custom.yaml"), "w", encoding="utf-8").write(
        "patch:\n  schema_list:\n    - schema: testenc\n")
    shutil.copy(os.path.join(ds.REAL_USER, "installation.yaml"), os.path.join(d, "installation.yaml"))

    def boot():
        dll = ctypes.CDLL(ds.DLL)
        dll.RimeInitialize.argtypes = [ctypes.POINTER(ds.RimeTraits)]
        dll.RimeStartMaintenance.argtypes = [ctypes.c_bool]
        dll.RimeJoinMaintenanceThread.argtypes = []
        dll.RimeCreateSession.restype = ctypes.c_uint64
        S = ctypes.c_uint64
        dll.RimeSelectSchema.argtypes = [S, ctypes.c_char_p]
        dll.RimeSimulateKeySequence.argtypes = [S, ctypes.c_char_p]
        dll.RimeGetContext.argtypes = [S, ctypes.POINTER(ds.RimeContext)]
        dll.RimeFreeContext.argtypes = [ctypes.POINTER(ds.RimeContext)]
        dll.RimeGetCommit.argtypes = [S, ctypes.POINTER(ds.RimeCommit)]
        dll.RimeFreeCommit.argtypes = [ctypes.POINTER(ds.RimeCommit)]
        dll.RimeClearComposition.argtypes = [S]
        tr = ds.RimeTraits()
        tr.data_size = ctypes.sizeof(ds.RimeTraits) - ctypes.sizeof(ctypes.c_int)
        tr.shared_data_dir = ds.SHARED.encode("utf-8")
        tr.user_data_dir = d.encode("utf-8")
        tr.app_name = b"rime.encodertestD"
        dll.RimeInitialize(ctypes.byref(tr))
        dll.RimeStartMaintenance(True)
        dll.RimeJoinMaintenanceThread()
        sid = dll.RimeCreateSession()
        if not dll.RimeSelectSchema(sid, b"testenc"):
            print("[FATAL] 部署失败")
            return None, None
        return dll, sid

    def commit(dll, sid, code, pick="1"):
        dll.RimeSimulateKeySequence(sid, code.encode())
        dll.RimeSimulateKeySequence(sid, pick.encode())
        cm = ds.RimeCommit()
        cm.data_size = ctypes.sizeof(ds.RimeCommit) - ctypes.sizeof(ctypes.c_int)
        has = dll.RimeGetCommit(sid, ctypes.byref(cm))
        got = (cm.text or b"").decode("utf-8", "replace") if has else ""
        if has:
            dll.RimeFreeCommit(ctypes.byref(cm))
        print(f"   commit {code} 上屏【{got}】")

    def probe(dll, sid, code):
        dll.RimeSimulateKeySequence(sid, code.encode())
        ctx = ds.RimeContext()
        ctx.data_size = ctypes.sizeof(ds.RimeContext) - ctypes.sizeof(ctypes.c_int)
        dll.RimeGetContext(sid, ctypes.byref(ctx))
        n = ctx.menu.num_candidates
        cands = []
        if ctx.menu.candidates and n > 0:
            arr = ctypes.cast(ctx.menu.candidates, ctypes.POINTER(ds.RimeCandidate * n))[0]
            for i in range(n):
                t = (arr[i].text or b"").decode("utf-8", "replace")
                c = (arr[i].comment or b"").decode("utf-8", "replace")
                cands.append(t + (("[" + c + "]") if c else ""))
        dll.RimeFreeContext(ctypes.byref(ctx))
        dll.RimeClearComposition(sid)
        print(f"   probe {code:6s} 候选{cands[:4]}")

    def dump(tag):
        udb = os.path.join(d, "testenc.userdb")
        if not os.path.isdir(udb):
            print(f"   [{tag}] 无 userdb")
            return
        files = sorted(f for f in os.listdir(udb) if f.endswith((".log", ".ldb")))
        print(f"   [{tag}] 落库文件: {files}")
        dumper = os.path.join(SRC, "_dump_userdb.py")
        if os.path.exists(dumper):
            subprocess.run([sys.executable, dumper, "--raw", udb], check=False)

    print("== 会话 1：连续 6 次单词提交 ==")
    dll, sid = boot()
    if not dll:
        return 1
    for code in ("na", "de", "na", "de", "na", "de"):
        commit(dll, sid, code)
    dll.RimeFinalize()
    dump("会话1结束")

    print("== 会话 2：重开引擎（触发 recovery 压实）+ 探测跨会话持久化 ==")
    dll, sid = boot()
    if not dll:
        return 1
    for code in ("nade", "dena", "nd"):
        probe(dll, sid, code)
    dll.RimeFinalize()
    dump("会话2结束")

    shutil.rmtree(d, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
