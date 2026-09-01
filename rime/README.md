# HanPinyin × 小狼毫（Rime）部署指南

把 HanPinyin 做成「配置驱动」的输入法：拼音 → 候选窗同时出中文 + 韩文（及日/英）。
词典就是纯文本 `sino_mix.dict.yaml`，**改词库 = 改数据 → 跑脚本 → 重新部署**，无需碰 C++。

## 一、安装小狼毫（仅需一次）
1. 下载小狼毫 Weasel：https://rime.im 或 GitHub `rime/weasel` Releases（装最新版）。
2. 安装时默认会带上 `luna_pinyin`（中文拼音词库），本项目的中文候选就来自它。

## 二、部署本项目
> **本机当前状态（2026-08-31）**：`%AppData%\Rime\` 已是小狼毫用户目录，但小狼毫进程当前未运行、标准部署器也不在 PATH。本项目 v2 的 `sino_mix.schema.yaml` + `sino_mix.dict.yaml` **已预先复制**到该目录（旧版已备份为 `*.v1bak.*`）。你只需安装/启动小狼毫并「重新部署」即可生效。

最简单：跑一键脚本（自动备份旧文件 + 复制 + 给提示）
```
cd U:\SRF\HanPinyin\rime
python deploy.py
```
手动也行，把 `rime/` 下两个文件复制到 `%AppData%\Rime\`：

- 源：`U:\SRF\HanPinyin\rime\sino_mix.schema.yaml`
- 源：`U:\SRF\HanPinyin\rime\sino_mix.dict.yaml`
- 目标：`%AppData%\Rime\`（即 `C:\Users\<你>\AppData\Roaming\Rime\`）

> 若之前已有同名文件，直接覆盖即可（旧编译产物 `*.table.bin` / `*.prism.bin` 会在重新部署时自动重建）。

可选：若切换输入法时看不到「韩文拼音 HanPinyin」，把 schema 加进方案列表：
编辑 `%AppData%\Rime\default.custom.yaml`，在 `patch.schema_list:` 下加一行
```
  - schema: sino_mix
```
（本机 `%AppData%\Rime\default.custom.yaml` 已包含该条目，通常无需改动。）保存后重新部署。

## 三、重新部署
右键任务栏小狼毫图标 → **「重新部署」(Deploy)**。
（部署会编译 `sino_mix.dict.yaml` 生成 `sino_mix.table.bin`，并构建拼音 prism。）

## 四、切换与使用
- 用 `Win + 空格` 或输入法切换键，切到 **「韩文拼音 HanPinyin」**。
- 记事本里打 `nihao` → 候选窗应出现 `안녕하세요`（韩）、`你好`（中）等，用数字/空格选词。
- 打 `xiexie` → `감사합니다`；打 `wo` → `저`（韩）/ `我`（中）并排。

## 五、以后更新词库（核心优势）
1. 改数据源：
   - 批量词条：编辑 `U:\SRF\HanPinyin\data\main_dict.json` 或 `data\phrases.json`
   - 少量常用多语言词条：编辑 `U:\SRF\HanPinyin\rime\extra_phrases.txt`
2. 重新生成词库：
   ```
   cd U:\SRF\HanPinyin\rime
   python build_dict.py
   ```
3. 把新生成的 `sino_mix.dict.yaml` 复制到 `%AppData%\Rime\`，重新部署即可。

## 六、目录说明
```
HanPinyin/
  data/
    main_dict.json     # 项目唯一数据源：拼音 -> [[韩文, 词频], ...]（~608 条）
    phrases.json       # 项目唯一数据源：拼音 -> 韩文（42 条短语）
  rime/
    sino_mix.schema.yaml  # 输入法定义（中文 luna_pinyin + 韩文自定义词库）
    build_dict.py        # 数据源 -> sino_mix.dict.yaml 的生成器（去掉空格、按权重排序、自检）
    deploy.py            # 一键部署：备份旧文件 + 复制到 %AppData%/Rime/
    validate_rime.py     # 校验 schema/dict 合法性与"码不含空格"
    extra_phrases.txt    # 策划的多语言常用词条（韩/日/英/中混排）
    sino_mix.dict.yaml   # 由 build_dict.py 生成的词库（勿手改）
    README.md            # 本文件
```

## 七、常见问题
- **韩文没排在中文前面？** 调高 `extra_phrases.txt` / `main_dict.json` 里的权重，或增大 `sino_mix.schema.yaml` 中 `han:` 块的 `initial_quality`。
- **想做成纯韩文输入法（不带中文）？** 把 `sino_mix.schema.yaml` 的 `engine.translators` 里 `script_translator` 一行删掉即可。
- **改了 data 但候选没变？** 确认 `build_dict.py` 已重跑、`sino_mix.dict.yaml` 已复制到 `%AppData%\Rime\`、且执行了「重新部署」。

## 八、血泪教训：拼音码**绝不能含空格**
小狼毫 `table_translator` 的码（拼音）里不能出现空格——空格是上屏/确认键，一旦输入空格就提交当前候选，多音节码永远打不全。所以 **`bu hao yi si` 这种带空格的拼音是错的、不可达的**，必须拼成连贯码 `buhaoyisi`（与已验证可用的旧版 `nihao` / `duibuqi` / `meiguanxi` 风格一致）。

`build_dict.py` 的 `norm_pinyin()` 已负责去掉空格；`extra_phrases.txt` 里的拼音也请勿写空格。改完跑 `python validate_rime.py` 可自动检出"含空格/字段数异常"的行。
