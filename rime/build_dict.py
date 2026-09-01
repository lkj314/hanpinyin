#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HanPinyin -> Rime(小狼毫) 词库生成器
=====================================
读取 data/main_dict.json + data/phrases.json（项目唯一数据源），
合并 rime/extra_phrases.txt 里策划的多语言常用词条，
生成 Rime 词库 sino_mix.dict.yaml。

相比旧版，本脚本额外为每个词条生成：
  * 逐音节模糊音变体码（zh↔z、ang↔an、n↔l、f↔h …，每个音节都生效）
  * 简拼 / 混拼码（如 nihao -> nh / nih）

这样做的好处：韩文词典的拼音码是【连写】的（nihao / buhaoyisi），
小狼毫 speller.algebra 的模糊音规则只锚定整串开头、对连写码几乎无效；
由本脚本在生成阶段把模糊/简拼码直接写进词典，才能对多音节韩文词真正生效。

后续更新词库：改 data/*.json 或 rime/extra_phrases.txt -> 重跑本脚本 -> 小狼毫「重新部署」。

用法：
    python build_dict.py
"""

import os
import sys
import json
import itertools

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # HanPinyin/
RIME_DIR = os.path.dirname(os.path.abspath(__file__))               # HanPinyin/rime/
DATA_DIR = os.path.join(ROOT, "data")
OUT = os.path.join(RIME_DIR, "hanpinyin.dict.yaml")
EXTRA = os.path.join(RIME_DIR, "extra_phrases.txt")

MAIN = os.path.join(DATA_DIR, "main_dict.json")
PHRASES = os.path.join(DATA_DIR, "phrases.json")


# ---------------------------------------------------------------------------
# 拼音音节拆分
# ---------------------------------------------------------------------------
# 数据源里 data/*.json 的拼音【已用空格分隔音节】（"ni hao"）；
# extra_phrases.txt 的拼音是【连写】的（"nihao"）。两种都要支持。
# 连写时按"有效音节表"做最长匹配贪婪拆分；有效音节表 = 从 data 引导 + 内置常用音节兜底。

# 内置常用音节兜底表（覆盖日常及本项目的游戏/聊天用语；与 data 引导集合并后用于连写拆分）
_HARDCODED_SYLLABLES = set("""
a o e ai ei ao ou an en ang eng er
yi ya yan yang yao ye yin ying yong yo yu yue yuan yun
wu wa wai wan wang wei wen weng wo
ba bo bai bei bao bu ban ben bang beng bi bia bie biao bian bin bing
pa po pai pei pao pou pu pan pen pang peng pi pie piao pian pin ping
ma mo me mai mei mao mou mu man men mang meng mi mie miao miu mian min ming
fa fo fei fou fu fan fen fang feng
da de dai dao dou du duan dun dui dang deng di dia die diao diu dian ding
ta te tai tao tou tu tuan tun tui tang teng ti tie tiao tian ting
na ne nai nei nao nou nu nuan nun nv ni nia nie niao niu nian nin niang ning
la le lai lei lao lou lu luan lun lv li lia lie liao liu lian lin liang ling
ga ge gai gei gao gou gu gua guo guai gui guan gun guang gang geng gong
ka ke kai kao kou ku kua kuo kuai kui kuan kun kuang kang keng kong
ha he hai hei hao hou hu hua huo huai hui huan hun huang hang heng hong
ji jia jie jiao jiu jian jin jiang jing ju jue juan jun
qi qia qie qiao qiu qian qin qiang qing qu que quan qun
xi xia xie xiao xiu xian xin xiang xing xu xue xuan xun
zhi zha zhe zhai zhao zhou zhu zhua zhuo zhuai zhui zhuan zhun zhuang zhang zheng zhong
chi cha che chai chao chou chu chua chuo chuai chui chuan chun chuang chang cheng chong
shi sha she shai shao shou shu shua shuo shuai shui shuan shun shuang shang sheng
ri ra re rao rou ru rua ruo rui ruan run rong rang reng
zi za ze zai zao zou zu zuan zun zui zang zeng zong
ci ca ce cai cao cou cu cuan cun cui cang ceng cong
si sa se sai sou su suan sun sui sang seng song
""".split())

# 从 data/*.json 引导出已正确分词的音节，作为连写拆分的权威词表
def _bootstrap_vocab():
    vocab = set()
    for fn in (MAIN, PHRASES):
        if not os.path.exists(fn):
            continue
        try:
            arr = json.load(open(fn, encoding="utf-8"))
        except Exception:
            continue
        for e in arr:
            py = (e.get("pinyin") or "").lower()
            for s in py.split():
                if s:
                    vocab.add(s)
    return vocab

VOCAB = _HARDCODED_SYLLABLES | _bootstrap_vocab()


def split_pinyin(raw):
    """拼音 -> 音节列表。空格分隔的按空格切；连写的按有效音节表贪婪拆分。"""
    raw = (raw or "").strip().lower()
    if not raw:
        return []
    if " " in raw:
        return [s for s in raw.split() if s]
    return _greedy_split(raw, VOCAB)


def _greedy_split(code, vocab):
    out = []
    i, n = 0, len(code)
    while i < n:
        matched = None
        for L in range(min(6, n - i), 0, -1):
            if code[i:i + L] in vocab:
                matched = code[i:i + L]
                break
        if matched:
            out.append(matched)
            i += len(matched)
        else:
            out.append(code[i])   # 兜底：取单字符（极端情况，通常不应触发）
            i += 1
    return out


# ---------------------------------------------------------------------------
# 模糊音：在"声母 / 韵母"两个维度上分别做音变，再组合
# ---------------------------------------------------------------------------
_INITIALS = ["zh", "ch", "sh", "b", "p", "m", "f", "d", "t", "n", "l",
             "g", "k", "h", "j", "q", "x", "r", "z", "c", "s", ""]

_INITIAL_PAIRS = [("zh", "z"), ("ch", "c"), ("sh", "s"),
                  ("n", "l"), ("r", "l"), ("f", "h")]

_FINAL_PAIRS = [("ang", "an"), ("eng", "en"), ("ing", "in"),
                ("iang", "ian"), ("uang", "uan"), ("ong", "on")]


def _split_initial(syll):
    for ini in _INITIALS:
        if ini and syll.startswith(ini):
            return ini, syll[len(ini):]
    return "", syll


def _initial_variants(ini):
    vs = {ini}
    for a, b in _INITIAL_PAIRS:
        if ini == a:
            vs.add(b)
        elif ini == b:
            vs.add(a)
    return vs


def _final_variants(fin):
    vs = {fin}
    for a, b in _FINAL_PAIRS:
        if fin == a:
            vs.add(b)
        elif fin == b:
            vs.add(a)
    return vs


def fuzzy_set(syll):
    """返回该音节在模糊音规则下的所有等价音节集合（含自身）。"""
    ini, fin = _split_initial(syll)
    out = set()
    for vi in _initial_variants(ini):
        for vf in _final_variants(fin):
            out.add(vi + vf)
    return out


def abbrev_codes(sylls):
    """简拼（全首字母）/ 混拼（首音节全拼 + 其余首字母）。多音节才有意义。"""
    if len(sylls) < 2:
        return set()
    out = set()
    initials = "".join(s[0] for s in sylls)                 # nh / bhys
    mixed = sylls[0] + "".join(s[0] for s in sylls[1:])     # nih / buhys
    for code in (initials, mixed):
        if len(code) >= 2:
            out.add(code)
    return out


# ---------------------------------------------------------------------------
# 载入数据源
# ---------------------------------------------------------------------------
def load_raw():
    raw = []  # (text, raw_pinyin, weight, provenance)

    # 1) main_dict.json：{pinyin, candidates:[[text, freq], ...]}
    if os.path.exists(MAIN):
        for e in json.load(open(MAIN, encoding="utf-8")):
            py = e.get("pinyin", "")
            for cand in e.get("candidates", []):
                raw.append((cand[0], py, int(cand[1]), "data"))

    # 2) phrases.json：{pinyin, korean}
    if os.path.exists(PHRASES):
        for e in json.load(open(PHRASES, encoding="utf-8")):
            raw.append((e["korean"], e.get("pinyin", ""), 100, "data"))

    # 3) extra_phrases.txt：策划的多语言常用词条（拼音可连写）
    if os.path.exists(EXTRA):
        with open(EXTRA, encoding="utf-8") as f:
            for line in f:
                line = line.rstrip("\n")
                if not line or line.lstrip().startswith("#"):
                    continue
                parts = line.split("\t")
                if len(parts) < 2:
                    continue
                text, raw_py = parts[0], parts[1]
                if not raw_py.strip():
                    continue
                try:
                    w = int(parts[2]) if len(parts) >= 3 else 100
                except ValueError:
                    w = 100
                raw.append((text, raw_py, w, "extra"))

    return raw


def expand(raw):
    """把每条原始词条展开为 (text, code, weight) 集合：
    规范码 + 逐音节模糊音变体 + 简拼/混拼码。按 (text, code) 去重取最大权重。"""
    out = {}  # (text, code) -> max weight

    def add(text, code, w):
        if not code:
            return
        key = (text, code)
        if key not in out or w > out[key]:
            out[key] = w

    for text, raw_py, weight, prov in raw:
        sylls = split_pinyin(raw_py)
        if not sylls:
            continue
        canon = "".join(sylls)
        add(text, canon, weight)

        # 逐音节模糊音：每个音节取等价集合，笛卡尔积得到所有组合
        per = [fuzzy_set(s) for s in sylls]
        combos = list(itertools.product(*per))
        if len(combos) <= 64:  # 安全上限，避免极端组合爆炸
            for combo in combos:
                fc = "".join(combo)
                if fc != canon:
                    add(text, fc, max(weight - 1, 1))

        # 简拼 / 混拼
        for ac in abbrev_codes(sylls):
            if ac != canon and len(ac) >= 2:
                add(text, ac, max(weight - 5, 1))

    return out


# ---------------------------------------------------------------------------
# 输出
# ---------------------------------------------------------------------------
def main():
    raw = load_raw()
    entries = expand(raw)
    items = sorted(entries.items(), key=lambda kv: (-kv[1], kv[0][1], kv[0][0]))

    lines = [
        "# HanPinyin 词库（由 data/*.json + rime/extra_phrases.txt 自动生成，请勿手改；",
        "# 改数据源后重跑 build_dict.py，再在小狼毫里「重新部署」）",
        "# 每个词条含：规范拼音码 + 逐音节模糊音变体码 + 简拼/混拼码",
        "---",
        "name: hanpinyin",
        'version: "2.3"',
        "sort: by_weight",
        "use_preset_vocabulary: false",
        "columns:",
        "  - text",
        "  - code",
        "  - weight",
        "max_phrase_length: 20",
        "...",
    ]
    for (text, code), w in items:
        lines.append(f"{text}\t{code}\t{w}")

    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

    from_data = sum(1 for r in raw if r[3] == "data")
    from_extra = sum(1 for r in raw if r[3] == "extra")
    print(f"OK: 写出 {len(items)} 条 (text,code) -> {OUT}")
    print(f"     原始数据源条目: data={from_data} | extra={from_extra}")
    print(f"     模糊音/简拼展开后词条数: {len(items)}（含变体码）")

    # 自检：所有 data 源 (text, 规范码) 必须出现在产物中
    missing = []
    for text, raw_py, weight, prov in raw:
        if prov != "data":
            continue
        sylls = split_pinyin(raw_py)
        if not sylls:
            continue
        canon = "".join(sylls)
        if (text, canon) not in entries:
            missing.append((text, canon))
    if missing:
        print(f"[WARN] {len(missing)} 条数据源未进入产物（请检查拼音合法性）:")
        for m in missing[:20]:
            print("   ", m)
    else:
        print(f"      数据源完整覆盖校验: 通过")


if __name__ == "__main__":
    main()
