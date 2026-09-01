// HanPinyin · 拼音音节分割器实现

#include "pinyin_segmenter.h"
#include <cctype>
#include <unordered_set>

namespace hanpinyin {

namespace {
// 合法拼音音节表（覆盖常用及游戏术语所需音节）。
// 最大长度 6（如 zhuang / chuang），FMM 由长到短尝试。
const char* kSyllableList[] = {
    "a", "ai", "an", "ang", "ao",
    "ba", "bai", "ban", "bang", "bao", "bei", "ben", "beng", "bi", "bian", "biao",
    "bie", "bin", "bing", "bo", "bu",
    "ca", "cai", "can", "cang", "cao", "ce", "cen", "ceng", "cha", "chai", "chan",
    "chang", "chao", "che", "chen", "cheng", "chi", "chong", "chou", "chu", "chua",
    "chuai", "chuan", "chuang", "chui", "chun", "chuo", "ci", "cong", "cou", "cu",
    "cuan", "cui", "cun", "cuo",
    "da", "dai", "dan", "dang", "dao", "de", "deng", "di", "dia", "dian", "diao",
    "die", "ding", "diu", "dong", "dou", "du", "duan", "dui", "dun", "duo",
    "e", "ei", "en", "eng", "er",
    "fa", "fan", "fang", "fei", "fen", "feng", "fo", "fou", "fu",
    "ga", "gai", "gan", "gang", "gao", "ge", "gei", "gen", "geng", "gong", "gou",
    "gu", "gua", "guai", "guan", "guang", "gui", "gun", "guo",
    "ha", "hai", "han", "hang", "hao", "he", "hei", "hen", "heng", "hong", "hou",
    "hu", "hua", "huai", "huan", "huang", "hui", "hun", "huo",
    "ji", "jia", "jian", "jiang", "jiao", "jie", "jin", "jing", "jiong", "jiu",
    "ju", "juan", "jue", "jun",
    "ka", "kai", "kan", "kang", "kao", "ke", "ken", "keng", "kong", "kou", "ku",
    "kua", "kuai", "kuan", "kuang", "kui", "kun", "kuo",
    "la", "lai", "lan", "lang", "lao", "le", "lei", "leng", "li", "lia", "lian",
    "liang", "liao", "lie", "lin", "ling", "liu", "lo", "long", "lou", "lu",
    "luan", "lun", "luo", "lv", "lve",
    "ma", "mai", "man", "mang", "mao", "me", "mei", "men", "meng", "mi", "mian",
    "miao", "mie", "min", "ming", "miu", "mo", "mou", "mu",
    "na", "nai", "nan", "nang", "nao", "ne", "nei", "nen", "neng", "ng", "ni",
    "nian", "niang", "niao", "nie", "nin", "ning", "niu", "nong", "nou", "nu",
    "nuan", "nun", "nuo", "nv", "nve",
    "o", "ou",
    "pa", "pai", "pan", "pang", "pao", "pei", "pen", "peng", "pi", "pian", "piao",
    "pie", "pin", "ping", "po", "pou", "pu",
    "qi", "qia", "qian", "qiang", "qiao", "qie", "qin", "qing", "qiong", "qiu",
    "qu", "quan", "que", "qun",
    "ran", "rang", "rao", "re", "ren", "reng", "ri", "rong", "rou", "ru", "ruan",
    "rui", "run", "ruo",
    "sa", "sai", "san", "sang", "sao", "se", "sen", "seng", "sha", "shai", "shan",
    "shang", "shao", "she", "shen", "sheng", "shi", "shou", "shu", "shua", "shuai",
    "shuan", "shuang", "shui", "shun", "shuo", "si", "song", "sou", "su", "suan",
    "sui", "sun", "suo",
    "ta", "tai", "tan", "tang", "tao", "te", "teng", "ti", "tian", "tiao", "tie",
    "ting", "tong", "tou", "tu", "tuan", "tui", "tun", "tuo",
    "wa", "wai", "wan", "wang", "wei", "wen", "weng", "wo", "wu",
    "xi", "xia", "xian", "xiang", "xiao", "xie", "xin", "xing", "xiong", "xiu",
    "xu", "xuan", "xue", "xun",
    "ya", "yan", "yang", "yao", "ye", "yi", "yin", "ying", "yo", "yong", "you",
    "yu", "yuan", "yue", "yun",
    "za", "zai", "zan", "zang", "zao", "ze", "zei", "zen", "zeng", "zha", "zhai",
    "zhan", "zhang", "zhao", "zhe", "zhen", "zheng", "zhi", "zhong", "zhou", "zhu",
    "zhua", "zhuai", "zhuan", "zhuang", "zhui", "zhun", "zhuo", "zi", "zong",
    "zou", "zu", "zuan", "zui", "zun", "zuo"
};

const std::unordered_set<std::string>& validSyllables() {
    static std::unordered_set<std::string> set;
    static bool inited = false;
    if (!inited) {
        for (const char* s : kSyllableList) set.insert(s);
        inited = true;
    }
    return set;
}

// 最大音节长度
const size_t kMaxSyllableLen = 6;
}  // namespace

std::string PinyinSegmenter::sanitize(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        if (c >= 'A' && c <= 'Z') {
            out.push_back(static_cast<char>(c - 'A' + 'a'));
        } else if (c >= 'a' && c <= 'z') {
            out.push_back(c);
        } else if (c == '\'' || c == '-') {
            // 保留音节分隔符，用于手动分隔音节如 xi'an → [xi, an]（P1-4）
            out.push_back(c);
        }
        // 其它字符（空格、数字、符号）直接丢弃，交由 InputSession 决定
    }
    return out;
}

