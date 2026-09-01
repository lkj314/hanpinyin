// HanPinyin · 词库数据模型
// 装载主词库（main_dict.json）与短语库（phrases.json），注入 Trie。
// 缩写词条在装载时即以「首字母序列」为键插入，便于缩写查询。
// 本文件属于 core，严禁 #include <windows.h>。

#pragma once

#include <string>
#include <vector>
#include "types.h"
#include "trie.h"

namespace hanpinyin {

class Dictionary {
public:
    Dictionary();

    // 装载主词库：数组，每条 { "pinyin": "...", "candidates": [[韩语, 词频], ...] }
    void loadMain(const std::string& path);
    // 装载短语库：数组，每条 { "pinyin": "...", "korean": "..." }
    void loadPhrases(const std::string& path);
    // 重新装载（配置热更新时调用）
    void reload(const std::string& mainPath, const std::string& phrasePath);

    const Trie& getTrie() const { return trie_; }
    Trie& getTrie() { return trie_; }

    // 运行时增删词条（配置面板调用），并同步内存列表以便持久化
    void addMainEntry(const std::string& pinyin, const std::wstring& korean, int freq);
    void removeMainEntry(const std::string& pinyin);
    void addPhraseEntry(const std::string& pinyin, const std::wstring& korean);
    void removePhraseEntry(const std::string& pinyin);

    // 将内存列表写回 JSON 文件
    void saveMain(const std::string& path) const;
    void savePhrases(const std::string& path) const;

    size_t mainEntryCount() const { return mainEntries_.size(); }
    size_t phraseEntryCount() const { return phraseEntries_.size(); }

private:
    struct MainEntry {
        std::string pinyin;
        std::vector<std::pair<std::wstring, int>> cands;  // 韩语 + 词频
    };
    struct PhraseEntry {
        std::string pinyin;
        std::wstring korean;
    };

    Trie trie_;
    std::vector<MainEntry> mainEntries_;
    std::vector<PhraseEntry> phraseEntries_;

    // 将单条主词库条目插入 Trie（全拼 + 缩写两套）
    void insertMainEntry(const MainEntry& e);
    // 将单条短语插入 Trie
    void insertPhraseEntry(const PhraseEntry& e);

    // 插入缩写 + 混合简拼（逐音节全拼/首字母组合）键
    void insertAbbrevMasks(const std::vector<std::string>& syllables,
                          const std::vector<std::string>& ab,
                          const Candidate& base);

    // "wan le" -> ["wan","le"]
    static std::vector<std::string> splitPinyin(const std::string& pinyin);
    // 取每个音节首字母："wan le" -> ["w","l"]
    static std::vector<std::string> abbrevOf(const std::vector<std::string>& syllables);
};

}  // namespace hanpinyin
