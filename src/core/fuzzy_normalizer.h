// HanPinyin · 模糊音归一化器
// 完整模糊集（受 ConfigModel.fuzzyOn 驱动，默认开启）：
//   声母：zh→z, ch→c, sh→s
//   后鼻音（长→短）：ang→an, eng→en, ing→in
//   单字母：n→l, r→l
// 归一为单向查词（long→short），展示仍使用用户原始拼音。
// 为支持「双向」模糊（an↔ang 等），另提供 reverse() 生成反方向形式，
// 由 CandidateManager 同时查询两种形式，避免归一化环路。
// 本文件属于 core，严禁 #include <windows.h>。

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "types.h"

namespace hanpinyin {

class ConfigModel;  // 前置声明

class FuzzyNormalizer {
public:
    FuzzyNormalizer();

    // 依据配置刷新内部模糊映射表
    void setConfig(const ConfigModel& cfg);

    // 归一化到「规范短形式」（long→short）。fuzzyOn 关闭或无可映射项时原样返回。
    std::string normalize(const std::string& syllable) const;

    // 归一化到「反方向长形式」（short→long），用于双向模糊查询。
    std::string reverse(const std::string& syllable) const;

    bool isFuzzyOn() const { return fuzzyOn_; }

private:
    bool fuzzyOn_ = true;
    // 声母映射（如 zh→z）及反方向（z→zh）
    std::unordered_map<std::string, std::string> initials_;
    std::unordered_map<std::string, std::string> initialsInv_;
    // 后鼻音 长→短（ang→an）及 短→长（an→ang）
    std::unordered_map<std::string, std::string> finalsL2S_;
    std::unordered_map<std::string, std::string> finalsS2L_;
    // 单字母（n→l）及反方向（l→n）
    std::unordered_map<std::string, std::string> singles_;
    std::unordered_map<std::string, std::string> singlesInv_;

    void loadDefaultMap();
};

}  // namespace hanpinyin
