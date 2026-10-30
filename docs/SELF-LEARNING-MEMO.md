# HanPinyin 自学习机制备忘录（T1-1 归因结论）

> 日期：2026-10-31 ｜ 任务来源：《全面优化更新蓝图》T1-1
> 证据来源：librime 1.13.1 源码引证（grep.app 逐函数抓取）+ 5 组隔离环境实验 + 真实 userdb 逐条解析
> 工具：`rime/_dump_userdb.py`（userdb 只读解析）、`rime/_exp_learn.py`（学习重排实验）、`rime/_exp_learn4.py`（编码持久化判别实验）
> 全程未改动任何输入法文件；所有实验在临时目录进行。

---

## 0. 三行结论

1. **单词条学习（选词记忆 → 重排 → 衰减）：真实有效**，这就是"越用越懂你"的可用引擎。
2. **☯ 长句编码（encode_commit_history）：两层断裂，判定不可修**——第一层是词典缺 `encoder:` 段导致编码器自禁用；第二层是即使补上规则，连写韩文词典下编码产物也不落库（librime 编码器内部拆词环节，无编译器不可修）。
3. **文档里"长句自学习、敲开头字母整句调出"为不实描述**（09-26 只验证了 schema 开关存在，未验证链路产出），需按本备忘录修正。

## 1. 机制全景（实测状态）

| 机制 | schema 开关 | 实测状态 | 证据 |
|---|---|---|---|
| 单词条学习（userdb） | `enable_user_dict: true` | ✅ **有效**：每次选词 +1 commit；同码重输时学过的词跳到候选首位；频次×新近度计权、久不用衰减 | 实验 E1 + 真实 userdb 54 条 |
| ☯ 长句编码 | `enable_encoder + encode_commit_history` | ❌ **无效**：真实词典下零产出 | 实验 E0/E2/E4 + 源码 |
| 临时组句（MakeSentence） | `enable_sentence`（现为 false） | ◐ 机制存在但**不落库、会话即逝**，且与"连写码组句不可控"的既有判断一致 | 实验 E3 |

## 2. 单词条学习：完整行为规格（已实证）

- **落库格式**：键 = `码 \t 词`，值 = `c=提交次数 d=权重 dee t=时间戳 tick`。例：`zmb \t어떡해요 => c=2 d=1.99005 t=3`。
- **重排**（实验 E1）：故意选第二候选 어떡해요，重输 `zmb` 后它立即跳到首位——学过的词在同码竞争中有优先权。
- **学习按 (码, 词) 对生效**：在 `zmb` 下学的偏好，敲 `zenmeban` 不继承。
- **模糊派生码会原样入库**：敲 `nihao` 可能被记成 `lihao \t안녕하세요`（speller algebra 的 derive 码参与匹配后按实际命中码落库）。真实 userdb 里 `lihao` c=5 即此现象。无害，但挖矿统计需按词聚合。
- **衰减**：`dee`（权重基）随提交增长（c=7 → d=5.7），时间推进时久不用的词会衰减（`user_dictionary.cc` 的 formula_d/formula_p 记忆动力学）。
- **真实数据**（`_dump_userdb.py` 解析，54 条）：Top——`hah`→ㅋㅋ (c=7)、`wanan`→잘자 (c=6)、`lihao`→안녕하세요 (c=5)、`wandangle`→망했다 (c=5)、`dabuguole`→못 이겨 (c=4)。
- **时间线**：userdb 全部写入发生在 09-06 ~ 09-26（v2.3 时代）；**09-26 05:11 部署后至今零提交**——学习引擎待命一个月，缺的是使用数据，不是机制。

## 3. ☯ 长句编码：根因链（两层断裂）

### 断裂① 词典缺 `encoder:` 段 → 编码器自禁用（生产环境现症）

- 源码：`unity_table_encoder.cc:39`——
  ```
  if (!settings.enable_encoding) {
    LOG(WARNING) << "unity table encoder is not enabled in dict settings.";
  ```
- `enable_encoding` 来自**词典元数据**（`table_db.cc`：`settings->enable_encoding = meta_data_->dict_format->options().enable_encoding`），不由 schema 决定。
- 实验 E1（真实 schema + 真实词典隔离编译）：警告出现，schema 里 `enable_encoder/encode_commit_history: true` 形同虚设。**当前 `build_dict.py` 生成的词典头没有 encoder 段，此即生产现症。**
- 实验 E2：给词典补 `encoder: rules:` 后警告消失（开关打开）；单加 `enable_encoding: true` 键无效（键名不存在，靠 rules 段推导）。

### 断裂② 补上规则后，编码产物仍不落库（librime 内部，判定不可修）

- 实验 E4（生产形态：rules + `enable_sentence: false`）：6 次连续单词提交，WAL 逐条检查**零条 `\x7fenc` 条目**；重开引擎触发压实后探针 `nade`/`dena` 全空。
- 对照构造：微型词典刻意包含「两词拼接恰为词条」（가+나다=가나다）也无济于事（实验 E2）。
- 源码旁证（`table_translator.cc Memorize`，grep.app 抓取）：历史遍历逻辑存在且类型白名单为 `table/user_table/sentence/uniquified`，遍历后逐级调用 `encoder_->EncodePhrase(...)`；EncodePhrase 内部需把拼接文本**拆回词典词条**才能推导码——拆词环节对"多音节连写韩文词"不工作（推测：拆词器面向单字/固定音节体系，如五笔单字层；韩文多音节词无拆分依据）。该环节源文件（`gear/encoder.cc`）无法经现有网络通道完整获取（raw.githubusercontent ECONNRESET、web_reader 截断），**列为残留未决**，但不影响结论：即使修通第一层，第二层仍卡死。
- **对照**：五笔等单字层词典的自造词在社区长期可用（librime#303 在讨论其删除），佐证断裂②是"词典形态依赖"而非普遍故障。

