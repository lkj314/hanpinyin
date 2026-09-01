# HanPinyin · 用汉语拼音打韩文的输入法（基于小狼毫 / Rime）

> 一句话：你敲**中文拼音**，候选窗同时给出**韩文（及日/英/中）**，在任意窗口（韩服游戏聊天框、浏览器、微信…）里像正常输入法一样选词上屏。

> 本输入法**套壳小狼毫（Rime / Weasel）**实现——直接复用成熟、已能在 Windows 注册为系统输入法的小狼毫引擎，只魔改了词库与少量拼音规则。自研的 EXE / TSF 代码（`src/`）只是早期未完成的实验，**并未上线**，请勿依赖。

---

## 这是什么

HanPinyin 是一个 Windows 上的「拼音 → 韩文」输入方案。主要给在韩服《英雄联盟》等游戏里玩、但不熟悉韩文键盘布局（두벌식）的中国玩家：不用记键位、不用切到韩文输入法，照常打拼音（`nihao`、`duibuqi`），候选栏就列出对应的韩文（`안녕하세요`、`죄송합니다`），像普通输入法一样选词上屏。

核心资产是**词库 + 拼音规则**：拼音到韩文 / 中文 / 日 / 英的映射，以及模糊音、简拼等拼音容错规则。

---

## 真实运行形态（请据此理解项目）

| 形态 | 说明 | 现状 |
|------|------|------|
| **① 小狼毫 / Rime 方案（本仓库真正在跑的）** | 把 HanPinyin 做成 Rime 的 `sino_mix` 方案：拼音 → 候选窗同时出中文 + 韩文（及日/英）。部署即用，已能在系统输入法列表里出现。 | **可用，主力**。代码在 `rime/`。 |
| ② 自研程序（EXE / TSF DLL，`src/`） | 早期尝试自己写输入法核心与 Windows TSF 注册。 | **未完工、未上线**。自研 TSF 难以注册进系统输入法列表，已放弃；仅作历史参考。 |

> 简单说：**词库与拼音规则（在 `rime/`）是唯一在用的部分**；`src/` 是自研探索的残留，不要把文档里的 EXE / TSF 当作现状。

---

## 快速开始（小狼毫方案）

### 1. 安装小狼毫
下载 Weasel（https://rime.im），安装时自带 `luna_pinyin`（中文拼音词库，本方案的中文候选来自它）。

### 2. 部署本项目
最简单：
```bat
cd HanPinyin\rime
python deploy.py
```
它会把 `sino_mix.schema.yaml` + 生成的 `hanpinyin.dict.yaml` 复制到 `%AppData%\Rime\` 并提示你「重新部署」。

### 3. 重新部署
右键任务栏小狼毫图标 → **「重新部署」**。

### 4. 使用
`Win+空格` 切到「韩文拼音 HanPinyin」，记事本里打 `nihao` → 候选窗出现 `안녕하세요` / `你好`，数字键或空格选词。

---

## 输入体验（搜狗式容错）

为更接近搜狗的"流畅输入"，本方案支持：

- **模糊拼音（逐音节）**：`zh↔z`、`ang↔an`、`n↔l`、`f↔h` 等，且对韩文多音节词**每个音节**都生效（由 `build_dict.py` 预先生成模糊码）。例如 `lihao` 也能出 `안녕하세요`。
- **简拼 / 混拼**：打 `nh` 出 `nihao` → `안녕하세요`，打 `nih` 出 `你好`。中文侧由 `cn.speller` 的 `abbrev` 规则实现，韩文侧由 `build_dict.py` 生成简拼码实现。

---

## 词库怎么扩充

词库就是数据，改了重跑脚本即可，无需碰 C++：

- `data/main_dict.json` —— 主词库，拼音**用空格分隔音节**：`{ "pinyin": "ni hao", "candidates": [["안녕하세요", 20]] }`
- `data/phrases.json` —— 整句短语：`{ "pinyin": "ni hao", "korean": "안녕하세요" }`
- `rime/extra_phrases.txt` —— 策划的多语言常用词条（韩/日/英/中混排），拼音为连贯码（如 `nihao`）。

改完：
```bat
cd HanPinyin\rime
python build_dict.py     # 重新生成 hanpinyin.dict.yaml
python deploy.py         # 复制到 %AppData%/Rime/
```
再到小狼毫「重新部署」。

---

## 目录结构

```
HanPinyin/
├── README.md
├── data/                 # 词库与 schema（核心资产）
│   ├── main_dict.json    # 主词库（拼音空格分隔音节）
│   ├── phrases.json      # 整句短语库
│   └── schema.md
├── rime/                 # 真正在跑的输入法方案（小狼毫 / Rime）
│   ├── sino_mix.schema.yaml  # 方案定义（中文 luna_pinyin + 韩文自定义词库 + 模糊音/简拼规则）
│   ├── build_dict.py         # 数据源 -> hanpinyin.dict.yaml 生成器（含模糊音/简拼码生成）
│   ├── deploy.py             # 一键部署到 %AppData%/Rime/
│   ├── validate_rime.py      # 校验 schema/dict 合法性
│   ├── extra_phrases.txt     # 策划多语言词条
│   └── hanpinyin.dict.yaml   # 由 build_dict.py 生成的词库（勿手改）
└── src/                  # 自研实验代码（EXE / TSF），未上线，仅供参考
```

---

## 已知限制 / 诚实说明

- 仅 Windows 10/11 x64（依赖小狼毫 Weasel）。
- 模糊音 / 简拼在韩文侧由预生成码实现，覆盖常用音变；极偏方言音变未全量枚举。
- `src/` 自研路线未完工，请勿使用。

---

## License

待定（请仓库所有者补充）。
