# HanPinyin × 小狼毫（Rime）部署与维护指南

把 HanPinyin 做成「配置驱动」的输入法：拼音 → 候选窗同时出中文 + 韩文（及日/英）。
词典是纯文本 `hanpinyin.dict.yaml`，**改词库 = 改数据 → 跑脚本 → GUI 重新部署**，无需碰 C++。

> ⛔ 铁律：**部署只能走小狼毫自己的 GUI**（右键托盘图标 →「重新部署」）。
> 不要用任何脚本去复制/重启/部署——重启进程 ≠ 重新部署，运行时只读编译后的 `build\*.bin`。

## 一、安装小狼毫（仅需一次）

1. 下载小狼毫 Weasel：<https://rime.im> 或 GitHub `rime/weasel` Releases（装最新版）。
2. 安装时默认会带上 `luna_pinyin`（中文拼音词库），本项目的中文候选就来自它。

## 二、首次部署

1. 复制这两个文件到小狼毫用户目录 `%APPDATA%\Rime\`（即 `C:\Users\<你>\AppData\Roaming\Rime\`）：
   - `rime/sino_mix.schema.yaml`（方案定义）
   - `rime/hanpinyin.dict.yaml`（生成的多语词典）
2. 右键任务栏小狼毫托盘图标 → **「重新部署」**（等几秒即可，旧编译产物 `*.table.bin` / `*.prism.bin` 会自动重建）。
3. 若输入法列表里没有「韩文拼音 HanPinyin」：右键托盘 → **「输入法设定」** 勾选它；
   或编辑 `%APPDATA%\Rime\default.custom.yaml`，在 `patch.schema_list:` 下加一行 `- schema: sino_mix`，保存后重新部署。

## 三、验证（打字测试）

用 `Win + 空格` 切到「韩文拼音 HanPinyin」，记事本里打：

| 敲的字母 | 应出现的候选 |
|---|---|
| `nihao` | 안녕하세요 / こんにちは / hello / 你好 |
| `lihao` | 안녕하세요（模糊音 n↔l） |
| `nh` / `nih` | 你好 / 안녕하세요（简拼/混拼） |
| `dbq` | 죄송합니다 |
| `paiwei` | 랭크 |

能打出来 = 新版已生效。

## 四、以后更新词库

1. 改数据源：
   - 批量词条：`data/main_dict.json`（拼音空格分隔音节）或 `data/phrases.json`（整句）
   - 少量常用多语言词条：`rime/extra_phrases.txt`（拼音连写，`词条<TAB>拼音<TAB>权重`，`#` 注释）
2. 重新生成并校验：
   ```
   cd rime
   python build_dict.py          # 生成 hanpinyin.dict.yaml
   python verify_regression.py   # 数据质量 + 回归点校验（必须 PASS）
   python validate_rime.py       # 方案结构校验（需 PyYAML）
   ```
3. 把 `sino_mix.schema.yaml` + `hanpinyin.dict.yaml` 复制到 `%APPDATA%\Rime\` → 托盘「重新部署」→ 打字测试。

## 五、方案里的两个 translator（改 schema 前必懂）

| translator | 词典 | 码型 | 说明 |
|---|---|---|---|
| 默认 `translator`（table_translator） | `hanpinyin.dict.yaml` | **连写码**（`nihao`，无 `'`） | 韩/英/多语自定义词。**必须挂默认 translator**，部署阶段只编译它 |
| `script_translator@cn` | luna_pinyin | 音节分隔码（`ni'hao`） | 中文候选，复用官方已编译 .bin |

排坑记录：韩文词典若挂在命名 translator（如 `table_translator@han`）上，部署阶段根本不会编译它，运行时报
`Error loading table for dictionary 'hanpinyin'` 且永远打不出韩文——所以必须 `translator.dictionary: hanpinyin`。

自学习说明（2026-09-06 查证 librime 源码）：`charset_filter` 只滤 CJK 扩展区/兼容表意文字，
韩文谚文（U+AC00–D7AF）、假名、拉丁字母全部放行，因此两个 translator 均已开启
`enable_user_dict: true`（自学习），韩文侧另开 `enable_completion: true`（前缀补全）。

## 六、目录说明

```
rime/
  sino_mix.schema.yaml  # 方案定义（中文 luna_pinyin + 韩文自定义词库 + 模糊音/简拼规则）
  hanpinyin.dict.yaml   # 生成的多语词典（约 7088 码，勿手改）
  build_dict.py         # 数据源 -> 词典生成器（含逐音节模糊音 + 简拼/混拼码生成）
  verify_regression.py  # 数据质量 + 构建覆盖 + 回归点校验（改词库后必跑）
  validate_rime.py      # 方案/词典结构校验（需 PyYAML）
  _verify_ctx.py        # 用 rime.dll 直连引擎做端到端候选验证（开发期工具）
  extra_phrases.txt     # 补充多语言词条（策划手编）
```

## 七、常见问题

- **韩文没排在中文前面？** 调高 `main_dict.json` / `extra_phrases.txt` 里的权重，或增大 `sino_mix.schema.yaml` 中 `translator:` 的 `initial_quality`。
- **想做成纯韩文输入法（不带中文）？** 把 `sino_mix.schema.yaml` 的 `engine.translators` 里 `script_translator@cn` 一行删掉即可。
- **改了 data 但候选没变？** 确认 `build_dict.py` 已重跑、`hanpinyin.dict.yaml` 已复制到 `%APPDATA%\Rime\`、且执行了**「重新部署」**（重启进程不算）。
- **自学习想清零重学？** 删除 `%APPDATA%\Rime\hanpinyin.userdb\`（或 `luna_pinyin.userdb\`）后重新部署。

## 八、血泪教训：拼音码**绝不能含空格**

小狼毫 `table_translator` 的码（拼音）里不能出现空格——空格是上屏/确认键，一旦输入空格就提交当前候选，多音节码永远打不全。所以 **`bu hao yi si` 这种带空格的拼音是错的、不可达的**，必须拼成连贯码 `buhaoyisi`（与已验证可用的 `nihao` / `duibuqi` / `meiguanxi` 风格一致）。

`build_dict.py` 在生成阶段负责把 `data/*.json` 的空格分隔拼音拼成连贯码；`extra_phrases.txt` 里的拼音请直接写连贯码。改完跑 `python validate_rime.py` 可自动检出「含空格/字段数异常」的行。