### 旁证：`enable_sentence` 的 ☯ 不是自学习

- 实验 E3：`enable_sentence: true` 时，会话**第一次提交之前** `nade` 就出现 `가나다[ ☯ ]` 候选——这是实时组句（MakeSentence），不落库、重开即逝。09-25 已因"组句不可控"关闭，维持关闭。

## 4. 实验记录一览

| # | 配置 | 操作 | 结果 | 结论 |
|---|---|---|---|---|
| E0 | 真实 userdb 解析 | `parse_ldb/WAL` | 59 键 = 5 元数据 + 54 学习条目；零 `\x7fenc` | 单词学习真实积累；☯ 从未产出 |
| E1 | 真实 schema+词典，隔离 | zmb 选第二候选→重输 | 어떡해요 跳首位 | 学习重排有效；出现 unity_table_encoder 警告 |
| E2 | 微型词典 ×2（`enable_encoding` 键 / `encoder.rules` 段） | 2 次提交 | A 仍警告；B 警告消失但零编码 | 开关=词典 `encoder:` 段；仅开开关不够 |
| E3 | rules + `enable_sentence: true` | 组句提交 + 探测 | ☯ 候选出现但不落库 | ☯=临时组句，非持久化 |
| E4 | rules + 生产形态，6 连击 + 重开 | WAL + 探针 | 零 `\x7fenc`；重开无 ☯ | 断裂②确认，持久化链路对连写韩文词典不工作 |

## 5. 对既有文档的修正清单（→ 蓝图 T1-4）

| 文件 | 位置 | 不实表述 | 修正为 |
|---|---|---|---|
| `README.md` | 输入体验节 | "长句自学习……常用长句打过一次，之后敲开头几个字母就能整句调出" | 删除该条；替换为"选词自学习：常选的词自动上浮"（实测有效） |
| `HANDOVER.md` | 1.3 / 9 节 | "长句自学习开启（enable_encoder…候选带 ☯）" | 标注"实测未生效（见 docs/SELF-LEARNING-MEMO.md），开关保留但无产出"或随 T1-2 移除 |
| `rime/README.md` | 同类描述 | 同上 | 同上 |

## 6. 路线结论（→ 蓝图 T1-2 建议）

- **路线 A（修通 ☯ 编码）：否决。** 断裂②在 librime C++ 编码器内部，本沙箱无编译器、无完整源码，即便补词典 rules 也不落库；收益（整句码召回）不确定而成本不可控。唯一例外：**将来若升级 librime，先用 `_exp_learn4.py` 复测**（5 分钟出结论）再重新评估。
- **路线 B（挖矿 → 固化）：推荐。** 单词条学习（§2）已真实有效，负责"越用越顺"的日常部分；长句/高频词的持久优势改由固化管线提供——定期 `_dump_userdb.py` 挖矿 → 高频词条经用户过目进 `data/`（main/extra/shortcodes）→ build → verify → 部署。固化产物可配短码、可进词典文档、不怕 userdb 清零/换机。
- schema 里 `enable_encoder` / `encode_commit_history` 两个开关随 T1-2 一并处置（移除并修正注释，或保留但注明无效——建议移除，消除误导与启动警告）。

## 7. 工具用法与复验方法

```bash
# 查看真实学习数据（只读，可随时跑）
python rime/_dump_userdb.py

# 指定目录（如隔离实验的临时库）
python rime/_dump_userdb.py --raw <userdb目录>   # --raw 逐文件输出（含历史版本）

# 学习重排实验（验证选词记忆行为）
python rime/_exp_learn.py

# ☯ 编码持久化判别（升级 librime 后复测；约 1 分钟）
python rime/_exp_learn4.py
# 判读：userdb 出现 [☯编码] 条目或会话2探针命中 ⇒ 链路可用，重新评估路线 A
```

> 实验脚本坑位备忘（已内置修复）：librime 新版 `RimeInitialize` 返回 void；所有带 session id 的 ctypes 函数必须显式 `c_uint64`（大 session id 否则 OverflowError）；Rime 日志走 stderr；临时目录必须带 `default.custom.yaml`（登记方案）与 `installation.yaml`，否则部署器不编译自定义方案。

## 8. 残留不确定项（诚实清单）

| # | 事项 | 影响 | 备注 |
|---|---|---|---|
| 1 | 断裂②的确切源码位置（`gear/encoder.cc` 拆词器行为） | 仅影响归因完整度，不影响路线结论 | 源文件获取通道受限；librime 升级时复测即可 |
| 2 | 模糊派生码落库的选择规律（敲 nihao 何时报 lihao） | 挖矿统计口径（按词聚合可规避） | derive 码与规范码的命中顺序待查，不紧急 |
| 3 | 真实生产日志中的警告确认 | 无（隔离实验已用真实词典+方案复现） | 当前 WeaselServer 日志为新进程，尚无 sino_mix 激活记录；下次打字后可在 `%TEMP%\rime.weasel\` 最新 INFO 日志中 grep "unity table encoder" 直接眼见 |
