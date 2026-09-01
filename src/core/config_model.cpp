// HanPinyin · 配置数据模型实现

#include "config_model.h"
#include "json.hpp"
#include <fstream>
#include <sstream>

namespace hanpinyin {

namespace {
// 读取整个文件为字符串
bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// 写入整个文件
bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << content;
    return true;
}
}  // namespace

ConfigModel::ConfigModel() {
    // 默认模糊集（当配置未提供时使用）
    fuzzyPairs_ = {
        "zh=z", "ch=c", "sh=s",
        "ang=an", "eng=en", "ing=in",
        "iang=ian", "uang=uan",
        "n=l", "r=l",
        "f=h", "k=g"
    };
}

void ConfigModel::load(const std::string& path) {
    std::string text;
    if (!readFile(path, text)) {
        // 文件缺失：保持默认值
        return;
    }
    hp_json::Value v = hp_json::parse(text);
    if (!v.isObject()) return;

    if (v.has("fuzzyOn")) fuzzyOn_ = v["fuzzyOn"].asBool();

    if (v.has("fuzzyPairs") && v["fuzzyPairs"].isArray()) {
        fuzzyPairs_.clear();
        for (const auto& item : v["fuzzyPairs"].arr) {
            if (item.isString()) fuzzyPairs_.push_back(item.asString());
        }
    }

    if (v.has("hotkey") && v["hotkey"].isObject()) {
        const auto& h = v["hotkey"];
        if (h.has("modifiers")) hotkey_.modifiers = h["modifiers"].asInt();
        if (h.has("vk")) hotkey_.vk = h["vk"].asInt();
    }

    if (v.has("mainDictPath")) mainDictPath_ = v["mainDictPath"].asString();
    if (v.has("phraseDictPath")) phraseDictPath_ = v["phraseDictPath"].asString();
    if (v.has("userDictPath")) userDictPath_ = v["userDictPath"].asString();
}

void ConfigModel::save(const std::string& path) const {
    hp_json::Value v(hp_json::Value::Type::Object);
    v["fuzzyOn"] = hp_json::Value(fuzzyOn_);
    v["fuzzyPairs"] = hp_json::Value(hp_json::Value::Type::Array);
    for (const auto& p : fuzzyPairs_) {
        v["fuzzyPairs"].arr.push_back(hp_json::Value(p));
    }
    hp_json::Value hk(hp_json::Value::Type::Object);
    hk["modifiers"] = hp_json::Value(hotkey_.modifiers);
    hk["vk"] = hp_json::Value(hotkey_.vk);
    v["hotkey"] = hk;
    v["mainDictPath"] = hp_json::Value(mainDictPath_);
    v["phraseDictPath"] = hp_json::Value(phraseDictPath_);
    v["userDictPath"] = hp_json::Value(userDictPath_);

    writeFile(path, hp_json::stringify(v, 2));
}

}  // namespace hanpinyin
