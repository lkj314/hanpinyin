// 单元测试：拼音前缀树（构建与前缀/多音节查询）
#include "../src/core/trie.h"
#include <iostream>
#include <vector>
#include <cassert>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::cout << "[FAIL] " << (msg) << "  (" << __FILE__ << ":" << __LINE__ << ")\n"; ++g_fail; } \
    else { std::cout << "[PASS] " << (msg) << "\n"; } \
} while (0)

int main() {
    hanpinyin::Trie trie;

    hanpinyin::Candidate c1;
    c1.korean = L"완"; c1.source_pinyin = "wan"; c1.freq = 10;
    c1.source = hanpinyin::Source::kMain; c1.matchMode = hanpinyin::MatchMode::kFull;
    hanpinyin::Candidate c2;
    c2.korean = L"완료"; c2.source_pinyin = "wan le"; c2.freq = 12;
    c2.source = hanpinyin::Source::kMain; c2.matchMode = hanpinyin::MatchMode::kFull;

    trie.insert({"wan"}, c1);
    trie.insert({"wan", "le"}, c2);

    // 1. 单音节前缀匹配
    std::vector<hanpinyin::Candidate> out;
    int n = trie.collect(0, {"wan", "le", "yi"}, out);
    CHECK(n == 2, "从位置0收集到 2 条（wan + wan le）");
    CHECK(out.size() == 2, "out 含 2 条候选");

    // 2. 多音节前缀匹配（wan le 在位置0）
    std::vector<hanpinyin::Candidate> out2;
    trie.collect(0, {"wan", "le"}, out2);
    CHECK(out2.size() == 2, "wan le 命中两条前缀");

    // 3. 从位置1开始（le 单音节，无独立词条）
    std::vector<hanpinyin::Candidate> out3;
    trie.collect(1, {"wan", "le"}, out3);
    CHECK(out3.empty(), "位置1无词典词");

    // 4. collectWithLen 携带音节长度
    std::vector<std::pair<int, hanpinyin::Candidate>> wl;
    trie.collectWithLen(0, {"wan", "le"}, wl);
    CHECK(wl.size() == 2, "collectWithLen 返回 2 条");
    bool hasLen2 = false;
    for (const auto& p : wl) if (p.first == 2) hasLen2 = true;
    CHECK(hasLen2, "存在长度为 2 的词（wan le）");

    if (g_fail == 0) { std::cout << "\nALL PASSED (trie)\n"; return 0; }
    std::cout << "\n" << g_fail << " FAILED (trie)\n";
    return 1;
}
