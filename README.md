# HanPinyin · 用汉语拼音打韩文的输入法

> 一句话：你敲**中文拼音**，它给出**韩文**候选词，选中后一键发到当前窗口（比如韩服游戏聊天框）。

---

## 这是什么

HanPinyin 是一个 **Windows 上的拼音 → 韩文** 输入工具，主要给在**韩服《英雄联盟》等游戏**里玩、但不熟悉韩文键盘布局（두벌식）的中国玩家用。

你不用记韩文键位，也不用切到韩文输入法——照常打拼音（例如 `ni hao`、`wan le`），候选栏就会列出对应的韩文（`안녕하세요`、`완료`），用数字键或鼠标选一个，工具自动把它发到当前激活的输入框。

核心资产是**词库**：拼音到韩文的映射表。目前主词库 `data/main_dict.json` 有 **816 条**、整句短语库 `data/phrases.json` 有 **80 条**，覆盖问候、感谢、情绪、餐饮、购物、时间、方位、工作、天气、网络语、亲属、请求、邀约、联络等日常场景，敬语（요/습니다）与平语（반말）双层级都有。

---

## 输入能力（拼音 → 韩文）

- **全拼**：`ni hao` → `안녕하세요`，多音节整句按词频 + Viterbi 组词排序。
- **模糊拼音（默认开启，配置面板可关）**：常见发音混淆一键互通，不用死记标准拼音。当前支持 **12 组**：
  `zh↔z`、`ch↔c`、`sh↔s`、`ang↔an`、`eng↔en`、`ing↔in`、`iang↔ian`、`uang↔uan`、`n↔l`、`r↔l`、`f↔h`、`k↔g`。
  例：打 `zong` 也能出 `zhong` 开头的词；打 `xiang` 与 `xian` 互通。
- **简拼 + 混合简拼**：可只打每字首字母（如 `nh` → `ni hao`），也可**混合**（如 `nhao` / `nih` → `ni hao`），不必打全拼。
- **用户词频自学习**：选中过的候选会被提升，越用越顺手。

> 想加更多模糊组？编辑 `config.json` 的 `fuzzyPairs`（或配置面板）即可，无需改代码。

---

## 两种运行形态（请知悉现状）

代码库历史上存在过两条路线，但**当前构建目标只产出两种产物**：

| 形态 | 说明 | 现状 |
|------|------|------|
| **① 系统输入法（TSF DLL）** `hanpinyin_tsf.dll` | 注册成 Windows 文本服务框架（TSF）输入法，出现在系统"添加键盘"列表里，像正常输入法一样用（上屏走 `ITfRange::SetText` 官方通道）。这是**当前唯一的真正输入法**。 | 已构建。注册表曾缺 `Category` 命名值与 `Associations` 关联键，导致注册成功却不出现在列表——**本版已修复**，重建 + 重新注册后应正常出现（待你本地验证）。 |
| **② 设置 / 注册程序** `hanpinyin_config.exe` | 系统托盘 + 调 `regsvr32` 自注册/卸载 TSF + 配置面板（模糊音开关 / 热键 / 词库路径）。 | 已构建，配套使用。 |
| ⚠️ `build/Release/hanpinyin.exe` | **旧的"全局键盘钩子 EXE"架构残留二进制**，已不在任何 CMake 构建目标里。 | **陈旧、请勿继续作为主力**——它早于模糊拼音等功能，用它会感觉"模糊音为零"。如仍在用请改投 TSF。 |

> 简单说：**真正能用的输入法是 `hanpinyin_tsf.dll`**（TSF）；`hanpinyin.exe` 是历史遗留，应退役。本仓库重心是词库与核心逻辑（trie / 模糊音 / 候选管理 / Viterbi）。

---

## 快速开始（以 TSF 形态为准；旧 EXE 路线已废弃）

> 当前真正可用的输入法是 TSF（`hanpinyin_tsf.dll`）。下方编译出的 `hanpinyin.exe` 是历史残留，**不要用它**——它没有模糊音等功能。请编译 `hanpinyin_tsf.dll` + `hanpinyin_config.exe`，用配置程序注册 TSF 后，在系统"语言设置 → 键盘"里启用 HanPinyin。

### 编译（MSVC 14.x / Visual Studio 2022，x64）

