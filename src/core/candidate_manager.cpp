// HanPinyin · 候选词管理器实现

#include "candidate_manager.h"
#include <cmath>
#include <algorithm>
#include <unordered_set>

namespace hanpinyin {

CandidateManager::CandidateManager() = default;

CandidateList CandidateManager::getCandidates(const std::vector<Segment>& segs) {
    std::vector<Candidate> fullCands;
    std::vector<Candidate> abbrevCands;

    // 拼接全拼段落的音节序列，供 Viterbi 使用：
    //   fullOrigSyllables —— 用户原始输入
    //   fullRevSyllables  —— 反方向归一化（reverse(normalize) = 规范长形式 = Trie 键）
    // 二者并集运行 Viterbi，确保模糊（短形式）输入也能正确加权（P1-2 修复）。
    std::vector<std::string> fullOrigSyllables;
    std::vector<std::string> fullRevSyllables;

    for (const auto& seg : segs) {
        // 模糊归一化每个音节（规范短形式 + 反方向长形式）
        std::vector<std::string> norm, revSeg;
        norm.reserve(seg.syllables.size());
        revSeg.reserve(seg.syllables.size());
        for (const auto& s : seg.syllables) {
            std::string n = normalizer_ ? normalizer_->normalize(s) : s;
            std::string r = normalizer_ ? normalizer_->reverse(s) : s;
            norm.push_back(n);
            revSeg.push_back(r);
        }
        if (!seg.isAbbrev) {
            fullOrigSyllables.insert(fullOrigSyllables.end(),
                                     seg.syllables.begin(), seg.syllables.end());
            fullRevSyllables.insert(fullRevSyllables.end(),
                                    revSeg.begin(), revSeg.end());
        }

        const int n = static_cast<int>(seg.syllables.size());
        // 从每个起始位置收集前缀匹配（覆盖所有连续子词）
        // 查询三种音节序列：原始 / 规范模糊 / 反方向模糊（实现双向模糊）
        for (int i = 0; i < n; ++i) {
            auto collectAll = [&](const std::vector<std::string>& seq) {
                if (seg.isAbbrev) {
                    dict_->getTrie().collect(i, seq, abbrevCands);
                } else {
                    dict_->getTrie().collect(i, seq, fullCands);
                }
            };
            collectAll(seg.syllables);  // 原始精确
            collectAll(norm);           // 规范模糊（long→short）
            collectAll(revSeg);         // 反方向模糊（short→long，双向）
        }
    }

    // 用户词库频率叠加（按 source_pinyin 取 boost）
    if (userDict_) {
        for (auto& c : fullCands) {
            c.freq += userDict_->getBoost(c.source_pinyin);
        }
        for (auto& c : abbrevCands) {
            c.freq += userDict_->getBoost(c.source_pinyin);
        }
    }

    // Viterbi 最优词序列 → 对命中的全拼候选做轻微加权
    // Trie 以「规范长形式（canonical）」拼音为键；模糊（短形式）输入需经 reverse(normalize)
    // 还原为规范形式才能命中，故对「原始输入」与「反方向归一化」两种序列各跑一遍并取并集（P1-2）。
    std::unordered_set<std::string> pathKeys;
    auto collectPathKeys = [&](const std::vector<std::string>& syls) {
        std::vector<Candidate> path = viterbi(syls);
        for (const auto& c : path) pathKeys.insert(c.source_pinyin);
    };
    collectPathKeys(fullOrigSyllables);
    collectPathKeys(fullRevSyllables);
    for (auto& c : fullCands) {
        if (pathKeys.count(c.source_pinyin)) c.freq += kViterbiBoost;
    }

    CandidateList result = mergeAndRank(fullCands, abbrevCands);

    // 缺词降级：无匹配候选时，返回原始拼音串本身作为候选（P1-5）
    // 让用户至少看到自己输入了什么，不吞键
    if (result.items.empty() && !segs.empty()) {
        // 拼接第一个全拼段的音节作为 source_pinyin
        std::string rawPinyin;
        for (const auto& seg : segs) {
            if (!seg.isAbbrev) {
                for (size_t i = 0; i < seg.syllables.size(); ++i) {
                    if (!rawPinyin.empty()) rawPinyin += ' ';
                    rawPinyin += seg.syllables[i];
                }
                break;
            }
        }
        if (rawPinyin.empty()) rawPinyin = segs[0].raw;

        Candidate fallback;
        fallback.source_pinyin = rawPinyin;
        // 将 ASCII 拼音转为 wstring（韩文字段复用为原始拼音显示）
        std::wstring wKorean;
        for (char c : rawPinyin) wKorean.push_back(static_cast<wchar_t>(c));
        fallback.korean = wKorean;
        fallback.freq = 0;
        fallback.source = Source::kMain;
        fallback.matchMode = MatchMode::kFull;
        result.items.push_back(fallback);
        result.hasMore = false;
    }

    return result;
}

std::vector<Candidate> CandidateManager::viterbi(
    const std::vector<std::string>& syllables) const {
    const int n = static_cast<int>(syllables.size());
    if (n == 0 || !dict_) return {};

    const double INF = 1e18;
    std::vector<double> best(n + 1, INF);
    std::vector<int> prev(n + 1, -1);
    std::vector<Candidate> candAt(n + 1);

    best[0] = 0.0;
    for (int i = 0; i < n; ++i) {
        if (best[i] >= INF) continue;
        std::vector<std::pair<int, Candidate>> words;
        dict_->getTrie().collectWithLen(i, syllables, words);
        for (const auto& w : words) {
            int L = w.first;
            if (i + L > n) continue;
            double cost = best[i] + (-std::log(static_cast<double>(w.second.freq) + 1.0));
            if (cost < best[i + L]) {
                best[i + L] = cost;
                prev[i + L] = i;
                candAt[i + L] = w.second;
            }
        }
        // 容错：本位置无词典词时，以「未识别音节」单步前进（高代价），保证全程可达
        if (best[i + 1] >= INF && i + 1 <= n) {
            Candidate junk;
            junk.korean = L"";
            junk.source_pinyin = syllables[i];
            junk.freq = 1;
            junk.source = Source::kMain;
            junk.matchMode = MatchMode::kFull;
            best[i + 1] = best[i] + 1000.0;
            prev[i + 1] = i;
            candAt[i + 1] = junk;
        }
    }

    // 回溯最优路径
    std::vector<Candidate> path;
    int cur = n;
    while (cur > 0 && prev[cur] != -1) {
        path.push_back(candAt[cur]);
        cur = prev[cur];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<Candidate> CandidateManager::getBestSequence(
    const std::vector<std::string>& syllables) {
    return viterbi(syllables);
}

std::vector<Candidate> CandidateManager::dedupByKorean(std::vector<Candidate> cands) {
    std::vector<Candidate> out;
    for (auto& c : cands) {
        bool found = false;
        for (auto& o : out) {
            if (o.korean == c.korean) {
                found = true;
                if (c.freq > o.freq) o.freq = c.freq;
                // source 取较高优先级（枚举值更小者优先：kMain<kPhrase<kUser）
                if (static_cast<int>(c.source) < static_cast<int>(o.source)) {
                    o.source = c.source;
                }
                break;
            }
        }
        if (!found) out.push_back(c);
    }
    return out;
}

void CandidateManager::sortByFreqDesc(std::vector<Candidate>& cands) {
    std::sort(cands.begin(), cands.end(), [](const Candidate& a, const Candidate& b) {
        if (a.freq != b.freq) return a.freq > b.freq;
        // 词频相同：source 优先级高者在前
        if (a.source != b.source) return static_cast<int>(a.source) < static_cast<int>(b.source);
        return a.korean < b.korean;
    });
}

CandidateList CandidateManager::mergeAndRank(std::vector<Candidate> full,
                                             std::vector<Candidate> abbrev) const {
    // 1. 各自按韩文去重（取较大 freq / 较高 source）
    full = dedupByKorean(std::move(full));
    abbrev = dedupByKorean(std::move(abbrev));

    // 2. 各自按 freq 降序
    sortByFreqDesc(full);
    sortByFreqDesc(abbrev);

    // 3. 合并：全拼候选全部在前；缩写候选若与全拼同韩文则合并为一条（kFull 优先）
    CandidateList out;
    for (auto& c : full) out.items.push_back(c);
    for (auto& a : abbrev) {
        bool merged = false;
        for (auto& c : out.items) {
            if (c.korean == a.korean) {
                merged = true;
                if (a.freq > c.freq) c.freq = a.freq;
                if (static_cast<int>(a.source) < static_cast<int>(c.source)) {
                    c.source = a.source;
                }
                // 保留 kFull 匹配模式
                c.matchMode = MatchMode::kFull;
                break;
            }
        }
        if (!merged) out.items.push_back(a);
    }

    // 4. 分页信息
    out.hasMore = static_cast<int>(out.items.size()) > kPageSize;
    out.page = 0;
    return out;
}

}  // namespace hanpinyin
