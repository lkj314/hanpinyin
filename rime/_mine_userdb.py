#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""userdb 挖矿（T1-3 固化管线的首环）：把输入法的真实学习数据变成可决策清单。

只读：复用 _dump_userdb 的解析器，不写入、不加锁、不碰输入法。

输出四块：
  1. 概览：学习条目数 / 提交总量 / 最后写入时间
  2. 高频榜：按「词」聚合（跨码合并），标注该词在静态词典中的最高权重
     —— 高频且静态权重低 = 固化候选（进 extra/shortcodes，防 userdb 清零掉排序）
  3. 孤儿学习：学习条目 (码,词) 不在当前静态词典 —— 词典改版残留，
     会在同码下永久冒出（如 shengqi→生气），列入处置清单
  4. 污染检查：非韩文/非拉丁输出（中文、假名）

用法：  python _mine_userdb.py [userdb目录]     # 默认挖真实 %APPDATA%\\Rime\\hanpinyin.userdb
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import _dump_userdb as du  # noqa: E402

REAL = du.REAL_USERDB
DICT = os.path.join(HERE, "hanpinyin.dict.yaml")

HAN_RE = re.compile(r"[\u3400-\u4DBF\u4E00-\u9FFF\uF900-\uFAFF]")
KANA_RE = re.compile(r"[\u3040-\u30FF]")
VAL_RE = re.compile(r"c=(\d+)\s+d=([\d.]+)\s+t=(\d+)")


def load_static():
    """静态词典：bycode[code][text]=weight；top[code]=(weight,text)。"""
    bycode, top = {}, {}
    started = False
    with open(DICT, encoding="utf-8") as f:
        for ln in f:
            s = ln.rstrip("\n")
            if s.strip() == "...":
                started = True
                continue
            if not started or not s.strip() or s.startswith("#"):
                continue
            parts = s.split("\t")
            if len(parts) != 3 or not parts[2].strip().isdigit():
                continue
            t, c, w = parts[0], parts[1], int(parts[2])
            bycode.setdefault(c, {})[t] = w
            if c not in top or w > top[c][0]:
                top[c] = (w, t)
    return bycode, top


def main():
    udb = sys.argv[1] if len(sys.argv) > 1 else REAL
    if not os.path.isdir(udb):
        print(f"[FATAL] userdb 目录不存在：{udb}")
        return 1
    merged = du.load_merged(udb)
    bycode, top = load_static()

    entries = []  # (code, text, commits, dee, tick)
    for k, (v, _src) in merged.items():
        s = k.decode("utf-8", "replace")
        if s.startswith("\x01") or s.startswith("\x7fenc\x1f") or "\t" not in s:
            continue
        code, text = s.split("\t", 1)
        code = code.strip()
        m = VAL_RE.match((v or b"").decode("utf-8", "replace"))
        if not m:
            continue
        entries.append((code, text, int(m.group(1)), float(m.group(2)), int(m.group(3))))

    if not entries:
        print("# 无学习条目（userdb 为空或尚未使用）")
        return 0

    last_mtime = max(os.path.getmtime(os.path.join(udb, f))
                     for f in os.listdir(udb) if f.endswith((".log", ".ldb")))
    import datetime
    total_commits = sum(e[2] for e in entries)
    max_tick = max((e[4] for e in entries), default=0)
    print("=" * 66)
    print(f"1. 概览：学习条目 {len(entries)} 条 | 提交总量 {total_commits} | 提交计数 tick={max_tick}")
    print(f"   （文件最后变动 {datetime.datetime.fromtimestamp(last_mtime):%Y-%m-%d %H:%M}，"
          f"含无提交的只打开；是否有新提交看 tick 是否增长）")

    # 2. 高频榜（按词聚合）
    per_word = {}
    for code, text, c, d, t in entries:
        w = per_word.setdefault(text, {"c": 0, "best": None, "codes": []})
        w["c"] += c
        w["codes"].append((c, code))
        if w["best"] is None or c > w["best"][0]:
            w["best"] = (c, code)
    print("=" * 66)
    print("2. 高频榜（按词聚合，Top 15；静态权重=该词在其最佳码下的词典权重）")
    for text, w in sorted(per_word.items(), key=lambda kv: -kv[1]["c"])[:15]:
        best_c, best_code = w["best"]
        static_w = bycode.get(best_code, {}).get(text)
        note = f"静态权重 {static_w}" if static_w is not None else "静态词典无此对"
        flag = "  <<< 固化候选（userdb 清零即掉位）" if static_w is None or static_w < 50 else ""
        print(f"   {w['c']:>3} 次  {text}  (最佳码 {best_code}, {note}){flag}")

    # 3. 孤儿学习：(码,词) 不在静态词典
    orphans = [(c, t, n) for c, t, n, _d, _t in entries
               if t not in bycode.get(c, {})]
    print("=" * 66)
    print(f"3. 孤儿学习（学习对不在当前词典，同码会永久冒出）：{len(orphans)} 条")
    for code, text, c in sorted(orphans, key=lambda x: -x[2]):
        print(f"   {c:>3} 次  {code} -> {text}")

    # 4. 污染检查
    dirty = [(c, t, n) for c, t, n, _d, _t in entries
             if HAN_RE.search(t) or KANA_RE.search(t)]
    print("=" * 66)
    print(f"4. 污染（中文/假名混入输出）：{len(dirty)} 条")
    for code, text, c in dirty:
        print(f"   {c:>3} 次  {code} -> {text}  <<< 应处置")
    print("=" * 66)
    print("处置选项（见蓝图 T1-5）：容忍 / 重置（删 hanpinyin.userdb + 重新部署）/ 精修（停服离线编辑）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
