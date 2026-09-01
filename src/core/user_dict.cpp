// HanPinyin · 用户词库实现

#include "user_dict.h"
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

UserDict::UserDict() = default;

std::string UserDict::makeKey(const std::vector<std::string>& syllables) {
    std::string key;
    for (size_t i = 0; i < syllables.size(); ++i) {
        if (i > 0) key.push_back(' ');
        key += syllables[i];
    }
    return key;
}

void UserDict::load(const std::string& path) {
    std::string text;
    if (!readFile(path, text)) return;
    hp_json::Value v = hp_json::parse(text);
    if (!v.isObject()) return;
    for (const auto& item : v.obj) {
        const std::string& key = item.first;
        const hp_json::Value& arr = item.second;
        if (!arr.isArray()) continue;
        std::vector<std::pair<std::wstring, int>> list;
        for (const auto& pair : arr.arr) {
            if (pair.isArray() && pair.size() >= 2 && pair[0].isString()) {
                list.emplace_back(utf8_to_wstring(pair[0].asString()), pair[1].asInt());
            }
        }
        if (!list.empty()) words_[key] = std::move(list);
    }
}

void UserDict::save(const std::string& path) const {
    hp_json::Value root(hp_json::Value::Type::Object);
    for (const auto& kv : words_) {
        hp_json::Value arr(hp_json::Value::Type::Array);
        for (const auto& p : kv.second) {
            hp_json::Value pair(hp_json::Value::Type::Array);
            pair.arr.push_back(hp_json::Value(wstring_to_utf8(p.first)));
            pair.arr.push_back(hp_json::Value(p.second));
            arr.arr.push_back(pair);
        }
        root[kv.first] = arr;
    }
    writeFile(path, hp_json::stringify(root, 2));
}

void UserDict::reload(const std::string& path) {
    words_.clear();
    load(path);
}

void UserDict::record(const std::vector<std::string>& syllables, const std::wstring& korean) {
    std::string key = makeKey(syllables);
    auto it = words_.find(key);
    if (it == words_.end()) {
        words_[key] = { {korean, 1} };
        return;
    }
    for (auto& p : it->second) {
        if (p.first == korean) {
            ++p.second;
            return;
        }
    }
    it->second.emplace_back(korean, 1);
}

int UserDict::getBoost(const std::string& key) const {
    auto it = words_.find(key);
    if (it == words_.end()) return 0;
    int maxFreq = 0;
    for (const auto& p : it->second) {
        if (p.second > maxFreq) maxFreq = p.second;
    }
    return maxFreq;
}

std::wstring UserDict::getPreferred(const std::string& key) const {
    auto it = words_.find(key);
    if (it == words_.end() || it->second.empty()) return L"";
    const auto* best = &it->second[0];
    for (const auto& p : it->second) {
        if (p.second > best->second) best = &p;
    }
    return best->first;
}

}  // namespace hanpinyin
