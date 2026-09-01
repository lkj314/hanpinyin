// HanPinyin · 词库数据模型实现

#include "dictionary.h"
#include "json.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace hanpinyin {

namespace {
bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}
bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << content;
    return true;
}
}  // namespace

Dictionary::Dictionary() = default;

std::vector<std::string> Dictionary::splitPinyin(const std::string& pinyin) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : pinyin) {
        if (c == ' ') {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            cur.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
        }
        // 其它字符忽略
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::vector<std::string> Dictionary::abbrevOf(const std::vector<std::string>& syllables) {
    std::vector<std::string> ab;
    for (const auto& s : syllables) {
        if (!s.empty()) ab.push_back(s.substr(0, 1));
    }
    return ab;
}

void Dictionary::insertMainEntry(const MainEntry& e) {
    std::vector<std::string> syllables = splitPinyin(e.pinyin);
    if (syllables.empty()) return;
    std::vector<std::string> ab = abbrevOf(syllables);
    for (const auto& kv : e.cands) {
        Candidate c;
        c.korean = kv.first;
        c.source_pinyin = e.pinyin;
        c.freq = kv.second;
        c.source = Source::kMain;
        // 全拼键
        c.matchMode = MatchMode::kFull;
        trie_.insert(syllables, c);
        // 缩写键（仅当缩写与全拼不同，避免无意义重复插入）
        if (!ab.empty() && ab != syllables) {
            Candidate ca = c;
            ca.matchMode = MatchMode::kAbbrev;
            trie_.insert(ab, ca);
        }
    }
}

void Dictionary::insertPhraseEntry(const PhraseEntry& e) {
    std::vector<std::string> syllables = splitPinyin(e.pinyin);
    if (syllables.empty()) return;
    std::vector<std::string> ab = abbrevOf(syllables);
    Candidate c;
    c.korean = e.korean;
    c.source_pinyin = e.pinyin;
    c.freq = 100;  // 短语默认较高词频，确保整句候选优先
    c.source = Source::kPhrase;
    c.matchMode = MatchMode::kFull;
    trie_.insert(syllables, c);
    if (!ab.empty() && ab != syllables) {
        Candidate ca = c;
        ca.matchMode = MatchMode::kAbbrev;
        trie_.insert(ab, ca);
    }
}

void Dictionary::loadMain(const std::string& path) {
    std::string text;
    if (!readFile(path, text)) return;
    hp_json::Value v = hp_json::parse(text);
    if (!v.isArray()) return;
    for (const auto& item : v.arr) {
        if (!item.isObject()) continue;
        if (!item.has("pinyin") || !item.has("candidates")) continue;
        std::string py = item["pinyin"].asString();
        // 定位是否已存在相同拼音的条目：合并重复 key，避免 mainEntries_ 出现多个相同
        // 拼音导致 addMainEntry 只命中首个、且候选栏重复。
        MainEntry* existing = nullptr;
        for (auto& e : mainEntries_) {
            if (e.pinyin == py) { existing = &e; break; }
        }
        std::vector<std::pair<std::wstring, int>> newCands;
        const auto& cands = item["candidates"];
        if (cands.isArray()) {
            for (const auto& pair : cands.arr) {
                // pair 为 [韩语字符串, 词频]
                if (pair.isArray() && pair.size() >= 2 && pair[0].isString()) {
                    std::wstring kor = utf8_to_wstring(pair[0].asString());
                    int freq = pair[1].asInt();
                    newCands.emplace_back(kor, freq);
                }
            }
        }
        if (newCands.empty()) continue;
        if (existing) {
            for (const auto& kv : newCands) existing->cands.emplace_back(kv.first, kv.second);
        } else {
            MainEntry e;
            e.pinyin = py;
            for (const auto& kv : newCands) e.cands.emplace_back(kv.first, kv.second);
            mainEntries_.push_back(e);
        }
        // 仅把“本次 JSON 项”的候选插入 trie，避免重复 key 整条重插导致候选栏重复。
        MainEntry toInsert;
        toInsert.pinyin = py;
        toInsert.cands = newCands;
        insertMainEntry(toInsert);
    }
}

void Dictionary::loadPhrases(const std::string& path) {
    std::string text;
    if (!readFile(path, text)) return;
    hp_json::Value v = hp_json::parse(text);
    if (!v.isArray()) return;
    for (const auto& item : v.arr) {
        if (!item.isObject()) continue;
        if (!item.has("pinyin") || !item.has("korean")) continue;
        PhraseEntry e;
        e.pinyin = item["pinyin"].asString();
        e.korean = utf8_to_wstring(item["korean"].asString());
        phraseEntries_.push_back(e);
        insertPhraseEntry(e);
    }
}

void Dictionary::reload(const std::string& mainPath, const std::string& phrasePath) {
    trie_.clear();
    mainEntries_.clear();
    phraseEntries_.clear();
    loadMain(mainPath);
    loadPhrases(phrasePath);
}

void Dictionary::addMainEntry(const std::string& pinyin, const std::wstring& korean, int freq) {
    // 若已存在相同拼音，则在该条目上追加/累加；只把“新增候选”插入 trie，
    // 不再整条重插，避免 append-only 的 trie 出现候选栏重复。
    for (auto& e : mainEntries_) {
        if (e.pinyin == pinyin) {
            for (auto& kv : e.cands) {
                if (kv.first == korean) {
                    kv.second += freq;   // 仅更新内存词频，不重插 trie（trie 副本频率略滞后，但无重复）
                    return;
                }
            }
            e.cands.emplace_back(korean, freq);
            MainEntry single;
            single.pinyin = pinyin;
            single.cands.emplace_back(korean, freq);
            insertMainEntry(single);     // 仅插入本次新增的候选
            return;
        }
    }
    MainEntry e;
    e.pinyin = pinyin;
    e.cands.emplace_back(korean, freq);
    mainEntries_.push_back(e);
    insertMainEntry(e);                  // 全新拼音：整条插入，无重复
}

void Dictionary::removeMainEntry(const std::string& pinyin) {
    mainEntries_.erase(
        std::remove_if(mainEntries_.begin(), mainEntries_.end(),
                       [&](const MainEntry& e) { return e.pinyin == pinyin; }),
        mainEntries_.end());
    // 词条删除后需重建 Trie（简单稳妥）
    trie_.clear();
    for (const auto& e : mainEntries_) insertMainEntry(e);
    for (const auto& e : phraseEntries_) insertPhraseEntry(e);
}

void Dictionary::addPhraseEntry(const std::string& pinyin, const std::wstring& korean) {
    for (auto& e : phraseEntries_) {
        if (e.pinyin == pinyin) { e.korean = korean; insertPhraseEntry(e); return; }
    }
    PhraseEntry e;
    e.pinyin = pinyin;
    e.korean = korean;
    phraseEntries_.push_back(e);
    insertPhraseEntry(e);
}

void Dictionary::removePhraseEntry(const std::string& pinyin) {
    phraseEntries_.erase(
        std::remove_if(phraseEntries_.begin(), phraseEntries_.end(),
                       [&](const PhraseEntry& e) { return e.pinyin == pinyin; }),
        phraseEntries_.end());
    trie_.clear();
    for (const auto& e : mainEntries_) insertMainEntry(e);
    for (const auto& e : phraseEntries_) insertPhraseEntry(e);
}

void Dictionary::saveMain(const std::string& path) const {
    hp_json::Value root(hp_json::Value::Type::Array);
    for (const auto& e : mainEntries_) {
        hp_json::Value item(hp_json::Value::Type::Object);
        item["pinyin"] = hp_json::Value(e.pinyin);
        hp_json::Value cands(hp_json::Value::Type::Array);
        for (const auto& kv : e.cands) {
            hp_json::Value pair(hp_json::Value::Type::Array);
            pair.arr.push_back(hp_json::Value(wstring_to_utf8(kv.first)));
            pair.arr.push_back(hp_json::Value(kv.second));
            cands.arr.push_back(pair);
        }
        item["candidates"] = cands;
        root.arr.push_back(item);
    }
    writeFile(path, hp_json::stringify(root, 2));
}

void Dictionary::savePhrases(const std::string& path) const {
    hp_json::Value root(hp_json::Value::Type::Array);
    for (const auto& e : phraseEntries_) {
        hp_json::Value item(hp_json::Value::Type::Object);
        item["pinyin"] = hp_json::Value(e.pinyin);
        item["korean"] = hp_json::Value(wstring_to_utf8(e.korean));
        root.arr.push_back(item);
    }
    writeFile(path, hp_json::stringify(root, 2));
}

}  // namespace hanpinyin
