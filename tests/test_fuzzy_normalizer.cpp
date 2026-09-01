// 单元测试：模糊音归一化（完整模糊集，含开关与双向）
#include "../src/core/fuzzy_normalizer.h"
#include "../src/core/config_model.h"
#include <iostream>
#include <cassert>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::cout << "[FAIL] " << (msg) << "  (" << __FILE__ << ":" << __LINE__ << ")\n"; ++g_fail; } \
    else { std::cout << "[PASS] " << (msg) << "\n"; } \
} while (0)

int main() {
    hanpinyin::ConfigModel cfg;  // 默认 fuzzyOn=true，含默认模糊集
    hanpinyin::FuzzyNormalizer fn;
    fn.setConfig(cfg);

    // 声母
    CHECK(fn.normalize("zhi") == "zi", "zh->z: zhi->zi");
    CHECK(fn.normalize("chi") == "ci", "ch->c: chi->ci");
    CHECK(fn.normalize("shi") == "si", "sh->s: shi->si");
    CHECK(fn.normalize("zhang") == "zan", "zhang->zan (声母+长→短)");

    // 后鼻音 长→短
    CHECK(fn.normalize("wang") == "wan", "ang->an: wang->wan");
    CHECK(fn.normalize("feng") == "fen", "eng->en: feng->fen");
    CHECK(fn.normalize("bing") == "bin", "ing->in: bing->bin");

    // 单字母
    CHECK(fn.normalize("nan") == "lan", "n->l: nan->lan");
    CHECK(fn.normalize("ren") == "len", "r->l: ren->len");

    // 短形式原样（无长→短反向）
    CHECK(fn.normalize("an") == "an", "an 原样");
    CHECK(fn.normalize("wan") == "wan", "wan 原样");

    // 反方向（双向）
    CHECK(fn.reverse("wan") == "wang", "reverse: wan->wang (双向 an↔ang)");
    CHECK(fn.reverse("zong") == "zhong", "reverse: zong->zhong (双向 z↔zh)");

    // 扩展模糊集（iang/ian, uang/uan, f/h, k/g）
    CHECK(fn.normalize("xiang") == "xian", "iang->ian: xiang->xian");
    CHECK(fn.reverse("xian") == "xiang", "reverse: xian->xiang");
    CHECK(fn.normalize("huang") == "huan", "uang->uan: huang->huan");
    CHECK(fn.reverse("huan") == "huang", "reverse: huan->huang");
    CHECK(fn.normalize("fa") == "ha", "f->h: fa->ha");
    CHECK(fn.reverse("ha") == "fa", "reverse: ha->fa");
    CHECK(fn.normalize("ka") == "ga", "k->g: ka->ga");
    CHECK(fn.reverse("ga") == "ka", "reverse: ga->ka");

    // 关闭模糊音
    cfg.setFuzzyOn(false);
    fn.setConfig(cfg);
    CHECK(fn.normalize("zhi") == "zhi", "关闭后 zhi 原样");
    CHECK(fn.normalize("wang") == "wang", "关闭后 wang 原样");
    CHECK(fn.isFuzzyOn() == false, "fuzzyOn=false");

    if (g_fail == 0) { std::cout << "\nALL PASSED (fuzzy_normalizer)\n"; return 0; }
    std::cout << "\n" << g_fail << " FAILED (fuzzy_normalizer)\n";
    return 1;
}
