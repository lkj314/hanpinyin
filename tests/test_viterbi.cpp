// 单元测试：整句 Viterbi 最优词序列
#include "../src/core/candidate_manager.h"
#include "../src/core/dictionary.h"
#include "../src/core/user_dict.h"
#include "../src/core/fuzzy_normalizer.h"
#include "../src/core/config_model.h"
#include <iostream>
#include <vector>
#include <cassert>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::cout << "[FAIL] " << (msg) << "  (" << __FILE__ << ":" << __LINE__ << ")\n"; ++g_fail; } \
    else { std::cout << "[PASS] " << (msg) << "\n"; } \
} while (0)

// 统计 source_pinyin 的音节数（以空格分隔）
static int sylCount(const std::string& pinyin) {
    int c = 0;
    bool inWord = false;
    for (char ch : pinyin) {
        if (ch == ' ') inWord = false;
        else if (!inWord) { ++c; inWord = true; }
    }
    return c;
}

int main() {
    using namespace hanpinyin;

    Dictionary dict;
    // 整句短语（高词频）
    dict.addMainEntry("ni hao", L"안녕하세요", 50);
    // 单音节词
    dict.addMainEntry("ni", L"너", 5);
    dict.addMainEntry("hao", L"호", 4);

    UserDict ud;
    FuzzyNormalizer fn;
    ConfigModel cfg;
    fn.setConfig(cfg);

    CandidateManager mgr;
    mgr.setDictionary(&dict);
    mgr.setUserDict(&ud);
    mgr.setNormalizer(&fn);

    // 1. 整句应优选单条高词频短语
    auto path1 = mgr.getBestSequence({"ni", "hao"});
    CHECK(path1.size() == 1, "整句 ni hao 优选单条路径");
    CHECK(!path1.empty() && path1[0].korean == L"안녕하세요", "最优词为 안녕하세요");

    // 2. 多音节分解：wan le yi
    Dictionary dict2;
    dict2.addMainEntry("wan", L"완", 5);
    dict2.addMainEntry("wan le", L"완료", 20);
    dict2.addMainEntry("le", L"르", 3);
    dict2.addMainEntry("yi", L"이", 8);
    CandidateManager mgr2;
    mgr2.setDictionary(&dict2);
    mgr2.setUserDict(&ud);
    mgr2.setNormalizer(&fn);

    auto path2 = mgr2.getBestSequence({"wan", "le", "yi"});
    int total = 0;
    for (const auto& c : path2) total += sylCount(c.source_pinyin);
    CHECK(!path2.empty(), "多音节路径非空");
    CHECK(total == 3, "路径覆盖全部 3 个音节");
    CHECK(path2[0].korean == L"완", "首词为 wan->완");

    if (g_fail == 0) { std::cout << "\nALL PASSED (viterbi)\n"; return 0; }
    std::cout << "\n" << g_fail << " FAILED (viterbi)\n";
    return 1;
}
