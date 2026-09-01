// HanPinyin · 核心引擎薄封装（CCoreWrapper）
// 隔离 TSF/Windows（wchar_t / UTF-16）与 core（UTF-8 std::string）。
// TIP 层只与本类打交道：加载词库、取候选、控制模糊音。

#pragma once

#include <windows.h>
#include <string>
#include <vector>

#include "../core/types.h"
#include "../core/candidate_manager.h"
#include "../core/dictionary.h"
#include "../core/user_dict.h"
#include "../core/fuzzy_normalizer.h"
#include "../core/pinyin_segmenter.h"
#include "../core/config_model.h"

namespace hanpinyin {
namespace tsf {

class CCoreWrapper {
public:
    CCoreWrapper();
    ~CCoreWrapper();

    // 装载词库：主词库 / 短语库 / 用户词库（宽路径，UTF-16）。
    // 全部加载成功返回 true；任一文件不存在时以空库兜底，不致命。
    bool LoadDict(const std::wstring& mainPath,
                  const std::wstring& phrasePath,
                  const std::wstring& userPath);

    // 重新装载（热重载入口）
    bool ReloadDict(const std::wstring& mainPath,
                    const std::wstring& phrasePath,
                    const std::wstring& userPath);

    // 设置模糊音开关（经 ConfigModel 刷新 FuzzyNormalizer）
    void SetFuzzyEnabled(bool on);

    // 处理一段拼音（UTF-16，仅含 a-z 及音节分隔符）。
    // 返回候选列表（korean 为 UTF-16，可直接用于 SetText / D2D 绘制）。
    hanpinyin::CandidateList Process(const std::wstring& pinyinW);

private:
    hanpinyin::PinyinSegmenter segmenter_;
    hanpinyin::Dictionary dict_;
    hanpinyin::UserDict userDict_;
    hanpinyin::FuzzyNormalizer normalizer_;
    hanpinyin::ConfigModel config_;
    hanpinyin::CandidateManager mgr_;

    // 将 UTF-16 宽路径转为 UTF-8（core 以 std::string 路径读取文件）
    static std::string WidePathToUtf8(const std::wstring& w);
};

}  // namespace tsf
}  // namespace hanpinyin
