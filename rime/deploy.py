#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HanPinyin -> 小狼毫(Weasel) 一键部署
====================================
1) 备份 %AppData%/Rime/ 下的旧 sino_mix.* 为 *.v1bak.<时间戳>
2) 复制 rime/ 下的 sino_mix.schema.yaml + sino_mix.dict.yaml 过去
3) 提示用户去小狼毫托盘「重新部署」

用法：  python deploy.py
(改完 data/*.json 或 extra_phrases.txt 后，先跑 build_dict.py 再跑本脚本)
"""
import os
import shutil
import time

RIME_DIR = os.path.dirname(os.path.abspath(__file__))
APP_RIME = os.path.expandvars(r"%AppData%/Rime")
SCHEMA = "sino_mix.schema.yaml"
DICT = "hanpinyin.dict.yaml"
TS = time.strftime("%Y%m%d%H%M%S")


def deploy_one(name):
    src = os.path.join(RIME_DIR, name)
    dst = os.path.join(APP_RIME, name)
    if not os.path.exists(src):
        print(f"[SKIP] 源文件缺失: {src}")
        return
    if os.path.exists(dst):
        bak = f"{dst}.v1bak.{TS}"
        shutil.copy2(dst, bak)
        print(f"[BACKUP] {dst} -> {bak}")
    shutil.copy2(src, dst)
    print(f"[DEPLOY] {src} -> {dst}")


if __name__ == "__main__":
    print(f"目标目录: {APP_RIME}")
    if not os.path.isdir(APP_RIME):
        print("[ERROR] 未找到小狼毫用户目录，请先安装小狼毫 Weasel (https://rime.im)")
        raise SystemExit(1)
    deploy_one(SCHEMA)
    deploy_one(DICT)
    print("\n完成。下一步：")
    print("  1) 若是首次，编辑 %AppData%/Rime/default.yaml 的 schema_list: 加一行  - schema: sino_mix")
    print("  2) 右键任务栏小狼毫图标 -> 『重新部署』(Deploy)")
    print("  3) Win+空格 切到 『韩文拼音 HanPinyin』，打 nihao 验证：안녕하세요 / 你好")
