// HanPinyin · 用户词库
// JSON 持久化（key 为音节序列 join(' ')），记录历史选择以叠加词频。
// 本文件属于 core，严禁 #include <windows.h>。

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "types.h"

namespace hanpinyin {

class UserDict {
public:
    UserDict();

    // 从 JSON 文件加载（对象：{ "音节序列": [["韩语", 词频], ...], ... }）
    void load(const std::string& path);
    // 写回 JSON 文件
    void save(const std::string& path) const;
    // 重新加载（配置热更新时调用）
    void reload(const std::string& path);

    // 记录一次选择：key = join(syllables, ' ')
    void record(const std::vector<std::string>& syllables, const std::wstring& korean);

    // 取该 key 的最大用户词频（作为候选 freq 的叠加量）
    int getBoost(const std::string& key) const;

    // 取某 key 下用户最常选的韩语（用于提示，可选）
    std::wstring getPreferred(const std::string& key) const;

    size_t entryCount() const { return words_.size(); }

private:
    // key -> [(韩语, 词频), ...]
    std::unordered_map<std::string, std::vector<std::pair<std::wstring, int>>> words_;

    static std::string makeKey(const std::vector<std::string>& syllables);
};

}  // namespace hanpinyin
