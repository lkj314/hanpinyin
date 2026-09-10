#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""《HanPinyin 词典》生成器 v2（体验升级版）
================================================
数据源：data/main_dict.json + data/phrases.json + rime/extra_phrases.txt
        + data/meanings.tsv（释义，键支持 code 与 code|韩文 两级）
        + data/dict_meta.tsv（分类 / 语体 / 全称 / 备注，同样的两级键）

内置功能：
  P0 搜索：与输入法同规则的输入（规范码·模糊音·简拼·混拼·中文·韩文·分类），
           空格不敏感，相关度排序 + 命中高亮 + 结果计数 + 无结果提示，
           快捷键 / 聚焦、Esc 清空、↑↓ 跳结果
  P1 释义：分类标签、语体标签（半语/敬语·해요/敬语·正式）、缩写全称（ㅇㅈ（인정））、
           用法备注、英雄英文名；附录《中文反查·同义聚类》
  P2 体验：分类筛选（音译层默认隐藏）、一键复制打码 / 复制直达链接、가나다 初声跳转、
           回到顶部、深色模式、打印优化、整句与正文去重、版本变更

用法：python docs/gen_dictionary.py     （退出码 0 = 释义全覆盖；非 0 会列出缺失码）
"""
import json
import os
import re
import subprocess
import sys
from datetime import date

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RIME = os.path.join(ROOT, "rime")
sys.path.insert(0, RIME)
import build_dict  # noqa: E402

HANGUL = re.compile(r"[\uAC00-\uD7A3\u3130-\u318F\u1100-\u11FF]")
JAMO = re.compile(r"^[\u3130-\u318F\u1100-\u11FF]+$")
MAIN = os.path.join(ROOT, "data", "main_dict.json")
PHRASES = os.path.join(ROOT, "data", "phrases.json")
EXTRA = os.path.join(RIME, "extra_phrases.txt")
MEANINGS = os.path.join(ROOT, "data", "meanings.tsv")
META = os.path.join(ROOT, "data", "dict_meta.tsv")
OUT = os.path.join(ROOT, "docs", "HanPinyin词典.html")
TRANSLIT_NOTE = "音译字（拼人名、地名等用）"
CATS = ["英雄", "战术", "报点", "缩写", "英文", "日常", "音译", "整句"]
DEFAULT_OFF = {"音译"}
MAX_VARIANTS = 20


def load_tsv(path):
    """键<TAB>值[<TAB>值…] -> dict；键可为 code 或 code|韩文"""
    out = {}
    for line in open(path, encoding="utf-8"):
        if not line.strip() or line.startswith("#"):
            continue
        p = line.rstrip("\n").split("\t")
        if len(p) >= 2 and p[0].strip():
            out[p[0].strip()] = [x.strip() for x in p[1:]]
    return out


def speech_of(text):
    """语体兜底判定（meta 缺失时使用）"""
    if not HANGUL.search(text):
        return "英文" if re.fullmatch(r"[A-Za-z0-9 !?.,'\-]+", text) else ""
    if JAMO.match(text):
        return "缩写"
    t = text.strip().rstrip("?!~.,")
    if any(x in t for x in ("습니다", "십시오", "시다", "ㅂ니다")):
        return "敬语·正式"
    if t.endswith(("요", "죠", "까", "세요")):
        return "敬语·해요"
    if t.endswith(("다", "자", "지", "냐", "어", "아", "해", "워", "와", "돼", "줘", "봐", "군")):
        return "半语"
    return ""


def main():
    meanings = load_tsv(MEANINGS)
    meta = load_tsv(META)

    wmap, disp, word_codes = {}, {}, {}
    src = {}
    phrases_all, raw = [], []

    def add(code, text, w, spaced, source):
        code = code.strip().lower()
        if not code or not re.fullmatch(r"[a-z]+", code) or not HANGUL.search(text):
            return
        if (code, text) not in wmap or w > wmap[(code, text)]:
            wmap[(code, text)] = w
        disp.setdefault(code, spaced)
        word_codes.setdefault(text, set()).add(code)
        src.setdefault((code, text), source)

    for e in json.load(open(MAIN, encoding="utf-8")):
        sylls = e["pinyin"].split()
        code = "".join(sylls)
        for t, w in e["candidates"]:
            add(code, t, int(w), " ".join(sylls), "main")
            raw.append((t, e["pinyin"], int(w), "data"))

    for line in open(EXTRA, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.lstrip().startswith("#"):
            continue
        p = line.split("\t")
        if len(p) < 2 or not p[1].strip():
            continue
        text, py = p[0], p[1].strip().lower()
        try:
            w = int(p[2]) if len(p) >= 3 else 100
        except ValueError:
            w = 100
        sylls = build_dict.split_pinyin(py)
        if sylls and "".join(sylls) == py:
            add(py, text, w, " ".join(sylls), "extra")
            raw.append((text, py, w, "extra"))

    for e in json.load(open(PHRASES, encoding="utf-8")):
        sylls = e["pinyin"].split()
        code = "".join(sylls)
        if HANGUL.search(e["korean"]) and re.fullmatch(r"[a-z]+", code):
            add(code, e["korean"], 100, " ".join(sylls), "phrase")
            raw.append((e["korean"], e["pinyin"], 100, "data"))
            phrases_all.append((code, " ".join(sylls), e["korean"]))

    # 模糊音 / 简拼 / 混拼变体（与输入法同源）——写进搜索索引，让词典搜索 = 输入法规则
    variants = {}
    for (text, code) in build_dict.expand(raw):
        variants.setdefault(text, set()).add(code)

    def get2(table, code, text, idx=0):
        row = table.get("%s|%s" % (code, text)) or table.get(code)
        if not row:
            return ""
        return row[idx] if idx < len(row) else ""

    def meaning_of(code, text):
        v = get2(meanings, code, text, 0)
        if v:
            return v
        if len(build_dict.split_pinyin(code)) == 1:
            return TRANSLIT_NOTE
        return ""

    # ---------- 词条 ----------
    words = {}
    for (code, text), w in wmap.items():
        info = words.setdefault(text, {"codes": [], "w": 0})
        info["codes"].append(code)
        info["w"] = max(info["w"], w)
    missing = []
    for text, info in words.items():
        info["codes"] = sorted(set(info["codes"]), key=lambda c: (-wmap[(c, text)], c))
        info["primary"] = info["codes"][0]
        info["meaning"] = meaning_of(info["primary"], text)
        # 元数据按来源优先级取码：正文词条优先用 main/extra 的码（整句来源的码只作打法）
        mcode = info["primary"]
        for pref in ("main", "extra", "phrase"):
            hit = next((c for c in info["codes"] if src.get((c, text)) == pref), None)
            if hit:
                mcode = hit
                break
        info["mcode"] = mcode
        if not info["meaning"]:
            missing.append(info["primary"])
            info["meaning"] = "释义待补"
        info["cat"] = get2(meta, info["mcode"], text, 0) or (
            "音译" if "音译字" in info["meaning"] else
            "英雄" if "（英雄" in info["meaning"] else
            "缩写" if JAMO.match(text) else
            "英文" if not HANGUL.search(text) else "日常")
        if info["cat"] == "整句":      # 正文词条不可能是整句（防 meta 来源串味）
            info["cat"] = ("音译" if "音译字" in info["meaning"] else
                           "英雄" if "（英雄" in info["meaning"] else
                           "缩写" if JAMO.match(text) else
                           "英文" if not HANGUL.search(text) else "日常")
        info["speech"] = get2(meta, info["mcode"], text, 1) or speech_of(text)
        info["full"] = get2(meta, info["mcode"], text, 2)
        info["note"] = get2(meta, info["mcode"], text, 3)
        vs = sorted(variants.get(text, set()) - set(info["codes"]), key=lambda c: (len(c), c))
        info["variants"] = vs[:MAX_VARIANTS]
        info["search_codes"] = info["codes"] + info["variants"]

    body_texts = {t for t, i in words.items()
                  if any(src.get((c, t)) in ("main", "extra") for c in i["codes"])}
    phrases = [p for p in phrases_all if p[2] not in body_texts]
    n_dup = len(phrases_all) - len(phrases)

    by_code = {}
    for (code, text), w in wmap.items():
        by_code.setdefault(code, []).append((w, text))
    for code in by_code:
        by_code[code] = [t for w, t in sorted(by_code[code], key=lambda x: -x[0])]

    def esc(s):
        return (s or "").replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")

    def norm(s):
        return re.sub(r"[\s'’·]", "", (s or "").lower())

    CIRC = "①②③④⑤⑥⑦⑧⑨⑩⑪⑫⑬⑭⑮"
    counter = [0]

    def entry_html(text, info, is_sentence=False):
        counter[0] += 1
        eid = "w%d" % counter[0]
        hw = esc(text)
        if info["full"] and JAMO.match(text):
            hw = '%s<em class="full">（%s）</em>' % (esc(text), esc(info["full"]))
        tags = ""
        if info["cat"]:
            tags += '<span class="tg tg-c">%s</span>' % esc(info["cat"])
        if info["speech"]:
            tags += '<span class="tg tg-s">%s</span>' % esc(info["speech"])
        typed = "、".join(info["codes"])
        py_show = "〕〔".join(disp.get(c, c) for c in info["codes"])
        ab = ""
        if not is_sentence and info["variants"]:
            short = [c for c in info["variants"] if len(c) <= 12][:2]
            if short:
                ab = '<span class="ab">也可打 %s</span>' % esc("、".join(short))
        cands = ""
        for c in info["codes"]:
            grp = by_code.get(c, [])
            if len(grp) > 1:
                cands += '<span class="cands">同码候选　' + "　".join(
                    CIRC[i] + " " + esc(t) for i, t in enumerate(grp[:15])) + "</span>"
                break
        note = '<div class="note">用法：%s</div>' % esc(info["note"]) if info["note"] else ""
        s_t = norm(text + " " + info["full"])
        s_c = " ".join(c.lower() for c in info["search_codes"])   # 保留空格分隔，保证精确/前缀匹配
        s_m = norm(info["meaning"] + " " + info["note"] + " " + info["cat"] + " " + info["speech"])
        return (
            '<div class="entry" id="%s" data-cat="%s" data-t="%s" data-c="%s" data-m="%s" data-code="%s">'
            '<div class="hd"><span class="hw">%s</span><span class="tags">%s</span>'
            '<span class="acts"><button class="act" data-copy="%s" title="复制打码">复制</button>'
            '<button class="act" data-link="%s" title="复制直达链接">链接</button></span></div>'
            '<div class="py">〔%s〕<span class="typed">打 %s</span>%s</div>'
            '<div class="def">释义：%s</div>%s%s</div>'
            % (eid, esc(info["cat"]), esc(s_t), esc(s_c), esc(s_m), esc(info["codes"][0]),
               hw, tags, esc(typed), eid, esc(py_show), esc(typed), ab, esc(info["meaning"]), note, cands))

    body = {}
    for text, info in sorted(words.items(), key=lambda kv: (kv[1]["primary"], kv[0])):
        body.setdefault(info["primary"][0].upper(), []).append(entry_html(text, info))
    sections = ['<section class="sec"><h2 class="letter">%s</h2><div class="cols">%s</div></section>'
                % (l, "\n".join(body[l])) for l in sorted(body)]

    ph_html = []
    for code, spaced, text in sorted(phrases, key=lambda x: x[0]):
        info = {"codes": [code], "primary": code, "meaning": get2(meanings, code, text, 0) or "整句",
                "cat": "整句", "speech": speech_of(text), "full": "", "note": "", "variants": []}
        info["search_codes"] = [code]
        ph_html.append(entry_html(text, info, is_sentence=True))

    # 附录二：中文反查·同义聚类
    groups = {}
    for text, info in words.items():
        if info["cat"] in ("音译", "英雄", "英文"):
            continue
        key = re.sub(r"[（(].*?[)）]", "", info["meaning"]).strip()
        if not key or key == "释义待补":
            continue
        groups.setdefault(key, []).append((text, info))
    groups = {k: v for k, v in groups.items() if len({t for t, _ in v}) >= 2}
    glist = sorted(groups.items(), key=lambda kv: (-len(kv[1]), kv[0]))
    cluster_html = []
    for key, items in glist:
        chips = []
        for text, info in items:
            sp = '<span class="tg tg-s">%s</span>' % esc(info["speech"]) if info["speech"] else ""
            chips.append('<span class="citem"><b>%s</b>%s<span class="cc">打 %s</span></span>'
                         % (esc(text), sp, esc(info["codes"][0])))
        cluster_html.append('<div class="cgroup"><div class="ckey">%s<span class="cn">%d 种说法</span></div>%s</div>'
                            % (esc(key), len(items), "".join(chips)))

    # 附录三：가나다역참조（初声跳转）
    JUMPS = [("초성어", None), ("ㄱ", 0), ("ㄴ", 2), ("ㄷ", 3), ("ㄹ", 5), ("ㅁ", 6), ("ㅂ", 7),
             ("ㅅ", 9), ("ㅇ", 11), ("ㅈ", 12), ("ㅊ", 14), ("ㅋ", 15), ("ㅌ", 16), ("ㅍ", 17), ("ㅎ", 18)]
    rev_items, cur = [], None
    for text in sorted(words, key=lambda t: (0 if JAMO.match(t) else 1, t)):
        ch = text[0]
        if JAMO.match(ch):
            grp = "초성어"
        elif "\uAC00" <= ch <= "\uD7A3":
            gi = (ord(ch) - 0xAC00) // 588
            grp = next((name for name, g in JUMPS if g == gi), "기타")
        else:
            grp = "기타"
        if grp != cur:
            cur = grp
            rev_items.append('<h3 class="subhead" id="rev-%s">%s</h3>' % (esc(grp), esc(grp)))
        codes = "、".join(sorted(word_codes[text]))
        c0 = sorted(word_codes[text])[0]
        rev_items.append(
            '<div class="entry rev" data-cat="%s" data-t="%s" data-c="%s" data-m="%s" data-code="%s">'
            '<span class="hw">%s</span><span class="py">→ %s</span></div>'
            % (esc(get2(meta, c0, text, 0) or "日常"), esc(norm(text)), esc(norm(codes)),
               esc(norm(text)), esc(codes), esc(text), esc(codes)))
    jump_html = "".join('<a href="#rev-%s">%s</a>' % (esc(name), esc(name)) for name, _ in JUMPS)

    try:
        log = subprocess.run(["git", "log", "--pretty=%h %ad %s", "--date=short", "-6"],
                             cwd=ROOT, capture_output=True, text=True, timeout=15).stdout.strip().split("\n")
    except Exception:
        log = []
    ver_html = "".join("<li><code>%s</code></li>" % esc(x) for x in log if x.strip()) or "<li>（无 git 记录）</li>"

    stats = "收录韩文词条 %d 条 · 整句短语 %d 句 · 同义聚类 %d 组" % (len(words), len(phrases), len(glist))
    nav = "".join('<a href="#sec-%s">%s</a>' % (l, l) for l in sorted(body))

    HTML = TEMPLATE
    for k, v in [("__STATS__", stats), ("__DATE__", date.today().isoformat()),
                 ("__CHIPS__", "".join('<label class="chip%s"><input type="checkbox" data-cat="%s"%s>%s</label>'
                                       % (" off" if c in DEFAULT_OFF else "", c,
                                          "" if c in DEFAULT_OFF else " checked", c) for c in CATS)),
                 ("__NAV__", nav), ("__BODY__", "\n".join(sections)), ("__PHRASES__", "\n".join(ph_html)),
                 ("__CLUSTER__", "\n".join(cluster_html)), ("__REVERSE__", "\n".join(rev_items)),
                 ("__JUMP__", jump_html), ("__VER__", ver_html),
                 ("__NCLUSTER__", str(len(glist))), ("__NPHASEDUP__", str(n_dup))]:
        HTML = HTML.replace(k, v)

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, "w", encoding="utf-8", newline="\n").write(HTML)
    print("OK:", OUT, "| %d KB" % (os.path.getsize(OUT) // 1024))
    print("STATS:", stats)
    print("MISSING 释义数:", len(missing), missing[:8])
    return 1 if missing else 0


TEMPLATE = r"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>HanPinyin 词典</title>
<style>
  :root{--paper:#FBF7EE;--ink:#2B2620;--muted:#7A7060;--line:#D8CFC0;--card:#FFFDF8;--acc:#A32D2D;--def:#1F4E79;--hl:#FFE9A8;}
  @media (prefers-color-scheme: dark){:root{--paper:#1E1B17;--ink:#EDE7DC;--muted:#A99F8E;--line:#3B362E;--card:#272320;--acc:#E0796F;--def:#9CC7F0;--hl:#5A4A1A;}}
  body{margin:0;background:var(--paper);color:var(--ink);font-family:"Noto Serif SC","Source Han Serif SC","SimSun","Malgun Gothic",serif;}
  .page{max-width:940px;margin:0 auto;padding:26px 20px 70px;}
  .titlepage{text-align:center;border-bottom:3px double var(--acc);padding:30px 0 20px;margin-bottom:16px;}
  .titlepage h1{font-size:38px;margin:0;letter-spacing:6px;color:var(--acc);}
  .titlepage .sub{font-size:14px;color:var(--muted);margin-top:9px;letter-spacing:1px;}
  .titlepage .stats{font-size:13px;color:var(--muted);margin-top:7px;}
  .toolbar{position:sticky;top:0;background:var(--paper);padding:9px 0 8px;border-bottom:1px solid var(--line);z-index:20;}
  .srow{display:flex;gap:8px;align-items:center;}
  .srow input{flex:1;padding:9px 12px;font-size:15px;border:1px solid var(--line);border-radius:8px;background:var(--card);color:inherit;font-family:inherit;}
  .count{font-size:12px;color:var(--muted);white-space:nowrap;min-width:60px;text-align:right;}
  .chips{margin:8px 0 0;display:flex;flex-wrap:wrap;gap:6px;}
  .chip{font-size:12px;border:1px solid var(--line);border-radius:999px;padding:2px 9px;background:var(--card);cursor:pointer;user-select:none;}
  .chip input{margin-right:4px;vertical-align:middle;}
  .chip.off{opacity:.55;}
  .nav{margin:7px 0 0;line-height:1.9;}
  .nav a,.jump a{display:inline-block;min-width:22px;text-align:center;margin-right:4px;padding:1px 6px;border:1px solid var(--line);border-radius:6px;text-decoration:none;color:var(--acc);font-size:12.5px;background:var(--card);}
  h2.letter{color:var(--acc);font-size:24px;border-bottom:2px solid var(--acc);padding-bottom:4px;margin:24px 0 10px;letter-spacing:3px;}
  h3.subhead{color:var(--muted);font-size:14px;margin:16px 0 8px;border-bottom:1px dashed var(--line);padding-bottom:3px;scroll-margin-top:130px;}
  .cols{column-count:2;column-gap:24px;}
  @media (max-width:660px){.cols{column-count:1;}}
  .entry{break-inside:avoid;padding:5px 0 6px;border-bottom:1px dotted var(--line);scroll-margin-top:130px;}
  .hd{display:flex;align-items:baseline;gap:6px;flex-wrap:wrap;}
  .hw{font-size:17px;font-weight:700;cursor:pointer;}
  .hw .full{font-style:normal;font-size:13px;color:var(--muted);font-weight:400;}
  .tags{display:inline-flex;gap:4px;}
  .tg{font-size:11px;border-radius:5px;padding:0 5px;border:1px solid var(--line);color:var(--muted);}
  .tg-c{color:var(--acc);border-color:var(--acc);}
  .acts{margin-left:auto;display:inline-flex;gap:4px;}
  .act{font-size:11px;border:1px solid var(--line);background:var(--card);color:var(--muted);border-radius:5px;padding:1px 6px;cursor:pointer;font-family:inherit;}
  .act:hover{color:var(--acc);border-color:var(--acc);}
  .py{display:block;font-size:12.5px;color:var(--muted);margin-top:2px;}
  .typed{color:var(--acc);margin-left:6px;}
  .ab{color:var(--muted);margin-left:6px;}
  .def{display:block;font-size:13px;color:var(--def);margin-top:2px;}
  .note{display:block;font-size:12.5px;color:var(--muted);margin-top:1px;}
  .cands{display:block;font-size:12.5px;color:var(--muted);margin-top:2px;}
  mark{background:var(--hl);color:inherit;padding:0 1px;border-radius:2px;}
  .appx{margin-top:40px;border-top:3px double var(--acc);padding-top:12px;}
  .appx h2{font-size:19px;color:var(--acc);letter-spacing:2px;margin:0 0 8px;}
  .guide{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:13px 16px;font-size:13.5px;line-height:1.85;}
  .guide b{color:var(--acc);}
  .fanli{font-size:13px;line-height:1.9;background:var(--card);border:1px solid var(--line);border-radius:10px;padding:11px 16px;margin:12px 0 4px;}
  .cgroup{border-bottom:1px dotted var(--line);padding:6px 0;}
  .ckey{font-weight:700;color:var(--acc);font-size:14px;}
  .cn{font-weight:400;color:var(--muted);font-size:12px;margin-left:8px;}
  .citem{display:inline-block;margin:3px 10px 0 0;font-size:14px;}
  .cc{font-size:11.5px;color:var(--muted);margin-left:5px;}
  #results{display:none;background:var(--card);border:1px solid var(--line);border-radius:10px;padding:4px 14px;}
  #hint{display:none;background:var(--card);border:1px dashed var(--line);border-radius:10px;padding:12px 16px;margin:14px 0;font-size:13.5px;color:var(--muted);}
  #toast{position:fixed;left:50%;bottom:26px;transform:translateX(-50%);background:var(--ink);color:var(--paper);padding:7px 14px;border-radius:999px;font-size:13px;opacity:0;transition:opacity .2s;pointer-events:none;z-index:50;}
  #toast.on{opacity:.92;}
  #top{position:fixed;right:18px;bottom:22px;width:38px;height:38px;border-radius:50%;border:1px solid var(--line);background:var(--card);color:var(--acc);font-size:16px;cursor:pointer;display:none;z-index:40;}
  .entry.focus{outline:2px solid var(--acc);outline-offset:2px;}
  @media print{.toolbar,#top,#toast,.acts{display:none!important}.page{max-width:100%}body{background:#fff}.entry,.sec,.appx{display:block!important}}
</style>
</head>
<body>
<div class="page">
  <div class="titlepage">
    <h1>HanPinyin 词典</h1>
    <div class="sub">用汉语拼音打韩文 · 输入法已实装词汇总览（体验增强版）</div>
    <div class="stats">__STATS__ · 词库版本 v2.12 · __DATE__ 编成</div>
  </div>

  <div class="toolbar">
    <div class="srow">
      <input id="q" type="search" placeholder="搜索：拼音（paiwei / lihao / nh）、中文（怎么办）、韩文（안녕）、分类…">
      <span class="count" id="cnt"></span>
    </div>
    <div class="chips">__CHIPS__</div>
    <div class="nav">__NAV__ ｜ <a href="#appx-phrases">整句</a> <a href="#appx-cluster">中文反查</a> <a href="#appx-reverse">가나다</a> <a href="#appx-guide">上手</a> <a href="#appx-ver">版本</a></div>
  </div>

  <div class="fanli">
    <b>凡例</b>　① 词条按<b>输入拼音的字母顺序</b>排列，韩文作词头；② 〔〕内为拼音音节，<span style="color:var(--acc)">打</span>后为实际键入的<b>连写码</b>；③ <b>释义</b>为该词的中文意思；④ 标签：<span class="tg tg-c">英雄</span> 为分类，<span class="tg tg-s">敬语·해요</span> 为<b>语体</b>（自动判定，仅供参考：요/죠 结尾多为敬语，자/어/지 结尾多为半语）；⑤ <b>搜索接受与输入法相同的输入</b>——规范码、<b>模糊音</b>（lihao→안녕하세요）、<b>简拼/混拼</b>（nh→안녕하세요）、中文释义、韩文、分类；空格不敏感（ni hao = nihao）；⑥ 快捷键：<b>/</b> 聚焦搜索、<b>Esc</b> 清空、<b>↑↓</b> 跳结果；⑦ 词条可一键<b>复制打码</b>或<b>复制直达链接</b>；⑧ 音译层（拼人名用）默认隐藏，可在顶部勾选显示。
  </div>

  <div id="hint"></div>
  <div id="results"></div>
  <div id="body">__BODY__</div>

  <div class="appx" id="appx-phrases">
    <h2>附录一　整句短语篇</h2>
    <div class="cols">__PHRASES__</div>
    <p style="font-size:12.5px;color:var(--muted)">注：另有 __NPHASEDUP__ 句与正文词条重合，已并入正文音序，此处不重复列出。</p>
  </div>

  <div class="appx" id="appx-cluster">
    <h2>附录二　中文反查 · 同义聚类（__NCLUSTER__ 组）</h2>
    <p style="font-size:12.5px;color:var(--muted)">同一个中文意思在韩语里的多种说法（含语体差异），按说法数量排序。</p>
    __CLUSTER__
  </div>

  <div class="appx" id="appx-reverse">
    <h2>附录三　가나다역참조（看词查打法）</h2>
    <div class="jump">__JUMP__</div>
    <div class="cols">__REVERSE__</div>
  </div>

  <div class="appx" id="appx-guide">
    <h2>附录四　新人上手指南</h2>
    <div class="guide">
      <b>第一步 · 部署</b>：把 <span style="font-family:monospace">sino_mix.schema.yaml</span> 与 <span style="font-family:monospace">hanpinyin.dict.yaml</span> 复制到 <span style="font-family:monospace">%APPDATA%\Rime\</span>（覆盖旧文件）。<br>
      <b>第二步 · 重新部署</b>：右键任务栏小狼毫（Weasel）托盘图标 →「重新部署」。重启输入法 ≠ 重新部署。<br>
      <b>第三步 · 打字验证</b>：切到「韩文拼音 HanPinyin」，打 <b>nihao</b> 应出 안녕하세요；<b>lihao</b>（模糊音）、<b>nh</b>（简拼）也应对应；打 <b>paiwei</b> 出 랭크。<br>
      <b>本词典怎么用</b>：搜中文找说法（搜「怎么办」）、搜拼音查词义（搜 <span style="font-family:monospace">zmb</span>）、看到韩文不认识就去附录三按字母查打法。<br>
      <b>小提示</b>：翻页用 <b>- / =</b>；常用词选过几次会自动上浮；重置自学习删除 <span style="font-family:monospace">%APPDATA%\Rime\hanpinyin.userdb\</span> 后重新部署。
    </div>
  </div>

  <div class="appx" id="appx-ver">
    <h2>附录五　版本变更</h2>
    <ul class="guide" style="margin:0;padding-left:34px">__VER__</ul>
  </div>

  <div style="text-align:center;color:var(--muted);font-size:12px;margin-top:34px;">—— 全书终 · Generated from HanPinyin data sources ——</div>
</div>
<button id="top" title="回到顶部">↑</button>
<div id="toast"></div>
<script>
var IDX = [];
function norm(s){return (s||"").toLowerCase().replace(/[\s'’·]/g,"");}
function init(){
  document.querySelectorAll('.entry:not(.rev)').forEach(function(el){
    IDX.push({el:el, orig:el.cloneNode(true), rank:IDX.length});
  });
  var q=document.getElementById('q');
  q.addEventListener('input', debounce(run,110));
  q.addEventListener('keydown', function(ev){ if(ev.key==='ArrowDown'){ ev.preventDefault(); run(); moveFocus(1);} });
  document.querySelectorAll('.chip input').forEach(function(cb){
    cb.addEventListener('change', function(){ cb.parentNode.classList.toggle('off', !cb.checked); run(); });
  });
  document.addEventListener('click', function(ev){
    var b=ev.target.closest && ev.target.closest('.act');
    if(b){
      ev.stopPropagation();
      if(b.dataset.copy) copyText(b.dataset.copy,'已复制打码：'+b.dataset.copy,b);
      else if(b.dataset.link){ copyText(location.href.split('#')[0]+'#'+b.dataset.link,'已复制直达链接',b); }
      return;
    }
    var h=ev.target.closest && ev.target.closest('.hw');
    if(h){ var e=h.closest('.entry'); if(e&&e.id){ location.hash=e.id; } }
  });
  window.addEventListener('scroll', function(){ document.getElementById('top').style.display=window.scrollY>400?'block':'none'; });
  document.getElementById('top').addEventListener('click', function(){ window.scrollTo({top:0,behavior:'smooth'}); });
  document.addEventListener('keydown', function(ev){
    if(ev.key==='/'&&document.activeElement.id!=='q'){ ev.preventDefault(); q.focus(); }
    else if(ev.key==='Escape'){ q.value=''; run(); }
    else if((ev.key==='ArrowDown'||ev.key==='ArrowUp')&&document.getElementById('results').style.display==='block'){
      ev.preventDefault(); moveFocus(ev.key==='ArrowDown'?1:-1);
    }
  });
  if(location.hash){ var t=document.querySelector(location.hash); if(t){ t.scrollIntoView(); t.classList.add('focus'); } }
  run();
}
function catsOn(){ var on={}; document.querySelectorAll('.chip input').forEach(function(cb){ on[cb.dataset.cat]=cb.checked; }); return on; }
function score(el,q){
  var t=el.dataset.t||"", c=(el.dataset.c||"").split(' '), m=el.dataset.m||"", cat=norm(el.dataset.cat||"");
  if(t===q||c.indexOf(q)>=0) return 4;
  if(t.indexOf(q)===0) return 3;
  for(var i=0;i<c.length;i++){ if(c[i].indexOf(q)===0) return 3; }
  if((el.dataset.c||"").indexOf(q)>=0) return 2;
  if(m.indexOf(q)>=0||cat.indexOf(q)>=0) return 1;
  if((t+el.dataset.c+m).indexOf(q)>=0) return 0;
  return -1;
}
function run(){
  var raw=document.getElementById('q').value.trim(), q=norm(raw), on=catsOn();
  var res=document.getElementById('results'), hint=document.getElementById('hint'), cnt=document.getElementById('cnt');
  var body=document.getElementById('body');
  if(!q){
    res.style.display='none'; res.innerHTML=''; hint.style.display='none'; cnt.textContent=''; body.style.display='';
    document.querySelectorAll('.appx').forEach(function(a){ a.style.display=''; });
    var secs=document.querySelectorAll('.sec');
    secs.forEach(function(sec){
      var any=false;
      sec.querySelectorAll('.entry').forEach(function(el){
        var ok = on[el.dataset.cat]!==false;
        el.style.display = ok? '' : 'none';
        if(ok) any=true;
      });
      sec.style.display = any? '' : 'none';
    });
    document.querySelectorAll('#appx-phrases .entry').forEach(function(el){ el.style.display = on['整句']!==false?'':'none'; });
    return;
  }
  var hits=[];
  IDX.forEach(function(o){
    var el=o.el;
    if(on[el.dataset.cat]===false) return;
    var s=score(el,q);
    if(s>=0) hits.push({el:el, s:s, rank:o.rank});
  });
  hits.sort(function(a,b){ return (b.s-a.s)||(a.rank-b.rank); });
  body.style.display='none';
  document.querySelectorAll('.appx').forEach(function(a){ a.style.display='none'; });
  res.innerHTML='';
  hits.slice(0,300).forEach(function(h){
    var clone=h.el.cloneNode(true); clone.classList.remove('focus');
    clone.removeAttribute('id');
    res.appendChild(clone);
    highlight(clone, raw);
  });
  res.style.display='block';
  cnt.textContent = hits.length? (hits.length+' 条'+(hits.length>300?'（显示前 300）':'')) : '';
  if(!hits.length){
    hint.style.display='block';
    hint.innerHTML='没有找到「'+raw+'」——试试：<b>简拼</b>（nh）、<b>模糊音</b>（lihao）、<b>中文</b>（怎么办）、<b>韩文</b>（안녕），或勾选「音译」分类。';
  } else hint.style.display='none';
}
function highlight(el,raw){
  if(!raw) return;
  var needle=raw, walker=document.createTreeWalker(el,NodeFilter.SHOW_TEXT,null,false), nodes=[];
  while(walker.nextNode()) nodes.push(walker.currentNode);
  nodes.forEach(function(node){
    var txt=node.nodeValue, low=txt.toLowerCase(), nl=needle.toLowerCase(), i=low.indexOf(nl);
    if(i<0) return;
    var frag=document.createDocumentFragment(), last=0;
    while(i>=0){
      frag.appendChild(document.createTextNode(txt.slice(last,i)));
      var mk=document.createElement('mark'); mk.textContent=txt.slice(i,i+needle.length); frag.appendChild(mk);
      last=i+needle.length; i=low.indexOf(nl,last);
    }
    frag.appendChild(document.createTextNode(txt.slice(last)));
    node.parentNode.replaceChild(frag,node);
  });
}
function moveFocus(d){
  var items=document.getElementById('results').querySelectorAll('.entry');
  if(!items.length) return;
  var cur=document.getElementById('results').querySelector('.entry.focus');
  var idx=cur? Array.prototype.indexOf.call(items,cur) : -1;
  var next=Math.min(items.length-1, Math.max(0, idx+d));
  if(cur) cur.classList.remove('focus');
  items[next].classList.add('focus'); items[next].scrollIntoView({block:'center'});
}
function copyText(t,msg,btn){
  var done=function(){ toast(msg); if(btn){ var o=btn.textContent; btn.textContent='已复制'; setTimeout(function(){btn.textContent=o;},900);} };
  if(navigator.clipboard && navigator.clipboard.writeText){ navigator.clipboard.writeText(t).then(done,function(){ fallback(t,done); }); }
  else fallback(t,done);
}
function fallback(t,done){ var ta=document.createElement('textarea'); ta.value=t; document.body.appendChild(ta); ta.select(); try{document.execCommand('copy');}catch(e){} document.body.removeChild(ta); done(); }
function toast(msg){ var el=document.getElementById('toast'); el.textContent=msg; el.classList.add('on'); setTimeout(function(){el.classList.remove('on');},1400); }
function debounce(fn,ms){ var t; return function(){ clearTimeout(t); t=setTimeout(fn,ms); }; }
document.addEventListener('DOMContentLoaded', init);
</script>
</body>
</html>
"""

if __name__ == "__main__":
    sys.exit(main())
