// HanPinyin · 核心引擎薄封装实现

#include "tsf_core_wrapper.h"

namespace hanpinyin {
namespace tsf {

CCoreWrapper::CCoreWrapper() {
    // 默认开启模糊音
    config_.setFuzzyOn(true);
    normalizer_.setConfig(config_);
    // 绑定候选管理器依赖
    mgr_.setDictionary(&dict_);
    mgr_.setUserDict(&userDict_);
    mgr_.setNormalizer(&normalizer_);
}

CCoreWrapper::~CCoreWrapper() = default;

std::string CCoreWrapper::WidePathToUtf8(const std::wstring& w) {
    return hanpinyin::wstring_to_utf8(w);
}

bool CCoreWrapper::LoadDict(const std::wstring& mainPath,
                            const std::wstring& phrasePath,
                            const std::wstring& userPath) {
    dict_.loadMain(WidePathToUtf8(mainPath));
    dict_.loadPhrases(WidePathToUtf8(phrasePath));
    userDict_.load(WidePathToUtf8(userPath));
    return true;
}

bool CCoreWrapper::ReloadDict(const std::wstring& mainPath,
                              const std::wstring& phrasePath,
                              const std::wstring& userPath) {
    dict_.reload(WidePathToUtf8(mainPath), WidePathToUtf8(phrasePath));
    userDict_.reload(WidePathToUtf8(userPath));
    return true;
}

void CCoreWrapper::SetFuzzyEnabled(bool on) {
    config_.setFuzzyOn(on);
    normalizer_.setConfig(config_);
}

hanpinyin::CandidateList CCoreWrapper::Process(const std::wstring& pinyinW) {
    // UTF-16 拼音 → UTF-8（core 内部处理 ASCII 拼音）
    std::string utf8 = hanpinyin::wstring_to_utf8(pinyinW);
    if (utf8.empty()) return hanpinyin::CandidateList{};

    // 切分音节（全拼 + 缩写两套 Segment）
    std::vector<hanpinyin::Segment> segs = segmenter_.segment(utf8);
    if (segs.empty()) return hanpinyin::CandidateList{};

    // 候选生成（内部含 Trie / Viterbi / 模糊归一 / 用户词库叠加 / 合并排序）
    return mgr_.getCandidates(segs);
}

}  // namespace tsf
}  // namespace hanpinyin
