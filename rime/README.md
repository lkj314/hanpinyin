# HanPinyin × 小狼毫（Rime）部署与维护指南

把 HanPinyin 做成「配置驱动」的输入法：拼音 → 候选窗出**韩文**（专精版，无中文候选）。
词典是纯文本 `hanpinyin.dict.yaml`，**改词库 = 改数据 → 跑脚本 → GUI 重新部署**，无需碰 C++。

> ⛔ 铁律：**部署只能走小狼毫自己的 GUI**（右键托盘图标 →「重新部署」）。
> 不要用任何脚本去复制/重启/部署——重启进程 ≠ 重新部署，运行时只读编译后的 `build\*.bin`。

## 一、安装小狼毫（仅需一次）

1. 下载小狼毫 Weasel：<https://rime.im> 或 GitHub `rime/weasel` Releases（装最新版）。
2. 默认安装即可（2026-09-26 起本方案不再依赖其中的中文词库）。

## 二、首次部署

1. 复制这两个文件到小狼毫用户目录 `%APPDATA%\Rime\`（即 `C:\Users\<你>\AppData\Roaming\Rime\`）：
   - `rime/sino_mix.schema.yaml`（方案定义，v2.4）
   - `rime/hanpinyin.dict.yaml`（生成的韩文词典）
2. 右键任务栏小狼毫托盘图标 → **「重新部署」**（等几秒即可，旧编译产物 `*.table.bin` / `*.prism.bin` 会自动重建）。
3. 若输入法列表里没有「韩文拼音 HanPinyin」：右键托盘 → **「输入法设定」** 勾选它；
   或编辑 `%APPDATA%\Rime\default.custom.yaml`，在 `patch.schema_list:` 下加一行 `- schema: sino_mix`，保存后重新部署。

## 三、验证（打字测试）

用 `Win + 空格` 切到「韩文拼音 HanPinyin」，记事本里打：

| 敲的字母 | 应出现（首位） | 说明 |
|---|---|---|
| `nihao` | 안녕하세요 | 规范码 |
| `lihao` | 안녕하세요 | 模糊音 n↔l |
| `nh` / `nih` | 안녕하세요 | 简拼/混拼 |
| `dbq` | 죄송합니다 | 速记码（对不起） |
| `pw` | 랭크 | 速记码（排位） |
| `ht` / `zb` | 한타 / 템 | 速记码（团战/装备） |
| `gank` | gank | 英文黑话直打（键=英文本身） |
| `zmb` | 어떡해 | 怎么办（情绪词） |

能打出来 = 新版已生效。候选窗里**不应再出现任何中文**（2026-09-26 起）。

## 四、以后更新词库

1. 改数据源：
   - 批量词条：`data/main_dict.json`（拼音空格分隔音节）或 `data/phrases.json`（整句）
   - 韩文基石高频词：`rime/extra_phrases.txt`（拼音连写，`词条<TAB>拼音<TAB>权重`，`#` 注释；**只允许韩文与游戏缩写**）
   - 速记码：`data/shortcodes.tsv`（`码<TAB>拼音<TAB>指定韩文可空<TAB>备注`，权重 110 保证首位）
   - 释义：`data/meanings.tsv`（词典用，加词需同步补一行）
2. 重新生成并校验：
   ```
   cd rime
   python build_dict.py          # 生成 hanpinyin.dict.yaml
   python verify_regression.py   # 数据质量 + 回归点 + 速记码首位 + schema 形态（必须 PASS）
   python validate_rime.py       # 方案结构校验（需 PyYAML）
   python ../docs/gen_dictionary.py   # 《HanPinyin 词典》再版（EXIT=0 = 释义零缺失）
   ```
3. 把 `sino_mix.schema.yaml` + `hanpinyin.dict.yaml` 复制到 `%APPDATA%\Rime\` → 托盘「重新部署」→ 打字测试。

## 五、方案形态（2026-09-26 专攻韩文版，改 schema 前必懂）

只有一个 translator：

| translator | 词典 | 码型 | 说明 |
|---|---|---|---|
| 默认 `translator`（table_translator） | `hanpinyin.dict.yaml` | **连写码**（`nihao`，无 `'`） | 韩文词 + 英文黑话（键=英文本身）。**必须挂默认 translator**，部署阶段只编译它 |

