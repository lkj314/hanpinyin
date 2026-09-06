#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HanPinyin 词库回归校验（每次改词库后必跑）
==========================================
与 validate_rime.py（方案结构校验）互补，本脚本管数据质量：

  A. 数据源完整性：main_dict.json / phrases.json
     - JSON 可解析、无 BOM
     - 拼音格式合法（空格分隔音节、全小写字母）
     - 同一拼音键内候选韩文不重复、词频降序
     - phrases 整句韩文不重复
  B. 构建覆盖：数据源每条 (词条, 规范码) 必须出现在生成的 hanpinyin.dict.yaml
  C. 回归点：一批"必须能打出来"的码 -> 期望词（含模糊/简拼/新增批次抽样）

用法：  python build_dict.py && python verify_regression.py
结果：  全部通过打印 PASS 并退出 0；任何失败打印 FAIL 并退出 1。
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_dict  # noqa: E402  复用 split_pinyin / expand

ROOT = os.path.dirname(HERE)
MAIN = os.path.join(ROOT, "data", "main_dict.json")
PHRASES = os.path.join(ROOT, "data", "phrases.json")
DICT = os.path.join(HERE, "hanpinyin.dict.yaml")

# 回归点：输入码 -> 期望出现在该码候选里的词
# （改动模糊音/简拼生成逻辑或删词后，这里必须全绿）
REGRESSION = [
    ("nihao", "안녕하세요"),            # 规范码
    ("lihao", "안녕하세요"),            # 模糊 n↔l
    ("nh", "안녕하세요"),               # 全缩简拼
    ("nih", "안녕하세요"),              # 混拼
    ("dbq", "죄송합니다"),              # 对不起简拼
    ("xx", "감사합니다"),               # 谢谢简拼
    ("zhongdan", "미드"),               # LoL 中单
    ("male", "지쳤어"),                 # 麻了
    ("paiwei", "랭크"),                 # 2026-09-06 新增：排位
    ("shenglv", "승률"),                # 胜率
    ("chaita", "타 부수자"),            # 拆塔
    ("xiaguxianfeng", "전령"),          # 峡谷先锋
    ("shuijing", "억제기"),             # 水晶
    ("jiawohaoyou", "친추 해 주세요"),  # 新增整句：加我好友
    ("wobuhuihanyu", "한국어 못해요"),  # 新增整句：我不会韩语
]

PY_RE = __import__("re").compile(r"^[a-z]+$")


def fail_list():
    return []


def check_bom(path, fails):
    with open(path, "rb") as f:
        head = f.read(3)
    if head == b"\xef\xbb\xbf":
        fails.append("BOM: %s 带 UTF-8 BOM（必须无 BOM）" % path)


def check_main(fails, warns):
    m = json.load(open(MAIN, encoding="utf-8"))
    for e in m:
        py = e.get("pinyin", "")
        sylls = py.split()
        if not sylls or any(not PY_RE.match(s) for s in sylls):
            fails.append("main_dict: 拼音格式非法 %r" % py)
            continue
        cands = e.get("candidates", [])
        texts = [c[0] for c in cands]
        if len(texts) != len(set(texts)):
            fails.append("main_dict: %r 键内候选重复: %s" % (py, texts))
        ws = [int(c[1]) for c in cands]
        if ws != sorted(ws, reverse=True):
            fails.append("main_dict: %r 词频未降序: %s" % (py, ws))
        if not cands:
            warns.append("main_dict: %r 无候选" % py)
    return m


def check_phrases(fails):
    p = json.load(open(PHRASES, encoding="utf-8"))
    seen = {}
    for e in p:
        py = e.get("pinyin", "")
        sylls = py.split()
        if not sylls or any(not PY_RE.match(s) for s in sylls):
            fails.append("phrases: 拼音格式非法 %r" % py)
        ko = e.get("korean", "")
        if ko in seen:
            fails.append("phrases: 整句韩文重复 %r (%r / %r)" % (ko, seen[ko], py))
        seen[ko] = py
    return p


def load_dict_bycode():
    bycode = {}
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
            if len(parts) != 3:
                continue
            bycode.setdefault(parts[1], set()).add(parts[0])
    return bycode


def main():
    fails, warns = fail_list(), []

    check_bom(MAIN, fails)
    check_bom(PHRASES, fails)
    m = check_main(fails, warns)
    p = check_phrases(fails)

    print("数据源: main=%d 条 | phrases=%d 条" % (len(m), len(p)))

    bycode = load_dict_bycode()
    print("生成词库: %d 个不同码" % len(bycode))

    # B. 构建覆盖
    missing = 0
    for e in m:
        canon = "".join(build_dict.split_pinyin(e["pinyin"]))
        for text, _w in e["candidates"]:
            if text not in bycode.get(canon, ()):  # (text, canon) 必须在产物中
                fails.append("覆盖缺失: %r (码 %r) 不在生成词库" % (text, canon))
                missing += 1
    for e in p:
        canon = "".join(build_dict.split_pinyin(e["pinyin"]))
        if e["korean"] not in bycode.get(canon, ()):
            fails.append("覆盖缺失: %r (码 %r) 不在生成词库" % (e["korean"], canon))
            missing += 1
    if not missing:
        print("构建覆盖: 全部数据源词条均已进入生成词库 ✔")

    # C. 回归点
    reg_bad = 0
    for code, expect in REGRESSION:
        got = bycode.get(code, set())
        if expect in got:
            print("  回归 %s -> %s ✔" % (code, expect))
        else:
            print("  回归 %s -> 期望 %s，实际 %s ✘" % (code, expect, sorted(got)[:4]))
            fails.append("回归点失败: %s -> %s" % (code, expect))
            reg_bad += 1

    for w in warns:
        print("  [WARN]", w)
    print("\nVERIFY", "PASS ✅" if not fails else "FAIL ❌ (%d 失败)" % len(fails))
    raise SystemExit(0 if not fails else 1)


if __name__ == "__main__":
    main()
