# HanPinyin 交接文档（HANDOVER）

> **写给下一位接手本项目的人（人或 AI）。**
> 本项目历史上踩过一连串代价很高的坑，每个坑都真实发生过、真实浪费过时间。
> **动手之前，请先通读本文档。** 文中每条"铁律"背后都对应一次真实事故（见第 6 节复盘）。

---

## 0. 30 秒版（TL;DR）

- **这是什么**：HanPinyin 是**基于 Rime/小狼毫（Weasel）魔改的韩文拼音输入法**——用户敲中文拼音，候选栏出韩文词（**2026-09-26 起纯韩文输出**：中文候选已移除，另有速记码两三键出常用词、长句自学习）。**不是**自研输入法框架。
- **真实在跑的代码**：只有 `rime/` 目录（一个 Rime 方案 + 一份生成词典）。`src/` 目录（自研 C++ TSF/EXE）是**已判死刑的死路**，只作历史存档，**绝不投入**。
- **三条铁律**：
  1. **禁止用任何命令行自动化（.py / .bat / PowerShell）改动输入法或触发部署**——输入法是 Windows 系统级软件，脚本既改不动它、也无法判断它是否更新成功；
  2. **改机制之前先查 Rime/Weasel 官方源码与文档，不要猜**；
  3. **更新是否成功 = 用户手动打字测试验证**，不是脚本返回值、不是进程状态。
- **部署三步（全程手动、GUI）**：复制 2 个 YAML → `%APPDATA%\Rime\` → 右键小狼毫托盘图标 →「重新部署」→ 打字验证。

---

## 1. 项目本质：输入法到底是什么

### 1.1 一句话定位

套壳**小狼毫（Weasel，Windows 版 Rime）**：我们只提供"方案配置 + 词典数据"，输入法的引擎、候选窗、系统注册、进程管理**全部由小狼毫负责**。我们不写、不编译、不注册任何输入法本体程序。

### 1.2 数据流全景（谁生成什么、谁读什么）

```
数据源                          开发期构建                    用户侧（运行时）
──────────────                 ──────────────────────        ─────────────────────────────
data/main_dict.json   ─┐
data/phrases.json     ─┼─►  rime/build_dict.py  ──►  rime/hanpinyin.dict.yaml ─┐
rime/extra_phrases.txt ┘   （生成模糊音/简拼码）                                │ 复制
                                                                            ▼
rime/sino_mix.schema.yaml（方案配置）──────────────────────────►  %APPDATA%\Rime\
                                                                            │ 「重新部署」
                                                                            ▼（小狼毫自己编译）
                                                              %APPDATA%\Rime\build\*.bin
                                                                            │
                                                                            ▼
                                                          小狼毫运行时（WeaselServer）只读 .bin
```

**关键认知**：
- 运行时**只读 `build\*.bin`**（编译后的二进制），**不读 YAML**。所以"把 YAML 拷过去"≠"生效"，必须经过**重新部署**这个编译动作。
- **2026-09-26 起专攻韩文**：`script_translator@cn`（中文）已从方案中移除——实测中文候选占候选窗 75%，全是谐音噪音，且用户中文有搜狗负责。现在**唯一 translator 是 table_translator（hanpinyin 词典）**，输出纯韩文 + 游戏英文黑话。

### 1.3 translator 形态（改 schema 必懂）

| translator | 词典 | 码型 | 说明 |
|---|---|---|---|
| 默认 `translator`（table_translator） | `hanpinyin.dict.yaml` | **连写码**（`nihao`，无 `'` 分隔） | 韩文词 + 英文黑话直打（键=英文本身）。**必须挂在默认 translator 上**，见 6.2 事故 |

**v2.4 关键开关（2026-09-26，均有 schema 断言守护）**：
- `enable_completion: false`——补全会把无关长码拉进候选窗（打 `hao` 冒出 짜증나），且补全只 -1 质量几乎无效（quality = exp(权重) ± 小量）。
- `enable_encoder: true` + `encode_commit_history: true`——**长句自学习**：连续上屏内容自动编码成用户词组（候选带 ☯），之后敲开头字母整句调出。查证 librime `table_translator.cc`，无需 `enable_sentence`。
- 权重体系：**速记码 110 > 基石词 100 > 主词条 10~14 > 变体码 5~9**；质量 = exp(权重)，`initial_quality` 影响可忽略（旧注释"抬高起始质量让韩文排前"是错误的，已删）。
- 速记码：`data/shortcodes.tsv`（87 条，码 = 中文意思拼音首字母，权重 110 保证该码首位）；`verify_regression.py` 有首位断言。