bool PinyinSegmenter::isValidSyllable(const std::string& s) const {
    return validSyllables().count(s) > 0;
}

std::vector<std::string> PinyinSegmenter::splitFull(const std::string& raw) const {
    std::string s = sanitize(raw);
    std::vector<std::string> res;
    size_t i = 0;
    const size_t n = s.size();
    while (i < n) {
        // 遇到分隔符：跳过并强制断开音节（P1-4）
        if (s[i] == '\'' || s[i] == '-') {
            ++i;
            continue;
        }
        // 计算到下一个分隔符（或串尾）的最大尝试长度，不超过 kMaxSyllableLen
        size_t maxLen = 0;
        for (size_t j = i; j < n && j < i + kMaxSyllableLen; ++j) {
            if (s[j] == '\'' || s[j] == '-') break;
            ++maxLen;
        }
        if (maxLen == 0) { ++i; continue; }  // 安全保护
        bool matched = false;
        // 由长到短尝试
        for (size_t L = maxLen; L >= 1; --L) {
            std::string cand = s.substr(i, L);
            if (isValidSyllable(cand)) {
                res.push_back(cand);
                i += L;
                matched = true;
                break;
            }
        }
        if (!matched) {
            // 无法识别：作为单字符音节兜底，保证前进
            res.push_back(s.substr(i, 1));
            ++i;
        }
    }
    return res;
}

std::vector<std::string> PinyinSegmenter::toAbbrev(const std::vector<std::string>& full) {
    std::vector<std::string> ab;
    ab.reserve(full.size());
    for (const auto& syl : full) {
        if (!syl.empty()) {
            ab.push_back(syl.substr(0, 1));
        }
    }
    return ab;
}

std::vector<Segment> PinyinSegmenter::segment(const std::string& raw) const {
    std::vector<Segment> out;
    std::string clean = sanitize(raw);
    if (clean.empty()) return out;

    Segment fullSeg;
    fullSeg.raw = clean;
    fullSeg.isAbbrev = false;
    fullSeg.syllables = splitFull(clean);
    fullSeg.normalized = fullSeg.syllables;
    out.push_back(fullSeg);

    Segment abbrevSeg;
    abbrevSeg.raw = clean;
    abbrevSeg.isAbbrev = true;
    abbrevSeg.syllables = toAbbrev(fullSeg.syllables);
    abbrevSeg.normalized = abbrevSeg.syllables;
    out.push_back(abbrevSeg);

    return out;
}

}  // namespace hanpinyin
