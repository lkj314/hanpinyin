// HanPinyin · 候选词管理器
// 流程：对每个 Segment 做模糊归一 → Trie 前缀/多音节查询（全拼 + 缩写两套）
//       → 用户词库叠加 boost → 多音节走 Viterbi 求最优词序列
//       → mergeAndRank（全拼优先 + 缩写合并）→ 输出 CandidateList。
// 接口契约（§7.5）：kFull 候选恒排在所有 kAbbrev 之前；同韩文被两者命中时合并一条
//                （取较大 freq，source 取较高优先级 kMain>kPhrase>kUser）。
// 本文件属于 core，严禁 #include <windows.h>。

#pragma once

#include <string>
#include <vector>
#include "types.h"
#include "trie.h"
#include "dictionary.h"
#include "user_dict.h"
#include "fuzzy_normalizer.h"

namespace hanpinyin {

class CandidateManager {
public:
    CandidateManager();

    // 注入依赖（AppContext 在构造时绑定）
    void setDictionary(Dictionary* dict) { dict_ = dict; }
    void setUserDict(UserDict* ud) { userDict_ = ud; }
    void setNormalizer(FuzzyNormalizer* fn) { normalizer_ = fn; }

    // 主入口：由一组 Segment 生成候选列表
    CandidateList getCandidates(const std::vector<Segment>& segs);

    // 供单测使用的 Viterbi 最优词序列（公开封装，内部调用私有 viterbi）
    std::vector<Candidate> getBestSequence(const std::vector<std::string>& syllables);

    // 单页候选数量上限
    static const int kPageSize = 5;

private:
    Dictionary* dict_ = nullptr;
    UserDict* userDict_ = nullptr;
    FuzzyNormalizer* normalizer_ = nullptr;

    // 多音节整句最优组合：状态=位置，转移代价=词频负对数，回溯最优词序列
    std::vector<Candidate> viterbi(const std::vector<std::string>& syllables) const;

    // 合并 + 排序：全拼优先，缩写合并；同韩文取较大 freq / 较高 source 优先级
    CandidateList mergeAndRank(std::vector<Candidate> full,
                               std::vector<Candidate> abbrev) const;

    // 按韩文去重（保留较大 freq 与较高 source 优先级），保持 source_pinyin
    static std::vector<Candidate> dedupByKorean(std::vector<Candidate> cands);

    // 按 freq 降序排序
    static void sortByFreqDesc(std::vector<Candidate>& cands);

    // Viterbi 最优词序列对候选的轻微加权（让分解感知的词略靠前，但不压过短语）
    static const int kViterbiBoost = 10;
};

}  // namespace hanpinyin
