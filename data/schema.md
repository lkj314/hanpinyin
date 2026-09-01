# HanPinyin 数据 JSON Schema 说明

> 本目录包含三套 JSON 数据文件，均由项目自研极简解析器 `src/core/json.hpp` 读取/写入。
> 所有文件统一 **UTF-8 无 BOM** 编码；韩文以原始 UTF-8 字节存储。
> 词库规模当前为种子版本（主词库约 120 条、短语库约 40 条），可后续增至 3000+，玩家也可自行增补。

---

## 1. 主词库 `main_dict.json`

数组，每个元素是一条「拼音 → 多个韩文候选」的映射。

```json
[
  {
    "pinyin": "wan le",
    "candidates": [
      ["완료", 12],
      ["완료됐어", 5]
    ]
  }
]
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `pinyin` | string | 拼音键，**空格分隔的音节、全小写**（如 `"wan le"`）。装载时自动拆分为音节序列 `[wan, le]`，并同时生成首字母缩写键 `[w, l]` 注入 Trie。 |
| `candidates` | array | 候选数组，每个元素为 `[韩文字符串, 词频整数]`。词频越大，候选排序越靠前。 |

> 同一韩文若同时被「全拼」与「缩写」命中，`CandidateManager` 会合并为一条（取较大词频，source 取较高优先级）。

---

## 2. 短语库 `phrases.json`

数组，每个元素是一句整句拼音 → 整句韩文。

```json
[
  {
    "pinyin": "wan le yi xia wu",
    "korean": "오후 내내 했어"
  }
]
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `pinyin` | string | 整句拼音（空格分隔音节、全小写）。 |
| `korean` | string | 对应的整句韩文。装载时赋予较高默认词频（100），确保整句候选优先展示。 |

---

## 3. 用户词库 `user_dict.json`

对象（map）。**键为音节序列以空格 join 的字符串**，值为该键下的候选数组。

```json
{
  "wan le": [["완료", 8]],
  "ni hao": [["안녕하세요", 15]]
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| 键（key） | string | `join(syllables, ' ')`，例如 `"wan le"`。 |
| 值（value） | array | `[[韩文字符串, 选择次数], ...]`。玩家每次选定某候选，对应计数 +1；计数作为词频叠加量，使常用词更靠前。 |

> 写入时机（Q5）：退出时全量写 + 累计选择满 50 次增量写，防止崩溃丢失。

---

## 4. 配置 `config.json`（由 `ConfigModel` 读写，位于程序运行目录或指定路径）

```json
{
  "fuzzyOn": true,
  "fuzzyPairs": ["zh=z", "ch=c", "sh=s", "ang=an", "eng=en", "ing=in", "n=l", "r=l"],
  "hotkey": { "modifiers": 1, "vk": 32 },
  "mainDictPath": "data/main_dict.json",
  "phraseDictPath": "data/phrases.json",
  "userDictPath": "data/user_dict.json"
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `fuzzyOn` | bool | 模糊音总开关，默认 `true`。 |
| `fuzzyPairs` | array<string> | 模糊映射对，格式 `"源=目标"`（如 `"zh=z"`）。为空时使用内置默认集。 |
| `hotkey.modifiers` | int | 修饰键位掩码：`0=无, 1=Ctrl, 2=Alt, 4=Shift`（可叠加）。 |
| `hotkey.vk` | int | 主键的虚拟键码（如 `32` = Space，`0x20`）。 |
| `*DictPath` | string | 三套词库文件路径。 |