---

## 2. 铁律（违反 = 事故重演）

### ⛔ 铁律 1：禁止命令行自动化改动输入法

**绝不用 `.py` / `.bat` / PowerShell 脚本去"部署""更新""重启""验证"输入法。**

- 输入法是 Windows **系统层**软件，外部脚本既无法可靠地改变它，也**无法判断它是否真的更新成功**（进程活着 ≠ 新版生效）。
- 最终用户**不使用终端**。任何"请运行 xxx.py / xxx.bat"的交付方式都等于没有交付——点了没反应，用户只会更愤怒。
- 允许存在的脚本只有**开发期构建工具**（`build_dict.py` 生成词典、`verify_regression.py`/`validate_rime.py` 校验），它们不触碰输入法系统状态。
- 历史违规产物 `一键更新.bat` 与 `rime/deploy.py` **已于 2026-09-06 从仓库删除**（见第 8 节事故复盘），不要试图恢复或重写类似脚本。

### ⛔ 铁律 2：先研究官方机制，再动手

对 Rime/Weasel 任何行为拿不准时，权威来源只有一个：**开源仓库**（https://github.com/rime/weasel 及 rime 系列），直接读源码（如 `WeaselDeployer.cpp`）和官方文档。**禁止靠猜机制 + 写脚本试错**。本项目"重启服务 ≠ 重新部署"这个代价最高的误判，就是猜出来的（见 6.2）。

### ⛔ 铁律 3：验证更新 = 手动打字测试

判断新版是否生效，唯一可靠方法是**用户亲手打字看候选词**（测试码见 5.3）。脚本返回值、进程是否存在、文件时间戳，都不能证明输入法已切换到新版。

### ⛔ 铁律 4：不碰 `src/`

`src/`（自研 C++ TSF DLL / EXE 路线）**从未上线、已被判死刑**。历史教训：曾有一次优化工作整批打在 `src/` 上，全部作废回滚（`git revert 77878f9` → `6f39749`）。若有人提议"重启自研路线"，先把第 6.4 节死因看完再讨论。

---

## 3. 日常维护手册（GUI 优先，用户手动操作）

### 3.1 改词库（加词/改词/删词）

1. 编辑数据源（按需组合）：
   - `data/main_dict.json` —— 主词库（拼音**空格分隔音节**，如 `"duo shao qian"`）
   - `data/phrases.json` —— 整句短语库
   - `rime/extra_phrases.txt` —— 韩文基石高频词（权重 100；**只允许韩文与游戏缩写**）
   - `data/shortcodes.tsv` —— 速记码表（码 = 中文意思拼音首字母，权重 110 保证首位）
   - `data/meanings.tsv` —— 释义表（《HanPinyin 词典》用，加词需补一行）
2. 运行开发期构建工具（这是纯数据生成，不属于"自动化改输入法"）：
   ```
   cd rime && python build_dict.py
   ```
   产出 `hanpinyin.dict.yaml`（当前约 10246 条，含模糊音/简拼/速记码；纯中文与日文候选会被自动过滤）。
3. 必跑校验：`python verify_regression.py`（数据质量 + 构建覆盖 + 回归点 + **速记码首位** + **schema 形态**，必须 PASS）；
   必跑 `python validate_rime.py`（方案结构校验，需 PyYAML）；再跑 `python ../docs/gen_dictionary.py`（词典再版，EXIT=0）。
4. 调权重/改配置后建议跑一遍面板体检对比：`python _bench_panel.py --fresh`。

### 3.2 部署到本机（用户手动，双击/GUI，无命令行）

