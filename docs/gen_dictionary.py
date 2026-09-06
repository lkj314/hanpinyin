#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""《HanPinyin 词典》生成器（正式版）
=================================
从输入法实装数据源（data/main_dict.json + data/phrases.json + rime/extra_phrases.txt）
与释义表（data/meanings.tsv）生成新华词典风格的 HTML 词典 docs/HanPinyin词典.html。

用法：    python docs/gen_dictionary.py
再版流程：改词库 → python rime/build_dict.py → 补 meanings.tsv 新词释义 → 重跑本脚本。
规则：    未在 meanings.tsv 中的【单音节】键自动标注"音译字"；
          多音节键缺释义会打印 MISSING 清单并以退出码 1 结束（词条仍生成，标"释义待补"）。"""
import json
import os
import re
import sys
from datetime import date

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RIME = os.path.join(ROOT, "rime")
sys.path.insert(0, RIME)
import build_dict  # noqa: E402

HANGUL = re.compile(r"[\uAC00-\uD7A3\u3130-\u318F\u1100-\u11FF]")
MAIN = os.path.join(ROOT, "data", "main_dict.json")
PHRASES = os.path.join(ROOT, "data", "phrases.json")
EXTRA = os.path.join(RIME, "extra_phrases.txt")
MEANINGS = os.path.join(ROOT, "data", "meanings.tsv")
OUT = os.path.join(ROOT, "docs", "HanPinyin词典.html")
TRANSLIT_NOTE = "音译字（拼人名、地名等用）"


def load_meanings():
    mp = {}
    for line in open(MEANINGS, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) >= 2 and parts[0].strip():
            mp[parts[0].strip()] = parts[1].strip()
    return mp


def main():
    meanings = load_meanings()

    # (code, text) -> weight；code -> 展示音节
    wmap, disp = {}, {}
    word_codes = {}
    phrases = []

    def add(code, text, w, spaced):
        code = code.strip().lower()
        if not code or not re.fullmatch(r"[a-z]+", code) or not HANGUL.search(text):
            return False
        key = (code, text)
        if key not in wmap or w > wmap[key]:
            wmap[key] = w
        disp.setdefault(code, spaced)
        word_codes.setdefault(text, set()).add(code)
        return True

    for e in json.load(open(MAIN, encoding="utf-8")):
        sylls = e["pinyin"].split()
        code = "".join(sylls)
        for t, w in e["candidates"]:
            add(code, t, int(w), " ".join(sylls))

    for line in open(EXTRA, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.lstrip().startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) < 2 or not parts[1].strip():
            continue
        text, py = parts[0], parts[1].strip().lower()
        try:
            w = int(parts[2]) if len(parts) >= 3 else 100
        except ValueError:
            w = 100
        sylls = build_dict.split_pinyin(py)
        if sylls and "".join(sylls) == py:
            add(py, text, w, " ".join(sylls))

    for e in json.load(open(PHRASES, encoding="utf-8")):
        sylls = e["pinyin"].split()
        code = "".join(sylls)
        if HANGUL.search(e["korean"]) and re.fullmatch(r"[a-z]+", code):
            add(code, e["korean"], 100, " ".join(sylls))
            phrases.append((code, " ".join(sylls), e["korean"]))

    # 释义：code -> 中文；缺失检测
    def meaning_of(code):
        if code in meanings:
            return meanings[code]
        if code in build_dict.VOCAB and len(code) <= 6:
            # 单音节且可再拆？仅当整体就是一个音节时按音译处理
            if " " not in disp.get(code, "") and code in build_dict.VOCAB and len(build_dict.split_pinyin(code)) == 1:
                return TRANSLIT_NOTE
        return None

    missing = sorted({c for (c, _t) in wmap if meaning_of(c) is None})
    for c in missing:
        print("MISSING 释义:", c)

    # 组装词条
    words = {}
    for (code, text), w in wmap.items():
        info = words.setdefault(text, {"codes": [], "w": 0})
        info["codes"].append(code)
        info["w"] = max(info["w"], w)
    for text, info in words.items():
        info["codes"] = sorted(set(info["codes"]), key=lambda c: (-wmap[(c, text)], c))
        info["primary"] = info["codes"][0]
    word_list = sorted(words.items(), key=lambda kv: (kv[1]["primary"], kv[0]))
    n_words = len(word_list)

    cands_by_code = {}
    for (code, text), w in wmap.items():
        cands_by_code.setdefault(code, []).append((w, text))
    for code in cands_by_code:
        cands_by_code[code] = [t for w, t in sorted(cands_by_code[code], key=lambda x: -x[0])]

    CIRC = "①②③④⑤⑥⑦⑧⑨⑩⑪⑫⑬⑭⑮"

    def esc(s):
        return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

    def word_def(info):
        seen, out = set(), []
        for c in info["codes"]:
            m = meaning_of(c) or "释义待补"
            if m not in seen:
                seen.add(m)
                out.append(m)
        return " · ".join(out)

    body_sections = {}
    for text, info in word_list:
        body_sections.setdefault(info["primary"][0].upper(), []).append((text, info))

    sec_html = []
    for letter in sorted(body_sections):
        items = []
        for text, info in body_sections[letter]:
            py_show = "〕〔".join(disp[c] for c in info["codes"])
            typed = "打 " + "、".join(info["codes"])
            cands = ""
            for c in info["codes"]:
                group = cands_by_code.get(c, [])
                if len(group) > 1:
                    cands += '<span class="cands">' + "　".join(
                        CIRC[i] + " " + esc(t) for i, t in enumerate(group[:15])) + "</span>"
            items.append(
                '<div class="entry" data-k="%s %s %s"><span class="hw">%s</span>'
                '<span class="py">〔%s〕<span class="typed">%s</span></span>'
                '<span class="def">释义：%s</span>%s</div>'
                % (esc(text), " ".join(info["codes"]), esc(word_def(info)),
                   esc(text), esc(py_show), esc(typed), esc(word_def(info)), cands))
        sec_html.append('<section id="sec-%s"><h2 class="letter">%s</h2><div class="cols">%s</div></section>'
                        % (letter, letter, "\n".join(items)))

    ph_sorted = sorted(phrases, key=lambda x: x[0])
    ph_items = "\n".join(
        '<div class="entry sent" data-k="%s %s %s"><span class="hw">%s</span>'
        '<span class="py">〔%s〕<span class="typed">打 %s</span></span>'
        '<span class="def">释义：%s</span></div>'
        % (esc(t), esc(code), esc(meanings.get(code, "释义待补")), esc(t), esc(spaced), esc(code),
           esc(meanings.get(code, "释义待补"))) for code, spaced, t in ph_sorted)

    def hangul_sort_key(text):
        ch = text[0]
        jamo = 0 if ("\u3130" <= ch <= "\u318F" or "\u1100" <= ch <= "\u11FF") else 1
        return (jamo, text)

    def text_def(text):
        seen, out = set(), []
        for c in sorted(word_codes[text]):
            m = meaning_of(c) or "释义待补"
            if m not in seen:
                seen.add(m)
                out.append(m)
        return " · ".join(out)

    rev_items, last_jamo = [], None
    for text in sorted(word_codes, key=hangul_sort_key):
        ch = text[0]
        is_jamo = 0 if ("\u3130" <= ch <= "\u318F" or "\u1100" <= ch <= "\u11FF") else 1
        if is_jamo != last_jamo:
            rev_items.append('<h3 class="subhead">%s</h3>' % ("초성어（字母缩写）" if is_jamo else "가나다（音节词）"))
            last_jamo = is_jamo
        codes = "、".join(sorted(word_codes[text]))
        rev_items.append('<div class="entry rev" data-k="%s %s %s"><span class="hw">%s</span>'
                         '<span class="py">→ %s</span><span class="def">释义：%s</span></div>'
                         % (esc(text), esc(codes), esc(text_def(text)), esc(text), esc(codes), esc(text_def(text))))

    nav = "".join('<a href="#sec-%s">%s</a>' % (l, l) for l in sorted(body_sections))
    STATS = "收录韩文词条 %d 条 · 整句短语 %d 句" % (n_words, len(phrases))

    HTML = TEMPLATE.replace("__STATS__", STATS).replace("__DATE__", date.today().isoformat()) \
                   .replace("__NAV__", nav).replace("__BODY__", "\n".join(sec_html)) \
                   .replace("__PHRASES__", ph_items).replace("__REVERSE__", "\n".join(rev_items))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(HTML)
    print("OK:", OUT)
    print("STATS:", STATS)
    print("MISSING 释义数:", len(missing))
    return 1 if missing else 0


TEMPLATE = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>HanPinyin 词典</title>
<style>
  body{margin:0;background:#FBF7EE;color:#2B2620;font-family:"Noto Serif SC","Source Han Serif SC","SimSun","Malgun Gothic",serif;}
  .page{max-width:900px;margin:0 auto;padding:28px 22px 60px;}
  .titlepage{text-align:center;border-bottom:3px double #A32D2D;padding:34px 0 22px;margin-bottom:18px;}
  .titlepage h1{font-size:40px;margin:0;letter-spacing:6px;color:#A32D2D;}
  .titlepage .sub{font-size:15px;color:#6B6154;margin-top:10px;letter-spacing:2px;}
  .titlepage .stats{font-size:13px;color:#8A7F6E;margin-top:8px;}
  .toolbar{position:sticky;top:0;background:#FBF7EEF2;padding:10px 0;border-bottom:1px solid #D8CFC0;z-index:9;}
  .toolbar input{width:100%;box-sizing:border-box;padding:9px 14px;font-size:15px;border:1px solid #C9BFAE;border-radius:8px;background:#FFFDF8;font-family:inherit;color:inherit;}
  .nav{margin:10px 0 0;line-height:2;}
  .nav a{display:inline-block;min-width:26px;text-align:center;margin-right:4px;padding:2px 6px;border:1px solid #D8CFC0;border-radius:6px;text-decoration:none;color:#A32D2D;font-size:13px;background:#FFFDF8;}
  h2.letter{color:#A32D2D;font-size:26px;border-bottom:2px solid #A32D2D;padding-bottom:4px;margin:26px 0 12px;letter-spacing:3px;}
  h3.subhead{color:#6B6154;font-size:15px;margin:18px 0 8px;border-bottom:1px dashed #C9BFAE;padding-bottom:3px;}
  .cols{column-count:2;column-gap:26px;}
  @media (max-width:640px){.cols{column-count:1;}}
  .entry{break-inside:avoid;padding:5px 0;border-bottom:1px dotted #DDD3C2;}
  .hw{font-size:17px;font-weight:700;}
  .py{display:block;font-size:12.5px;color:#7A7060;margin-top:1px;}
  .typed{color:#A32D2D;margin-left:6px;}
  .def{display:block;font-size:13px;color:#1F4E79;margin-top:2px;}
  .cands{display:block;font-size:13px;color:#4A4238;margin-top:2px;}
  .appx{margin-top:44px;border-top:3px double #A32D2D;padding-top:14px;}
  .appx h2{font-size:20px;color:#A32D2D;letter-spacing:2px;margin:0 0 10px;}
  .guide{background:#FFFDF8;border:1px solid #D8CFC0;border-radius:10px;padding:14px 18px;font-size:14px;line-height:1.9;}
  .guide b{color:#A32D2D;}
  .fanli{font-size:13.5px;line-height:1.95;color:#4A4238;background:#FFFDF8;border:1px solid #D8CFC0;border-radius:10px;padding:12px 18px;margin:14px 0 6px;}
  .hide{display:none!important;}
  @media print{.toolbar{display:none;}.page{max-width:100%;}body{background:#fff;}}
</style>
</head>
<body>
<div class="page">
  <div class="titlepage">
    <h1>HanPinyin 词典</h1>
    <div class="sub">用汉语拼音打韩文 · 输入法已实装词汇总览（释义版）</div>
    <div class="stats">__STATS__ · 词库版本 v2.7 · __DATE__ 编成</div>
  </div>

  <div class="toolbar">
    <input id="q" type="search" placeholder="检索：中文意思（如 你好）、拼音（paiwei）或韩文（안녕）…" oninput="filter()">
    <div class="nav">__NAV__</div>
  </div>

  <div class="fanli">
    <b>凡例</b>　① 词条按<b>输入拼音的字母顺序</b>排列，韩文作词头；② 〔〕内为拼音音节，<span style="color:#A32D2D">打</span>后为实际键入的<b>连写码</b>（音节之间不打空格）；③ <b>释义</b>为该词对应的中文意思；④ 同一韩文词有多种打法时一并列出，任意一种都能打出；⑤ ①②③ 为输入法候选顺序；⑥ 词库支持模糊音（n↔l、zh↔z、ang↔an、f↔h 等，逐音节生效）与简拼/混拼（如 nihao 可打 nh、nih），常用词会自动学习上浮；⑦ 英文游戏用语直接打英文本身（gg、gank），多词短语连写（mid diff → 打 middiff）；⑧ 초성어（ㅇㅈ、ㅁㄹ 等字母缩写）按其中文意思的拼音查。
  </div>

  __BODY__

  <div class="appx" id="appx-phrases">
    <h2>附录一　整句短语篇</h2>
    <div class="cols">
__PHRASES__
    </div>
  </div>

  <div class="appx" id="appx-reverse">
    <h2>附录二　가나다역참조（看词查打法）</h2>
    <div class="cols">
__REVERSE__
    </div>
  </div>

  <div class="appx" id="appx-guide">
    <h2>附录三　新人上手指南</h2>
    <div class="guide">
      <b>第一步 · 部署</b>：把 <span style="font-family:monospace">sino_mix.schema.yaml</span> 与 <span style="font-family:monospace">hanpinyin.dict.yaml</span> 复制到 <span style="font-family:monospace">%APPDATA%\\Rime\\</span>（覆盖旧文件）。<br>
      <b>第二步 · 重新部署</b>：右键任务栏小狼毫（Weasel）托盘图标 →「重新部署」。注意：重启输入法 ≠ 重新部署，必须走托盘菜单。<br>
      <b>第三步 · 打字验证</b>：切到「韩文拼音 HanPinyin」，打 <b>nihao</b> 应出 안녕하세요；打 <b>lihao</b>（模糊音）也应出 안녕하세요；打 <b>paiwei</b> 出 랭크。能打出来即部署成功。<br>
      <b>日常更新</b>：拿到新词典后重复第一、二步，并用上面测试码验证。<br>
      <b>小提示</b>：翻页用 <b>- / =</b>（或 , / .）；常用词选过几次后会自动排到前面；重置自学习可删除 <span style="font-family:monospace">%APPDATA%\\Rime\\hanpinyin.userdb\\</span> 后重新部署。
    </div>
  </div>

  <div style="text-align:center;color:#8A7F6E;font-size:12px;margin-top:40px;">—— 全书终 · Generated from HanPinyin data sources ——</div>
</div>
<script>
function filter(){
  var q=document.getElementById('q').value.trim().toLowerCase();
  document.querySelectorAll('.entry').forEach(function(el){
    var k=el.getAttribute('data-k').toLowerCase();
    el.classList.toggle('hide', !!q && k.indexOf(q)<0);
  });
  document.querySelectorAll('section').forEach(function(el){
    el.style.display = el.querySelectorAll('.entry:not(.hide)').length? '':'none';
  });
  document.querySelectorAll('.appx').forEach(function(app){
    var n=app.querySelectorAll('.entry:not(.hide)').length;
    var hasGuide=app.querySelector('.guide');
    app.style.display=(n||hasGuide)?'':'none';
  });
}
</script>
</body>
</html>
"""

if __name__ == "__main__":
    sys.exit(main())