```bat
cd HanPinyin
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

生成物：`build/Release/hanpinyin.exe`（x64）。**零第三方运行时依赖**（不引入 Boost / Qt / nlohmann-json，JSON 解析是自研 header-only）。

> 纯逻辑核心单测（`src/core` 不含 `windows.h`）可单独用 `cl` 编译，详见 `docs/` 与各 `tests/test_*.cpp`。

### 运行

1. 启动 `hanpinyin.exe`（建议管理员权限，全局钩子更稳）。
2. **Ctrl+Space** 切换激活（图标 🔤 / 关闭）。
3. 激活后输入拼音（如 `wan le yi xia wu`），**空格**确认并弹候选。
4. **数字键 1~5** 选择；**Tab** 翻页；**鼠标点击** 直接选；**Enter** 发首个候选；**Esc** 取消。
5. 选定后韩文自动发到当前激活窗口，按游戏内回车发送。
6. **Ctrl+Alt+P** 打开配置面板（增删词条 / 模糊音 / 热键）。

---

## 词库怎么扩充

词库就是两个 JSON，纯数据、改了即时生效（配置面板也能图形化增删）：

- `data/main_dict.json` —— 主词库，数组，每项为：
  ```json
  { "pinyin": "wan le", "candidates": [["완료", 12], ["완료됐어", 5]] }
  ```
  - `pinyin`：拼音键，**空格分隔音节、全小写**（如 `"duo shao qian"`），装载时拆成音节并生成首字母缩写键。
  - `candidates`：`[韩文字符串, 词频整数]`，**词频越大越靠前**。
- `data/phrases.json` —— 整句短语库，每项为 `{ "pinyin": "...", "korean": "..." }`（装载时词频强制为 100，整句优先）。
- 格式细节见 `data/schema.md`。**文件一律 UTF-8 无 BOM、候选按词频降序**。

> 词库的韩文标注早期由模型生成，建议自行校验并在面板修正/增补。

---

## 目录结构

```
HanPinyin/
├── CMakeLists.txt
├── README.md
├── data/                 # 词库与 schema（UTF-8 无 BOM）—— 项目的核心资产
│   ├── main_dict.json    # 主词库（816 条）
│   ├── phrases.json      # 整句短语库（80 条）
│   ├── schema.md         # 词库格式说明
│   └── user_dict.json    # 运行时生成，记录你的常用词
├── src/
│   ├── core/             # 纯逻辑核心层（严禁 windows.h）：trie / 词典 / 拼音分词 / 模糊音 / 候选管理 / Viterbi / 用户词库
│   ├── platform/         # Win32 / 悬浮窗 UI（Direct2D+DirectWrite）/ 配置面板
│   ├── app/main.cpp      # 入口
│   └── tsf/              # 实验性：TSF 文本服务 DLL（系统输入法形态）
├── tests/               # 纯逻辑单元测试
└── docs/                # PRD / DESIGN / ARCHITECTURE / 图示
```

---

## 设计文档

- `docs/PRD.md` —— 产品需求（目标用户、用户故事、需求池）。
- `docs/DESIGN.md` —— 原始设计方案（模块、数据结构、算法）。
- `docs/ARCHITECTURE.md` —— 架构说明。

---

## 已知限制 / 诚实说明

- **仅 Windows 10/11 x64**。
- EXE 形态依赖 `SendInput` + 剪贴板发字，能覆盖绝大多数 DirectInput 游戏窗口；个别游戏若屏蔽钩子/模拟输入，可能需要走剪贴板后备方案。
- **TSF 注册已修复**：补上了 `Category` 命名值与 `Associations` 关联键（此前注册成功却不出现在列表的根因）。请重建 `hanpinyin_tsf.dll` 并重新注册验证；若仍不出现，多半是系统/权限问题，可反馈。
- 模糊音默认开启（12 组：zh/z、ch/c、sh/s、ang/an、eng/en、ing/in、iang/ian、uang/uan、n/l、r/l、f/h、k/g），可在配置面板关闭或增删。
- `rime/`、`sogou/` 两个目录是"借其它输入法引擎分发韩文候选"的实验性渠道，**未纳入本仓库**（保持主仓库聚焦于 TSF/EXE 本体与词库）；如需要可单独纳入。

---

## License

待定（请仓库所有者补充）。
