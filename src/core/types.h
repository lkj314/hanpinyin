// HanPinyin · 核心公共类型定义
// 本文件属于纯逻辑核心层（core），严禁 #include <windows.h>，
// 仅依赖 C++ 标准库，保证可在无窗口环境下独立编译与单元测试。
//
// 编码约定：
//   · 拼音使用 std::string（纯 ASCII、小写、无空格，分割后按音节存于 vector<string>）
//   · 韩文使用 std::wstring（UTF-16LE），韩文常量以 L"..." 字面量表示
//   · 所有跨模块字符串转换统一走本文件的 utf8_to_wstring / wstring_to_utf8

#pragma once

#include <string>
#include <vector>

namespace hanpinyin {

// 候选来源：主词库 / 短语库 / 用户词库
enum class Source {
    kMain,    // 主词库 main_dict.json
    kPhrase,  // 短语库 phrases.json
    kUser     // 用户词库 user_dict.json
};

// 匹配模式：全拼 / 缩写首字母
enum class MatchMode {
    kFull,    // 全拼匹配
    kAbbrev   // 缩写（首字母）匹配
};

// 一个拼音片段（由 PinyinSegmenter 产出，全拼与缩写各一条）
struct Segment {
    std::string raw;                     // 原始输入串中对应本片段的子串
    std::vector<std::string> syllables;  // 音节序列（全拼 or 缩写首字母序列）
    bool isAbbrev = false;               // 是否为缩写切分
    std::vector<std::string> normalized; // 归一化后的音节序列（初值等同 syllables，由 CandidateManager 用 FuzzyNormalizer 覆写）
};

// 一条候选（韩语 + 触发拼音 + 词频 + 来源 + 匹配模式）
struct Candidate {
    std::wstring korean = L"";   // 韩文（UTF-16）
    std::string source_pinyin;   // 触发该候选的原始拼音（用于悬浮窗展示 "(wan le)"）
    int freq = 0;                // 词频（含用户词库叠加后的有效频率）
    Source source = Source::kMain;
    MatchMode matchMode = MatchMode::kFull;

    // 便于单测比较的辅助
    bool operator==(const Candidate& o) const {
        return korean == o.korean && source_pinyin == o.source_pinyin &&
               freq == o.freq && source == o.source && matchMode == o.matchMode;
    }
};

// 候选列表（带分页信息）
struct CandidateList {
    std::vector<Candidate> items;  // 当前页候选（已排序）
    bool hasMore = false;          // 是否还有更多候选（超过单页上限）
    int page = 0;                  // 当前页码（从 0 开始）
};

// UTF-8 -> UTF-16 转换（标准库手动实现，不依赖 windows.h / <codecvt>）
std::wstring utf8_to_wstring(const std::string& utf8);

// UTF-16 -> UTF-8 转换
std::string wstring_to_utf8(const std::wstring& ws);

}  // namespace hanpinyin