1. 把这两个文件复制到 `C:\Users\<用户名>\AppData\Roaming\Rime\`（即 `%APPDATA%\Rime\`），**覆盖同名旧文件**：
   - `rime/sino_mix.schema.yaml`
   - `rime/hanpinyin.dict.yaml`
2. 右键任务栏小狼毫托盘图标 → **「重新部署」**（等几秒，图标闪一下即完成）。
   - 等价官方命令：`WeaselDeployer.exe /deploy`（小狼毫自带程序，位于 `WeaselServer.exe` 同目录，如 `C:\Program Files\Rime\weasel-<版本号>\`）。这是**小狼毫自己的功能**，不是我们写的脚本。
3. 若改过全局配置类的东西，重启一次输入法进程也可以，但**重启绝不等于重新部署**（见 6.2）。

### 3.3 如何判断"更新成功了"（打字测试）

在词典里有一批**只有新版才有的专属码**，旧版绝对打不出来：

| 敲的字母 | 应出现的候选 | 验证的能力 |
|---|---|---|
| `lihao` | 안녕하세요 | 模糊音 n↔l（`nihao` 的模糊变体码） |
| `dbq` | **죄송합니다（首位）** | "对不起"速记码（2026-09-26 起首位=全词，ㅈㅅ 退居第二） |
| `pw` | **랭크** | **速记码**（排位，两键） |
| `ht` / `zb` | **한타 / 템** | **速记码**（团战/装备） |
| `bk` | **멘탈 터졌어** | **速记码**（崩溃，情绪词） |
| `gank` | gank（英文直打） | 英文黑话键=英文本身 |
| — | **候选窗无任何中文** | 2026-09-26 起专攻韩文的标志 |

**判定**：`pw` 能出 랭크、`ht` 能出 한타、候选窗干净无中文 ⇒ v2.4 已生效。
（2026-09-26 已用隔离环境 `_bench_panel.py --fresh` 实测：中文候选 0、日文 0、韩文全部首位。）

---

## 4. 关键技术细节（为什么这样设计）

### 4.1 模糊音/简拼为什么"生成进词典"，而不是写在 speller.algebra 里

- Rime 的 `speller.algebra` 模糊规则若用 `^` 锚定，**只作用于整串输入的开头**；而 hanpinyin 词典的码是**连写**（`nihao`/`buhaoyisi`），多音节词后面的音节根本吃不到模糊规则。
- 所以韩文侧的模糊音与简拼/混拼**在 `build_dict.py` 里按音节拆分后逐音节生成变体码，直接写进词典**，完全不依赖 algebra：
  - 声母对：`zh↔z, ch↔c, sh↔s, n↔l, r↔l, f↔h`（`_INITIAL_PAIRS`，build_dict.py:128）
  - 韵母对：`ang↔an, eng↔en, ing↔in, iang↔ian, uang↔uan, ong↔on`（`_FINAL_PAIRS`，build_dict.py:131）
  - `expand()`（build_dict.py：`expand` 函数）：每条词输出 规范码（原权重）+ 模糊码（权重-1）+ 简拼码（权重-5）；速记码在 `main()` 里以权重 110 落库。
- 全局 `speller.algebra`（韩文侧）保留模糊 derive 规则作为运行期兜底；**中文侧已随 `script_translator@cn` 移除**（2026-09-26），luna_pinyin 相关配置不再存在。

### 4.2 schema 排坑备忘（改 sino_mix.schema.yaml 前必读）

- **小狼毫"部署"只编译【默认 translator 的 dictionary】**。韩文词典必须挂在默认 translator（`translator.dictionary: hanpinyin`，schema 第 80 行）。若挂在命名 translator（如 `table_translator@han`）上，部署阶段**根本不会编译它**，运行时报 `Error loading table for dictionary 'hanpinyin'`，永远打不出韩文（schema 头部第 8-11 行注释记录了这次排坑）。
- **Rime 的简拼算子名是 `abbrev/.../`，不是 `abbreviate:`**（已对照 luna_pinyin 官方 pinyin.yaml 确认）；模糊规则必须写在 `abbrev` 之前。
- `.custom.yaml` 是官方补丁覆盖机制（`patch:`），如需不改本体文件可走这条路。

### 4.3 文件清单与职责

| 路径 | 状态 | 职责 |
|---|---|---|
| `rime/sino_mix.schema.yaml` | ✅ 在用 | 方案配置 v2.4：**纯韩文**（无中文 translator）、无补全、开长句自学习、模糊音规则 |
| `rime/hanpinyin.dict.yaml` | ✅ 在用 | 生成的韩文词典（约 10246 码），**构建产物但已入库** |
| `rime/build_dict.py` | ✅ 开发工具 | 数据源 → 词典生成（模糊/简拼展开 + 速记码落库 + 非韩文过滤） |
| `rime/verify_regression.py` | ✅ 开发工具 | 数据质量 + 构建覆盖 + 回归点 + **速记码首位** + **schema 形态**校验（改词库后必跑） |
| `rime/validate_rime.py` | ✅ 开发工具 | 方案/词典结构校验（需 PyYAML） |
| `rime/_bench_panel.py` | ✅ 体检工具 | 候选面板量化（韩文/中文/日文占比、韩文位次），调权/改配置前后各跑一遍 |
| `rime/_diag_select.py` | ✅ 诊断工具 | 端到端"能否选中上屏"（隔离环境模拟按键→选词→读上屏，绝不碰用户输入法数据） |
| `data/main_dict.json`、`data/phrases.json`、`rime/extra_phrases.txt` | ✅ 数据源 | 词库源头（改这里，不要直接改 dict.yaml；extra 只允许韩文与游戏缩写） |
| `data/shortcodes.tsv` | ✅ 数据源 | 速记码表（87 条，码=中文拼音首字母，权重 110 保证首位） |
| `data/meanings.tsv`、`data/dict_meta.tsv` | ✅ 词典数据源 | 释义 / 分类·语体·备注（《HanPinyin 词典》用） |
| `docs/gen_dictionary.py`、`docs/HanPinyin词典.html` | ✅ 词典 | 生成器（缺释义会报错退出）+ 新人词典（附录四=速记码表） |
| `src/`、`CMakeLists.txt`、`tests/` | 🗄️ 历史存档 | 自研 TSF/EXE 死路，仅供考古（见 6.4） |
| ~~`一键更新.bat`、`rime/deploy.py`、`rime/sino_mix.dict.yaml`、`installer/`、`sogou/`~~ | 🗑️ 已删除（2026-09-06） | 违规自动化产物与实验残留，勿恢复 |
| `rime/_installers/`（12MB weasel.exe）、`build/`、`data/user_dict.json` | 🚫 gitignored | 本地文件，不入库 |

---

## 5. 故障排查 FAQ（症状 → 根因 → 处理）

| 症状 | 根因 | 处理 |
|---|---|---|
| 打字完全不出韩文 | ① YAML 没复制到 `%APPDATA%\Rime`；② 复制了但**没有「重新部署」** | 重新走 3.2 三步；部署后用 3.3 打字测试验证 |
| 候选窗报 `Error loading table for dictionary 'hanpinyin'` | 词典没挂在**默认 translator** 上，部署阶段不编译它 | 检查 schema 第 80 行 `translator.dictionary: hanpinyin`（见 4.2） |
| 只有打全拼才出词，简拼/模糊不出 | 词典里没生成对应码（多半是直接手改了 dict.yaml 或没重跑 build_dict.py） | 改数据源 → 重跑 `build_dict.py` → 重新部署（见 3.1） |
| 「重新部署」了但候选还是旧的 | 复制的文件不对/没覆盖；或只重启了进程没部署 | 核对 `%APPDATA%\Rime` 下两个 YAML 的时间戳与内容；牢记重启 ≠ 部署（6.2） |
| 韩文候选排序怪/某词排不到首位 | 权重体系：速记码 110 > 基石词 100 > 主词条 10~14 > 变体码 -1/-5（质量=exp(权重)，放大明显） | 调数据源权重或 `data/shortcodes.tsv` 后重建；用 `_bench_panel.py --fresh` 实测对比 |
| 改了 `data/*.json` 但没任何变化 | 数据源拼音格式不符（主库要**空格分隔音节**、全小写） | 对照 `data/schema.md`；`extra_phrases.txt` 才是连写格式 |

> 通用原则：**先跑 `python validate_rime.py`，再做打字测试**。两步都过还有问题，才考虑动 schema。

---

## 6. 历史事故复盘（铁律的由来）

> 每一条铁律都不是拍脑袋，是真实事故换来的。接手者请把这段当"事故案例库"。

### 6.1 事故一：优化打错靶——在死路上投入了一整轮工作

- **经过**：用户反馈"模糊音基本等于零，必须打全拼"。接手者未先确认"项目到底是什么"，直接在 `src/`（自研 TSF core）上实施了完整的 Tier 0+1+2 优化（TSF 注册修复、模糊音集扩充、混合简拼），提交 `77878f9`。
- **真相**：用户实际在用的是 `rime/`（套壳小狼毫）方案，`src/` 从未上线。整轮工作全部作废，`git revert` 回滚（`6f39749`）。
- **教训**：**动手前先回答"用户跑的到底是哪个东西"**。看运行时证据（用户输入法列表里装的是什么、%APPDATA%\Rime 里有什么），而不是看仓库里哪个目录代码多。
- **对应铁律**：4。

### 6.2 事故二：重启服务 ≠ 重新部署（代价最高的机制误判）

- **经过**：更新词典后"没生效"，接手者猜"重启输入法进程就会重新加载"，据此做了 `一键更新.bat` v1：复制文件 + `taskkill WeaselServer` + 重启。用户实测"还是不行"。
- **真相**：查 Weasel 官方源码（`WeaselDeployer.cpp`）才发现：重新部署 = `WeaselDeployer.exe /deploy` → `configurator.UpdateWorkspace()`，它把 `%APPDATA%\Rime` 下的 YAML **重编译成 `build\*.bin`**；运行时只读 `.bin`。重启进程只做快速一致性检查，**不重编译**。类比 fcitx5-rime：Deploy = `start_maintenance(fullcheck=true)` 全量重编译，普通重启只是 quick check。
- **教训**：**机制问题查源码，一行源码胜过十次试错**。
- **对应铁律**：2。

### 6.3 事故三：命令行自动化三连败

- **经过**：为"让用户双击就能更新"，连续产出三版 `.bat`（`4dad85f` → `ab28064` 修括号 bug → `4a6020d` 改用官方 /deploy）。用户最终定论：**输入法这种系统级东西，绝不可能通过任何 py 命令与 win 批处理命令来做任何改动**，明确下令禁止一切命令行自动化方案。
- **过程中的具体坑**（即便要走这条路也躲不开）：
  - `%ProgramFiles(x86)%` 中的右括号会提前闭合 `for %%d in (...)` 的 IN 子句 → 批处理语法错误（v1 翻车点）；
  - 沙箱安全策略禁止 Bash→PowerShell、PowerShell→cmd.exe 互相调用 → **.bat 在开发环境里根本无法实测**，只能静态审查；
  - 脚本"跑完了"≠"输入法更新了"，无法闭环验证。
- **教训**：**给非技术用户的交付物必须是双击/GUI 可用的完整品**；对系统级软件，自动化脚本既不可靠也不可验证。
- **后续（2026-09-06）**：三版 `.bat` 与 `deploy.py` 已全部从仓库删除，此路线正式关闭。
- **对应铁律**：1、3。

### 6.4 存档：`src/` 自研路线的死因（防止有人想复活它）

自研 TSF DLL（`hanpinyin_tsf.dll`）当时卡死在**系统注册**环节：

- `regsvr32` 显示"注册成功"，但 Windows 输入法列表（添加键盘）里**永远不出现** HanPinyin；
- 根因（PowerShell 查用户本机注册表确认）：`tsf_registry.cpp` 的注册表结构与搜狗/微软拼音不对齐——`Category\Category\{TFCAT}` 写成了**默认值**（必须写成 CLSID 命名值，TSF 管理器才会枚举到）；LanguageProfile 下**缺少必填值** `Description`/`IconFile`/`IconIndex`/`Enable`/`HiddenInSettingUI` 等；
- 加上开发环境**没有任何 C++ 编译器**（见第 7 节），修复无法编译验证；
- TSF COM 注册、系统级调试成本极高，而套壳小狼毫早已达成同样目标 ⇒ **死刑判决成立**。
- 若将来真要复活：先解决上述两个注册表缺陷 + 准备 MSVC 环境，并做好远高于预期的调试预算。默认答案应该是"不复活"。

### 6.5 事故五："模糊音等于零"的真因

- 用户体感"必须打全拼"，最初误判为"词库太小/引擎不行"。真因有两个，都查实了：
  1. algebra 的 `^` 锚定模糊规则对连写码只作用于第一个音节（见 4.1）；
  2. 完全没有简拼/混拼。
- 修复方式是**在词典生成阶段展开码**（build_dict.py），而非调 algebra 参数——这是本项目的正确姿势。
- 修复后用户实测通过（2026-09-02）。

---

## 7. 开发环境限制（本沙箱）

| 限制 | 影响 | 对策 |
|---|---|---|
| **无任何 C++ 编译器**（g++/clang++/cl 全无） | `src/` 无法编译验证 | 依赖用户本机 MSVC（又一重死路理由） |
| **HTTPS git 传输被屏蔽**（`github.com:443`） | 不能 HTTPS push | 走 SSH(22)，见第 8 节命令 |
| Bash→PowerShell、PowerShell→cmd.exe 调用被安全拦截 | `.bat` 无法实测 | 静态审查 + 用户实测（也是放弃 bat 路线的原因之一） |
| 沙箱无法访问用户 `%APPDATA%\Rime` 的运行时状态 | 无法判断部署是否生效 | 一律手动打字测试（铁律 3） |

---

## 8. Git / GitHub

- **仓库**：https://github.com/lkj314/hanpinyin （public，`git@github.com:lkj314/hanpinyin.git`）
- **推送（沙箱内唯一可行方式，SSH key）**：
  ```bash
  GIT_SSH_COMMAND="ssh -i ~/.ssh/hanpinyin_push -o StrictHostKeyChecking=no" git push origin main
  ```
- **提交纪律**：禁止 `git add -A` / `git add .`；用**显式路径** add。删除文件逐个 `rm` + `git add -u`，**严禁 `git rm -r <目录>`**（有过整目录误删事故，见用户全局规约）。

### 关键 commit 地标

| commit | 内容 |
|---|---|
| `bfa6e2d` | 初始提交（当时还以 src/ 为主角） |
| `77878f9` | ⚠️ 打错靶的 Tier0-2 优化（src/） |
| `6f39749` | revert 77878f9 |
| `8ef06de` | 解除 rime/ 忽略 + README 重写（明示 Rime 是真实方案） |
| `ae05f20` | **核心功能**：模糊音逐音节 + 简拼/混拼（搜狗式）+ 中文侧 abbrev |
| `4dad85f`/`ab28064`/`4a6020d` | ⛔ 一键更新.bat 三连（违规产物，已于 2026-09-06 删除） |
| `304e698` | chore: 删除违规自动化产物与实验残留 |
| `462fd90` | feat(data): 词库扩充（+41/+15）+ verify_regression.py 回归工具，词典 7088 码 |
| `959fe8d` | feat(rime): 开启自学习与前缀补全（schema v2.3）+ README 纯 GUI 化 |
| `782f779` | feat(dict): 词典体验升级（搜索=输入法同规则、分类/语体/聚类/速记工具链） |
| `9a0859c` | feat(data): 第七批扩词——自我情绪表达 75 条 +「情绪」分类 |
| `bf9a001` | **feat!: 专攻韩文**（schema v2.4）：移除中文 translator、关补全、开长句自学习；速记码体系 87 条；清理 extra 中日英对照 97 行；新增 _bench_panel.py 体检工具 |

---

## 9. 当前遗留事项（2026-09-26 更新）

**已完成（2026-09-25/26，流畅度优化一轮）**：

1. ✅ 候选窗噪音治理：实测中文候选占 75% → 移除 `script_translator@cn`，纯韩文输出；
2. ✅ 前缀补全关闭（补全噪声实测 + librime 源码证实 -1 质量无效）；
3. ✅ 长句自学习开启（`enable_encoder` + `encode_commit_history`，候选带 ☯）；
4. ✅ 速记码体系 87 条（码=中文拼音首字母，权重 110 保证首位；回归校验含首位断言）；
5. ✅ extra_phrases 清理：删除中文/日/英对照行 97 行，纯韩文基石词库；
6. ✅ 体检工具链：`_bench_panel.py`（面板占比）+ `_diag_select.py`（上屏诊断）+ schema 形态断言；
7. ✅ **P2 模糊音瘦身经测量后否决**：省约 25% 词典行数但零体感收益（补全已关），反而损失容错——保留现有规则。

**待办**：

1. ⏳ **用户操作**：schema v2.4 + 词典已复制到 `%APPDATA%\Rime\`（备份 `*.v1bak.20260926001450`），等用户**托盘「重新部署」**后按 3.3 打字测试（`pw`→랭크、候选窗无中文 = 生效）；
2. ⏳ 长句自学习是全新机制，用户先用几天，观察 ☯ 词组是否出现、误记忆是否需要清理（删 `hanpinyin.userdb\` 重部署即清零）；
3. 速记码首批准确 87 条，按用户实战反馈继续增补（改 `data/shortcodes.tsv` → build → verify）；
4. 远期可选：同一词典移植 Android Trime；plum（东风破）配方分发给其他玩家（须先调研官方机制，交付物不能是命令行脚本——铁律 1）。

---

## 10. 给接手者：协作方式须知（与这位用户合作）

- **先诊断，再行动**；先看代码/本机实况，再给方案。反感"不看本地代码就瞎猜"。
- **方案先审批，再执行**；大型改动逐步确认，最小化修改。数据库/系统级变更尤其如此。
- **交付完整可用之物**：EXE/安装包级别的完成品，拒绝半成品和"需要手动配置"的原型。
- 用户**不使用终端**：所有给用户的操作指引必须是双击/GUI 级别。
- 用户会给**Tier 级别**圈定范围（如"仅 Tier 0+1+2"），说"开始吧"即视为批准推荐方案。
- 遇到反复失败：**停下来，系统性重新审视**，而不是继续换姿势重试。
