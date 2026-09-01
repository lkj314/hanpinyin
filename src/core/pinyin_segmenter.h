// HanPinyin · 拼音音节分割器（含首字母缩写分支）
// 采用正向最大匹配（FMM）：从串首尝试最长合法拼音音节。
// 同时产出「全拼切分」与「首字母缩写切分」两套 Segment。

#pragma once

#include <string>
#include <vector>
#include "types.h"

namespace hanpinyin {

class PinyinSegmenter {
public:
    // 对一段连续拼音串（无空格、已小写、仅含 a-z）做切分。
    // 返回两个 Segment：
    //   [0] isAbbrev=false：全拼音节序列（如 [wan, le, yi, xia, wu]）
    //   [1] isAbbrev=true ：首字母缩写序列（如 [w, l, y, x, w]）
    // 若输入为空，返回空 vector。
    std::vector<Segment> segment(const std::string& raw) const;

    // 仅做全拼切分，返回音节序列。
    std::vector<std::string> splitFull(const std::string& raw) const;

    // 由全拼音节序列生成缩写首字母序列（每个音节取首字母）。
    static std::vector<std::string> toAbbrev(const std::vector<std::string>& full);

private:
    // 判断是否为合法拼音音节
    bool isValidSyllable(const std::string& s) const;

    // 对 raw 做合法性清洗：转小写、仅保留 a-z
    static std::string sanitize(const std::string& raw);
};

}  // namespace hanpinyin
