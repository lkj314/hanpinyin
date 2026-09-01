// HanPinyin · 拼音前缀树实现

#include "trie.h"
#include <algorithm>

namespace hanpinyin {

Trie::Trie() : root_(new Node()), nodeCount_(1) {}

Trie::~Trie() {
    deleteNode(root_);
}

void Trie::deleteNode(Node* n) {
    if (!n) return;
    for (auto& kv : n->children) {
        deleteNode(kv.second);
    }
    delete n;
}

void Trie::clear() {
    deleteNode(root_);
    root_ = new Node();
    nodeCount_ = 1;
}

void Trie::insert(const std::vector<std::string>& syllables, const Candidate& cand) {
    Node* cur = root_;
    for (const auto& syl : syllables) {
        auto it = cur->children.find(syl);
        if (it == cur->children.end()) {
            Node* next = new Node();
            cur->children[syl] = next;
            ++nodeCount_;
            cur = next;
        } else {
            cur = it->second;
        }
    }
    cur->cands.push_back(cand);
}

int Trie::collect(int start, const std::vector<std::string>& syllables,
                  std::vector<Candidate>& out) const {
    int before = static_cast<int>(out.size());
    const Node* cur = root_;
    const int n = static_cast<int>(syllables.size());
    // 逐音节向下走，沿途每个节点的候选都收集（前缀匹配）
    for (int i = start; i < n; ++i) {
        auto it = cur->children.find(syllables[i]);
        if (it == cur->children.end()) break;
        cur = it->second;
        for (const auto& c : cur->cands) {
            out.push_back(c);
        }
    }
    return static_cast<int>(out.size()) - before;
}

void Trie::collectWithLen(int start, const std::vector<std::string>& syllables,
                          std::vector<std::pair<int, Candidate>>& out) const {
    const Node* cur = root_;
    const int n = static_cast<int>(syllables.size());
    int depth = 0;
    for (int i = start; i < n; ++i) {
        auto it = cur->children.find(syllables[i]);
        if (it == cur->children.end()) break;
        cur = it->second;
        ++depth;
        for (const auto& c : cur->cands) {
            out.emplace_back(depth, c);
        }
    }
}

}  // namespace hanpinyin
