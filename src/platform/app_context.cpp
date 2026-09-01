// HanPinyin · 设置 / 注册 exe 宿主实现

#include "app_context.h"
#include "../platform/logger.h"
#include <windows.h>
#include <shellapi.h>

namespace hanpinyin {

ConfigApp::ConfigApp() = default;

ConfigApp::~ConfigApp() = default;

int ConfigApp::run() {
    // 单实例：防止多开
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"HanPinyinConfigSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        MessageBoxW(nullptr, L"HanPinyin 设置程序已在运行。", L"HanPinyin",
                    MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // 创建隐藏宿主窗口（承载托盘图标 + 消息循环）
    const wchar_t* cls = L"HanPinyinConfigHostCls";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &ConfigApp::wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = cls;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(IDC_ARROW));
    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(0, cls, L"HanPinyin Config",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             0, 0, nullptr, nullptr, GetModuleHandleW(nullptr),
                             this);
    if (!m_hWnd) {
        if (hMutex) CloseHandle(hMutex);
        return -1;
    }

    // 加载配置（缺失则用默认）
    m_config.load("config.json");

    // 配置面板：写入 HKCU
    m_panel.setConfigModel(&m_config);
    m_panel.onSaveConfig([this](bool fuzzy, int mod, int vk) {
        this->writeConfig(fuzzy, mod, vk);
    });

    initTray();

    // 主消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    removeTray();
    if (hMutex) CloseHandle(hMutex);
    return 0;
}

void ConfigApp::initTray() {
    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = m_hWnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = kTrayMsg;
    nid_.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(IDI_INFORMATION));
    wcscpy_s(nid_.szTip, 128, L"HanPinyin · 设置 / 注册");
    Shell_NotifyIconW(NIM_ADD, &nid_);
}

void ConfigApp::removeTray() {
    Shell_NotifyIconW(NIM_DELETE, &nid_);
}

void ConfigApp::showTrayMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kMenuConfig, L"打开配置");
    AppendMenuW(menu, MF_STRING, kMenuRegister, L"注册输入法");
    AppendMenuW(menu, MF_STRING, kMenuUnregister, L"卸载输入法");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"退出");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hWnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, pt.x, pt.y, 0,
                   m_hWnd, nullptr);
    PostMessageW(m_hWnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

void ConfigApp::onTrayIcon(WPARAM, LPARAM l) {
    switch (l) {
        case WM_RBUTTONUP: showTrayMenu(); break;
        case WM_LBUTTONDBLCLK: m_panel.open(); break;
        default: break;
    }
}

void ConfigApp::doRegister() {
    wchar_t exePath[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir = exePath;
    auto pos = dir.find_last_of(L'\\');
    if (pos != std::wstring::npos) dir = dir.substr(0, pos + 1);
    std::wstring dll = dir + L"hanpinyin_tsf.dll";

    std::wstring params = L"/s \"";
    params += dll;
    params += L"\"";

    HINSTANCE h = ShellExecuteW(nullptr, L"runas", L"regsvr32.exe",
                                params.c_str(), nullptr, SW_HIDE);
    if (reinterpret_cast<INT_PTR>(h) > 32) {
        MessageBoxW(m_hWnd, L"注册成功。请在「语言设置 → 键盘」中启用 HanPinyin。",
                    L"HanPinyin", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(m_hWnd, L"注册失败。请右键以管理员身份运行本程序。",
                    L"HanPinyin", MB_OK | MB_ICONERROR);
    }
}

void ConfigApp::doUnregister() {
    wchar_t exePath[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir = exePath;
    auto pos = dir.find_last_of(L'\\');
    if (pos != std::wstring::npos) dir = dir.substr(0, pos + 1);
    std::wstring dll = dir + L"hanpinyin_tsf.dll";

    std::wstring params = L"/s /u \"";
    params += dll;
    params += L"\"";

    HINSTANCE h = ShellExecuteW(nullptr, L"runas", L"regsvr32.exe",
                                params.c_str(), nullptr, SW_HIDE);
    if (reinterpret_cast<INT_PTR>(h) > 32) {
        MessageBoxW(m_hWnd, L"卸载成功。", L"HanPinyin",
                    MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(m_hWnd, L"卸载失败。请右键以管理员身份运行本程序。",
                    L"HanPinyin", MB_OK | MB_ICONERROR);
    }
}

void ConfigApp::writeConfig(bool fuzzyOn, int mod, int vk) {
    m_config.setFuzzyOn(fuzzyOn);
    Hotkey hk;
    hk.modifiers = mod;
    hk.vk = vk;
    m_config.setHotkey(hk);
    m_config.save("config.json");

    HKEY hkKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\HanPinyin", 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hkKey,
                        nullptr) == ERROR_SUCCESS) {
        DWORD v = fuzzyOn ? 1 : 0;
        RegSetValueExW(hkKey, L"FuzzyOn", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&v), sizeof(DWORD));
        DWORD vm = static_cast<DWORD>(mod);
        RegSetValueExW(hkKey, L"HotkeyMod", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&vm), sizeof(DWORD));
        DWORD vv = static_cast<DWORD>(vk);
        RegSetValueExW(hkKey, L"HotkeyVk", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&vv), sizeof(DWORD));
        DWORD en = 1;
        RegSetValueExW(hkKey, L"Enabled", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&en), sizeof(DWORD));
        RegCloseKey(hkKey);
    }
}

LRESULT CALLBACK ConfigApp::wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    ConfigApp* self = nullptr;
    if (m == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(l);
        self = reinterpret_cast<ConfigApp*>(cs->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<ConfigApp*>(
            GetWindowLongPtrW(h, GWLP_USERDATA));
    }
    if (!self) return DefWindowProcW(h, m, w, l);

    switch (m) {
        case WM_COMMAND: {
            int id = LOWORD(w);
            if (id == kMenuConfig) self->m_panel.open();
            else if (id == kMenuRegister) self->doRegister();
            else if (id == kMenuUnregister) self->doUnregister();
            else if (id == kMenuExit) PostQuitMessage(0);
            return 0;
        }
        case kTrayMsg: {
            self->onTrayIcon(w, l);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(h, m, w, l);
    }
}

}  // namespace hanpinyin
