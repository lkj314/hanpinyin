// 单元测试：候选管理器（词频排序 + 全拼优先 + 缩写合并 + 同韩文合并）
#include "../src/core/candidate_manager.h"
#include "../src/core/dictionary.h"
#include "../src/core/user_dict.h"
#include "../src/core/fuzzy_normalizer.h"
#include "../src/core/config_model.h"
#include "../src/core/pinyin_segmenter.h"
#include <iostream>
#include <vector>
#include <cassert>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::cout << "[FAIL] " << (msg) << "  (" << __FILE__ << ":" << __LINE__ << ")\n"; ++g_fail; } \
    else { std::cout << "[PASS] " << (msg) << "\n"; } \
} while (0)

int main() {
    using namespace hanpinyin;

    // 构造一个小词库：wan(完,5) / wan(万,3) / wan le(完了,20)
    Dictionary dict;
    dict.addMainEntry("wan", L"완", 5);
    dict.addMainEntry("wan", L"만", 3);
    dict.addMainEntry("wan le", L"완료", 20);

    UserDict ud;
    FuzzyNormalizer fn;
    ConfigModel cfg;
    fn.setConfig(cfg);

    CandidateManager mgr;
    mgr.setDictionary(&dict);
    mgr.setUserDict(&ud);
    mgr.setNormalizer(&fn);

    PinyinSegmenter seg;

    // 全拼输入 "wanle"
    std::vector<Segment> segs = seg.segment("wanle");
    CandidateList list = mgr.getCandidates(segs);

    CHECK(!list.items.empty(), "生成候选非空");

    // 1. kFull 全部排在 kAbbrev 之前（此处无缩写命中，皆为 kFull）
    bool fullFirst = true;
    bool seenAbbrev = false;
    for (const auto& c : list.items) {
        if (c.matchMode == MatchMode::kAbbrev) seenAbbrev = true;
        if (seenAbbrev && c.matchMode == MatchMode::kFull) fullFirst = false;
    }
    CHECK(fullFirst, "kFull 恒在 kAbbrev 之前");

    // 2. 同 matchMode 内按 freq 降序（완료20 > 완5 > 만3）
    CHECK(list.items[0].korean == L"완료", "最高频 완료 排首位");
    if (list.items.size() >= 3) {
        CHECK(list.items[1].freq >= list.items[2].freq, "freq 降序");
    }

    // 3. 同韩文合并（wan 同时被全拼与缩写命中时应合并一条）
    //    构造带缩写的场景：输入 "wl"（缩写）与 "wanle"
    Dictionary dict2;
    dict2.addMainEntry("wan le", L"완료", 20);  // 全拼
    dict2.addMainEntry("wan le", L"완료", 1);   // 同韩文（模拟缩写侧重复，freq 取大）
    CandidateManager mgr2;
    mgr2.setDictionary(&dict2);
    mgr2.setUserDict(&ud);
    mgr2.setNormalizer(&fn);
    std::vector<Segment> segs2 = seg.segment("wanle");
    CandidateList list2 = mgr2.getCandidates(segs2);
    int wanCount = 0;
    for (const auto& c : list2.items) if (c.korean == L"완료") ++wanCount;
    CHECK(wanCount == 1, "同韩文 완료 合并为一条");

    // 4. 用户词库叠加：记录 wan le -> 완료 后该候选 freq 增大
    ud.record({"wan", "le"}, L"완료");
    CandidateManager mgr3;
    mgr3.setDictionary(&dict);
    mgr3.setUserDict(&ud);
    mgr3.setNormalizer(&fn);
    CandidateList list3 = mgr3.getCandidates(seg.segment("wanle"));
    // 완료 基础 freq 20 + boost(1) + viterbi 加权，应 >= 20
    bool found = false;
    for (const auto& c : list3.items) {
        if (c.korean == L"완료") { found = true; CHECK(c.freq >= 20, "用户词库叠加后 freq 不减小"); }
    }
    CHECK(found, "找到 완료 候选");

    if (g_fail == 0) { std::cout << "\nALL PASSED (candidate_manager)\n"; return 0; }
    std::cout << "\n" << g_fail << " FAILED (candidate_manager)\n";
    return 1;
}
