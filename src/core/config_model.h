// HanPinyin · 配置数据模型
// 承载：模糊音开关 / 模糊集 / 热键 / 词库路径。可序列化到 config.json。
// 本文件属于 core，严禁 #include <windows.h>；热键以普通整型表示（不引 VK_* 宏）。

#pragma once

#include <string>
#include <vector>

namespace hanpinyin {

// 热键修饰位（与 Windows MOD_* 对应，但以独立常量表示，避免依赖 windows.h）
const int kModNone = 0;
const int kModCtrl = 1;
const int kModAlt = 2;
const int kModShift = 4;

// VK 默认值（与 WinUser.h 一致，纯整型常量）
const int kVkSpace = 0x20;  // VK_SPACE
const int kVkOem3 = 0xC0;   // VK_OEM_3 (反引号 `，默认热键主键)
const int kVkCapital = 0x14;

struct Hotkey {
    int modifiers = kModCtrl;  // 默认 Ctrl
    int vk = kVkOem3;          // 默认 ` (反引号，避免与系统 Ctrl+Space 冲突)
};

class ConfigModel {
public:
    ConfigModel();

    // 从 JSON 文件加载；文件不存在时使用默认值。
    void load(const std::string& path);
    // 写入 JSON 文件。
    void save(const std::string& path) const;

    bool isFuzzyOn() const { return fuzzyOn_; }
    void setFuzzyOn(bool on) { fuzzyOn_ = on; }

    const std::vector<std::string>& getFuzzyPairs() const { return fuzzyPairs_; }
    void setFuzzyPairs(const std::vector<std::string>& p) { fuzzyPairs_ = p; }

    Hotkey getHotkey() const { return hotkey_; }
    void setHotkey(const Hotkey& h) { hotkey_ = h; }

    const std::string& getMainDictPath() const { return mainDictPath_; }
    const std::string& getPhraseDictPath() const { return phraseDictPath_; }
    const std::string& getUserDictPath() const { return userDictPath_; }
    void setMainDictPath(const std::string& p) { mainDictPath_ = p; }
    void setPhraseDictPath(const std::string& p) { phraseDictPath_ = p; }
    void setUserDictPath(const std::string& p) { userDictPath_ = p; }

private:
    bool fuzzyOn_ = true;                       // 模糊音默认开启（D3）
    std::vector<std::string> fuzzyPairs_;       // 如 ["zh=z","ch=c",...]，空则使用内置默认
    Hotkey hotkey_;                             // 默认 Ctrl+`
    std::string mainDictPath_ = "data/main_dict.json";
    std::string phraseDictPath_ = "data/phrases.json";
    std::string userDictPath_ = "data/user_dict.json";
};

}  // namespace hanpinyin
