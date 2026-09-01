// HanPinyin · 模糊音归一化器实现
//
// 完整模糊集（受 ConfigModel.fuzzyOn 驱动，默认开启）：
//   声母：zh→z, ch→c, sh→s
//   单字母声母（n/l、r/l 同为声母级混淆）：n→l, r→l
//   后鼻音（长→短）：ang→an, eng→en, ing→in
//
// 设计要点：
//   - normalize() 产生「规范短形式」（long→short），供词典查词。
//   - reverse()   产生「反方向长形式」（short→long），由 CandidateManager 同时查询，避免归一化环路。
//   - zh/ch/sh 与 n/l、r/l 均作为「声母前缀」替换（startsWith）：
//     之前的实现把 n/l 当作整音节精确匹配，导致 nan→lan、ren→len 失效。
//   - setConfig() 解析 "ang=an" 这类配置对时，必须同步补上反向映射 an→ang，
//     否则 reverse() 的短→长维度为空（旧实现只有 loadDefaultMap 填了反向，setConfig 重建时漏掉）。
//
// 本文件属于 core，严禁 #include <windows.h>。

#include "fuzzy_normalizer.h"
#include "config_model.h"
#include <algorithm>

namespace hanpinyin {

namespace {
bool startsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}
bool endsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}
bool isLongFinal(const std::string& s) {
    return s == "ang" || s == "eng" || s == "ing";
}
bool isShortFinal(const std::string& s) {
    return s == "an" || s == "en" || s == "in";
}
// 是否为「两字母声母」（zh/ch/sh）
bool isZhInitial(const std::string& s) {
    return s.size() == 2 && s[1] == 'h' &&
           (s[0] == 'z' || s[0] == 'c' || s[0] == 's');
}
}  // namespace

FuzzyNormalizer::FuzzyNormalizer() {
    loadDefaultMap();
}

void FuzzyNormalizer::loadDefaultMap() {
    initials_.clear();
    initialsInv_.clear();
    finalsL2S_.clear();
    finalsS2L_.clear();
    singles_.clear();
    singlesInv_.clear();

    // 声母：zh→z, ch→c, sh→s（前向 + 反向都建好）
    initials_["zh"] = "z";  initialsInv_["z"] = "zh";
    initials_["ch"] = "c";  initialsInv_["c"] = "ch";
    initials_["sh"] = "s";  initialsInv_["s"] = "sh";

    // 后鼻音 长→短 / 短→长（双向）
    finalsL2S_["ang"] = "an";  finalsS2L_["an"] = "ang";
    finalsL2S_["eng"] = "en";  finalsS2L_["en"] = "eng";
    finalsL2S_["ing"] = "in";  finalsS2L_["in"] = "ing";

    // 单字母声母：n→l, r→l（前向；反向只取其一避免冲突）
    singles_["n"] = "l";  singlesInv_["l"] = "n";
    singles_["r"] = "l";  // 反向 l→n 已设置，r/l 合并到 n
}

void FuzzyNormalizer::setConfig(const ConfigModel& cfg) {
    fuzzyOn_ = cfg.isFuzzyOn();
    const auto& pairs = cfg.getFuzzyPairs();
    if (pairs.empty()) {
        loadDefaultMap();
        return;
    }

    // 依据配置对重建（默认配置即非空，会走此分支）
    initials_.clear();
    initialsInv_.clear();
    finalsL2S_.clear();
    finalsS2L_.clear();
    singles_.clear();
    singlesInv_.clear();

    for (const auto& p : pairs) {
        size_t pos = p.find('=');
        if (pos == std::string::npos) continue;
        std::string from = p.substr(0, pos);
        std::string to = p.substr(pos + 1);
        if (from.empty() || to.empty()) continue;

        if (isZhInitial(from) || isZhInitial(to)) {
            // 声母：zh=z 等
            initials_[from] = to;
            initialsInv_[to] = from;
        } else if (isLongFinal(from) && isShortFinal(to)) {
            // ang=an：长→短，并补 短→长
            finalsL2S_[from] = to;
            finalsS2L_[to] = from;
        } else if (isShortFinal(from) && isLongFinal(to)) {
            // an=ang：短→长，并补 长→短
            finalsS2L_[from] = to;
            finalsL2S_[to] = from;
        } else if (from.size() == 1 && to.size() == 1) {
            // 单字母声母：n=l, r=l（作为前缀替换）
            singles_[from] = to;
            singlesInv_[to] = from;
        }
    }
}

std::string FuzzyNormalizer::normalize(const std::string& syllable) const {
    if (!fuzzyOn_ || syllable.empty()) return syllable;
    std::string s = syllable;

    // 声母（如 zh→z）——前缀替换
    for (const auto& kv : initials_) {
        if (startsWith(s, kv.first)) {
            s = kv.second + s.substr(kv.first.size());
            break;
        }
    }
    // 单字母声母（n→l, r→l）——前缀替换（关键修复点）
    for (const auto& kv : singles_) {
        if (startsWith(s, kv.first)) {
            s = kv.second + s.substr(kv.first.size());
            break;
        }
    }
    // 后鼻音 长→短（如 ang→an）——后缀替换
    for (const auto& kv : finalsL2S_) {
        if (endsWith(s, kv.first)) {
            s = s.substr(0, s.size() - kv.first.size()) + kv.second;
            break;
        }
    }
    return s;
}

std::string FuzzyNormalizer::reverse(const std::string& syllable) const {
    if (!fuzzyOn_ || syllable.empty()) return syllable;
    std::string s = syllable;

    // 声母反向（z→zh）——前缀替换
    for (const auto& kv : initialsInv_) {
        if (startsWith(s, kv.first)) {
            s = kv.second + s.substr(kv.first.size());
            break;
        }
    }
    // 单字母声母反向（l→n）——前缀替换
    for (const auto& kv : singlesInv_) {
        if (startsWith(s, kv.first)) {
            s = kv.second + s.substr(kv.first.size());
            break;
        }
    }
    // 后鼻音 短→长（如 an→ang）——后缀替换
    for (const auto& kv : finalsS2L_) {
        if (endsWith(s, kv.first)) {
            s = s.substr(0, s.size() - kv.first.size()) + kv.second;
            break;
        }
    }
    return s;
}

}  // namespace hanpinyin