关键开关（均有 schema 断言守护，`verify_regression.py` 会检查）：

- `script_translator@cn` **已移除**——实测中文候选占候选窗 75%，全是谐音噪音；中文交给用户的搜狗。
- `enable_completion: false`——实测补全会把无关长码拉进候选窗（打 `hao` 冒出 짜증나），且补全只 -1 质量几乎无效（quality = exp(weight) ± 小量）。
- `enable_encoder: true` + `encode_commit_history: true`——**长句自学习**：连续上屏内容自动编码成用户词组（候选带 ☯），之后敲开头字母可整句调出。查证自 librime `table_translator.cc`，无需 `enable_sentence`。
- 排序体系：质量 = exp(词典权重)；速记码 110 > 基石词 100 > 主词条 10~14 > 变体码 5~9。`initial_quality` 影响可忽略（此前"抬高起始质量"的注释是错的，已删）。

排坑记录：韩文词典若挂在命名 translator（如 `table_translator@han`）上，部署阶段根本不会编译它，运行时报
`Error loading table for dictionary 'hanpinyin'` 且永远打不出韩文——所以必须 `translator.dictionary: hanpinyin`。

自学习说明（2026-09-06 查证 librime 源码）：`charset_filter` 只滤 CJK 扩展区/兼容表意文字，
韩文谚文（U+AC00–D7AF）、假名、拉丁字母全部放行，`enable_user_dict: true` 安全。

## 六、目录说明

```
rime/
  sino_mix.schema.yaml  # 方案定义 v2.4（纯韩文输出：无中文 translator、无补全、开长句自学习）
  hanpinyin.dict.yaml   # 生成的韩文词典（约 10246 码，勿手改）
  build_dict.py         # 数据源 -> 词典生成器（模糊音 + 简拼/混拼 + 速记码 + 非韩文过滤）
  verify_regression.py  # 数据质量 + 构建覆盖 + 回归点 + 速记码首位 + schema 形态校验（改词库后必跑）
  validate_rime.py      # 方案/词典结构校验（需 PyYAML）
  _bench_panel.py       # 候选面板体检：量化每屏韩文/中文/日文占比与韩文位次
                        #   用法：python _bench_panel.py [--fresh]（调权重/改配置前后各跑一遍对比）
  _diag_select.py       # 端到端「能否选中上屏」诊断：临时目录隔离，绝不碰用户输入法数据
                        #   用法：python _diag_select.py zmb pw
                        #        python _diag_select.py --fresh pw   # 用仓库最新词典重编译再测
                        #   判据：按 1 选词后读 RimeGetCommit，看是否真的上屏
  extra_phrases.txt     # 韩文基石高频词（权重 100；只允许韩文与游戏缩写）
```

## 七、常见问题

- **候选顺序不对？** 调 `main_dict.json` / `extra_phrases.txt` 的权重（注意 exp 放大：权重差 1 = 质量差 e 倍）；
  速记码首位由 `data/shortcodes.tsv` + 权重 110 保证，改完跑 `verify_regression.py` 有首位断言。
- **改了 data 但候选没变？** 确认 `build_dict.py` 已重跑、`hanpinyin.dict.yaml` 已复制到 `%APPDATA%\Rime\`、且执行了**「重新部署」**（重启进程不算）。
- **自学习想清零重学？** 删除 `%APPDATA%\Rime\hanpinyin.userdb\` 后重新部署。
- **想临时打中文？** 直接切回系统里的搜狗/QQ 拼音（`Win+空格`）——本方案设计上不输出中文。

## 八、血泪教训：拼音码**绝不能含空格**

小狼毫 `table_translator` 的码（拼音）里不能出现空格——空格是上屏/确认键，一旦输入空格就提交当前候选，多音节码永远打不全。所以 **`bu hao yi si` 这种带空格的拼音是错的、不可达的**，必须拼成连贯码 `buhaoyisi`（与已验证可用的 `nihao` / `duibuqi` / `meiguanxi` 风格一致）。

`build_dict.py` 在生成阶段负责把 `data/*.json` 的空格分隔拼音拼成连贯码；`extra_phrases.txt` 里的拼音请直接写连贯码。改完跑 `python validate_rime.py` 可自动检出「含空格/字段数异常」的行。
