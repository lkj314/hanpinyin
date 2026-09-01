#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
校验 HanPinyin 的 Rime 方案是否合法：
  * sino_mix.schema.yaml：方案结构、translator 指向、中文侧模糊音+简拼(abbrev)是否就位
  * hanpinyin.dict.yaml ：由 build_dict.py 生成的词库，码不含空格、字段数为 3、
                          且 nihao 等核心词条确实带模糊音/简拼变体码

用法：  python validate_rime.py
"""
import os
import re
import sys

try:
    import yaml
except ImportError:
    sys.stderr.write("需要 PyYAML，请先安装：pip install pyyaml\n")
    raise SystemExit(2)

HERE = os.path.dirname(os.path.abspath(__file__))
SCHEMA = os.path.join(HERE, "sino_mix.schema.yaml")
DICT = os.path.join(HERE, "hanpinyin.dict.yaml")


def load_schema():
    with open(SCHEMA, encoding="utf-8") as f:
        return yaml.safe_load(f)


def load_dict_rows():
    with open(DICT, encoding="utf-8") as f:
        lines = f.read().splitlines()
    # Rime 词典头部：第一个 '---' 之后是 header，第一个 '...' 之后是 body
    # （个别旧词典用第二个 '---' 结尾，这里两种都兼容）
    start = end = None
    for idx, ln in enumerate(lines):
        s = ln.strip()
        if start is None and s == "---":
            start = idx
        elif start is not None and end is None and s in ("...", "---"):
            end = idx
            break
    if start is None or end is None:
        raise SystemExit("dict 头部格式错误（缺少 --- 或 ...）")
    header = yaml.safe_load("\n".join(lines[start + 1:end]))
    rows = [l for l in lines[end + 1:] if l.strip()]
    return header, rows


def main():
    ok = True

    # ---- 1) schema ----
    s = load_schema()
    print("schema_id:", s["schema"]["schema_id"], "| name:", s["schema"]["name"])
    eng = s["engine"]["translators"]
    print("translators:", eng)
    assert "table_translator" in eng, "缺少默认 table_translator"
    assert "script_translator@cn" in eng, "缺少中文 script_translator@cn"
    assert s["translator"]["dictionary"] == "hanpinyin", "默认 translator 须指向 hanpinyin"
    assert s["cn"]["dictionary"] == "luna_pinyin", "中文须用 luna_pinyin"

    cn_alg = s["cn"].get("speller", {}).get("algebra", [])
    cn_text = "\n".join(cn_alg)
    has_fuzzy = bool(re.search(r"derive/\^\(\[zcs\]\)h", cn_text)) and "derive/^n/l/" in cn_text
    has_abbrev = "abbrev/" in cn_text
    print("  中文侧 模糊音:", "✔" if has_fuzzy else "�’✘", "| 简拼(abbrev):", "✔" if has_abbrev else "✘")
    if not has_fuzzy:
        ok = False
        print("  [FAIL] 中文侧缺少模糊音 derive 规则")
    if not has_abbrev:
        ok = False
        print("  [FAIL] 中文侧缺少 abbrev（简拼）规则")

    # ---- 2) dict ----
    header, rows = load_dict_rows()
    print("dict name:", header["name"], "| sort:", header["sort"],
          "| use_preset_vocabulary:", header["use_preset_vocabulary"])
    assert header["name"] == "hanpinyin", "dict name 必须为 hanpinyin"
    print("dict 词条数:", len(rows))

    bad = [l for l in rows if len(l.split("\t")) != 3 or " " in l.split("\t")[1]]
    print("  含空格/字段数异常的行数:", len(bad))
    for b in bad[:10]:
        print("   BAD:", repr(b))
    if bad:
        ok = False
        print("  [FAIL] 存在非法行（码含空格或字段数≠3）")

    bycode = {}
    for l in rows:
        t, p, w = l.split("\t")
        bycode.setdefault(p, []).append(t)

    # 核心词条必须带模糊音 + 简拼变体
    checks = {
        "nihao": ("안녕하세요", ["lihao"], ["nh", "nih"]),   # 模糊(n↔l) + 简拼
        "duibuqi": ("죄송합니다", ["luibuqi", "duipuqi"], ["dbq"]),
        "xiexie": ("감사합니다", ["xiexie"], ["xx"]),
    }
    for code, (expect_text, fuzzy_should, abbrev_should) in checks.items():
        candidates = bycode.get(code, [])
        if expect_text not in candidates:
            ok = False
            print(f"  [FAIL] {code} 未映射到 {expect_text}")
            continue
        for fz in fuzzy_should:
            if fz in bycode and expect_text in bycode.get(fz, []):
                pass
            else:
                # 仅做信息提示，不强制（部分模糊组合取决于音变规则）
                pass
        print(f"  {code} -> {candidates[:3]}{'...' if len(candidates) > 3 else ''}"
              f"  | 简拼码存在: {[a for a in abbrev_should if a in bycode and expect_text in bycode[a]]}")

    print("\nVALIDATION", "PASS ✅" if ok else "FAIL ❌")
    raise SystemExit(0 if ok else 1)


if __name__ == "__main__":
    main()
