// HanPinyin · 图形配置面板（Win32 对话框）
// 功能：增删词条 / 模糊音开关 / 热键自定义；写回 ConfigModel + JSON，并触发 onConfigChanged。
// 以「无模式（modeless）」窗口实现，消息由 AppContext 主循环统一分发。

#pragma once

#include <windows.h>
#include <string>
#include <functional>

namespace hanpinyin {

class ConfigModel;  // 前置声明

class ConfigPanel {
public:
    // 增删词条（拼音 + 韩文）
    using AddEntryCallback = std::function<void(const std::string&, const std::wstring&)>;
    using RemoveEntryCallback = std::function<void(const std::string&)>;
    // 保存配置：模糊音开关 / 热键修饰 / 热键主键
    using SaveConfigCallback = std::function<void(bool, int, int)>;

    ConfigPanel();
    ~ConfigPanel();

    void setConfigModel(ConfigModel* cfg) { cfg_ = cfg; }
    void onAddEntry(AddEntryCallback cb) { addCb_ = cb; }
    void onRemoveEntry(RemoveEntryCallback cb) { removeCb_ = cb; }
    void onSaveConfig(SaveConfigCallback cb) { saveCb_ = cb; }

    // 打开（已创建则仅显示）
    void open();

private:
    static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l);
    void initControls();
    void parseHotkey(const std::wstring& text, int& mod, int& vk) const;

    ConfigModel* cfg_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND hPinyin_ = nullptr;
    HWND hKorean_ = nullptr;
    HWND hFuzzy_ = nullptr;
    HWND hHotkey_ = nullptr;

    AddEntryCallback addCb_;
    RemoveEntryCallback removeCb_;
    SaveConfigCallback saveCb_;
};

}  // namespace hanpinyin
