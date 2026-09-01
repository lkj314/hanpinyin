// HanPinyin · 设置 / 注册 exe 宿主（ConfigApp）
// 承载：系统托盘 + 消息循环 + 自注册/卸载（调 regsvr32）+ 配置面板。
// 由 hanpinyin_config.exe 入口（src/app/main.cpp）驱动。

#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>
#include "config_panel.h"
#include "../core/config_model.h"

namespace hanpinyin {

class ConfigApp {
public:
    ConfigApp();
    ~ConfigApp();

    // 进入主循环；返回进程退出码
    int run();

private:
    void initTray();
    void removeTray();
    void showTrayMenu();
    void onTrayIcon(WPARAM w, LPARAM l);

    // 自注册 / 卸载（以 runas 启动 regsvr32）
    void doRegister();
    void doUnregister();

    // 将配置写入 HKCU\Software\HanPinyin
    void writeConfig(bool fuzzyOn, int mod, int vk);

    static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l);

    HWND m_hWnd = nullptr;
    NOTIFYICONDATAW nid_ = {};
    ConfigPanel m_panel;
    ConfigModel m_config;

    static constexpr UINT kTrayMsg = WM_USER + 1;
    static constexpr UINT kMenuConfig = 1001;
    static constexpr UINT kMenuRegister = 1002;
    static constexpr UINT kMenuUnregister = 1003;
    static constexpr UINT kMenuExit = 1004;
};

}  // namespace hanpinyin
