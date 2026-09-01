// 单元测试：拼音分割器（全拼 + 缩写）
#include "../src/core/pinyin_segmenter.h"
#include <iostream>
#include <string>
#include <vector>
#include <cassert>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::cout << "[FAIL] " << (msg) << "  (" << __FILE__ << ":" << __LINE__ << ")\n"; ++g_fail; } \
    else { std::cout << "[PASS] " << (msg) << "\n"; } \
} while (0)

int main() {
    hanpinyin::PinyinSegmenter seg;

    // 1. 全拼切分
    auto segs = seg.segment("wanleyixiawu");
    CHECK(segs.size() == 2, "返回两个 Segment（全拼 + 缩写）");
    CHECK(!segs[0].isAbbrev, "第一个为全拼");
    CHECK(segs[0].syllables.size() == 5, "全拼切分为 5 个音节");
    CHECK((segs[0].syllables == std::vector<std::string>{"wan", "le", "yi", "xia", "wu"}),
          "全拼音节正确 [wan,le,yi,xia,wu]");

    // 2. 缩写切分（首字母）
    CHECK(segs[1].isAbbrev, "第二个为缩写");
    CHECK((segs[1].syllables == std::vector<std::string>{"w", "l", "y", "x", "w"}),
          "缩写音节正确 [w,l,y,x,w]");

    // 3. 大小写与非法字符清洗
    auto segs2 = seg.segment("Wan Le!");
    CHECK((segs2[0].syllables == std::vector<std::string>{"wan", "le"}),
          "大写转小写并丢弃非字母");

    // 4. 单音节
    auto segs3 = seg.segment("ni");
    CHECK((segs3[0].syllables == std::vector<std::string>{"ni"}), "单音节 ni");
    CHECK((segs3[1].syllables == std::vector<std::string>{"n"}), "单音节缩写 n");

    // 5. 空串
    auto segs4 = seg.segment("");
    CHECK(segs4.empty(), "空串返回空");

    if (g_fail == 0) { std::cout << "\nALL PASSED (pinyin_segmenter)\n"; return 0; }
    std::cout << "\n" << g_fail << " FAILED (pinyin_segmenter)\n";
    return 1;
}
