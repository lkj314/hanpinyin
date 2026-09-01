// HanPinyin · 拼音→韩语 前缀树（按音节切边）
// Trie 的边为「单个音节」（std::string），节点上挂载候选词条。
// 同时支持全拼与缩写查询：缩写词条在装载时即以「首字母序列」为键插入。

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "types.h"

namespace hanpinyin {

class Trie {
public:
    Trie();
    ~Trie();

    // 插入一条候选（按音节序列走到终端节点并追加候选）
    void insert(const std::vector<std::string>& syllables, const Candidate& cand);

    // 从 start 位置开始，收集与 syllables[start..] 形成前缀匹配的所有词条。
    // 即：凡是「音节序列为 syllables[start..] 前缀」的已插入词条都会被收集。
    // 返回收集到的候选数量。out 累积到传入的 vector。
    int collect(int start, const std::vector<std::string>& syllables,
                std::vector<Candidate>& out) const;

    // 供 Viterbi 使用：从 start 开始收集所有前缀匹配词条，并附带其音节长度。
    // out 元素为 (音节长度, 候选)。
    void collectWithLen(int start, const std::vector<std::string>& syllables,
                        std::vector<std::pair<int, Candidate>>& out) const;

    // 清空整棵树（用于配置热更新时重载词库）
    void clear();

    size_t nodeCount() const { return nodeCount_; }

private:
    struct Node {
        std::vector<Candidate> cands;             // 该节点（即该音节序列）挂载的候选
        std::unordered_map<std::string, Node*> children;
    };

    Node* root_;
    size_t nodeCount_;

    void deleteNode(Node* n);
};

}  // namespace hanpinyin
