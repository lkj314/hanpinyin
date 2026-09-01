// HanPinyin · 配置面板实现

#include "config_panel.h"
#include "../core/config_model.h"
#include "../core/types.h"
#include <cwctype>  // towlower

namespace hanpinyin {

ConfigPanel::ConfigPanel() = default;

ConfigPanel::~ConfigPanel() {
    if (hwnd_) DestroyWindow(hwnd_);
}

void ConfigPanel::open() {
    if (!hwnd_) {
        const wchar_t* cls = L"HanPinyinPanelCls";
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &ConfigPanel::wndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = cls;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(IDC_ARROW));
        RegisterClassExW(&wc);

        hwnd_ = CreateWindowExW(
            WS_EX_TOPMOST, cls, L"HanPinyin 配置",
            WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT, 360, 260,
            nullptr, nullptr, GetModuleHandleW(nullptr), this);
        initControls();
    }
    ShowWindow(hwnd_, SW_SHOW);
    BringWindowToTop(hwnd_);
}

void ConfigPanel::initControls() {
    HINSTANCE hinst = GetModuleHandleW(nullptr);

    CreateWindowW(L"STATIC", L"拼音:", WS_CHILD | WS_VISIBLE, 12, 14, 50, 20,
                  hwnd_, nullptr, hinst, nullptr);
    hPinyin_ = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                             70, 12, 160, 22, hwnd_, nullptr, hinst, nullptr);

    CreateWindowW(L"STATIC", L"韩文:", WS_CHILD | WS_VISIBLE, 12, 44, 50, 20,
                  hwnd_, nullptr, hinst, nullptr);
    hKorean_ = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                             70, 42, 160, 22, hwnd_, nullptr, hinst, nullptr);

    CreateWindowW(L"BUTTON", L"添加词条", WS_CHILD | WS_VISIBLE, 240, 12, 90, 24,
                  hwnd_, reinterpret_cast<HMENU>(1), hinst, nullptr);
    CreateWindowW(L"BUTTON", L"删除词条", WS_CHILD | WS_VISIBLE, 240, 42, 90, 24,
                  hwnd_, reinterpret_cast<HMENU>(2), hinst, nullptr);

    CreateWindowW(L"BUTTON", L"模糊音", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                  12, 80, 80, 22, hwnd_, reinterpret_cast<HMENU>(3), hinst, nullptr);
    hFuzzy_ = GetDlgItem(hwnd_, 3);
    if (cfg_) {
        SendMessageW(hFuzzy_, BM_SETCHECK,
                     cfg_->isFuzzyOn() ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    CreateWindowW(L"STATIC", L"热键(如 Ctrl+`):", WS_CHILD | WS_VISIBLE,
                  12, 112, 160, 20, hwnd_, nullptr, hinst, nullptr);
    hHotkey_ = CreateWindowW(L"EDIT", L"Ctrl+`",
                             WS_CHILD | WS_VISIBLE | WS_BORDER, 12, 134, 160, 22,
                             hwnd_, nullptr, hinst, nullptr);

    CreateWindowW(L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE, 200, 132, 70, 26,
                  hwnd_, reinterpret_cast<HMENU>(4), hinst, nullptr);
    CreateWindowW(L"BUTTON", L"关闭", WS_CHILD | WS_VISIBLE, 280, 132, 60, 26,
                  hwnd_, reinterpret_cast<HMENU>(5), hinst, nullptr);

    CreateWindowW(L"STATIC", L"提示：热键实时生效；词条增删即时重载词库。",
                  WS_CHILD | WS_VISIBLE, 12, 170, 320, 40, hwnd_, nullptr, hinst, nullptr);
}

void ConfigPanel::parseHotkey(const std::wstring& text, int& mod, int& vk) const {
    mod = 0;
    vk = 0xC0;  // 默认 ` (VK_OEM_3)
    std::wstring lower = text;
    for (auto& c : lower) c = towlower(c);
    if (lower.find(L"ctrl") != std::wstring::npos) mod |= kModCtrl;
    if (lower.find(L"alt") != std::wstring::npos) mod |= kModAlt;
    if (lower.find(L"shift") != std::wstring::npos) mod |= kModShift;

    // 取最后一个 '+' 之后的键名
    size_t pos = lower.rfind(L'+');
    std::wstring key = (pos == std::wstring::npos) ? lower : lower.substr(pos + 1);
    // 去掉空白
    size_t a = key.find_first_not_of(L" \t");
    size_t b = key.find_last_not_of(L" \t");
    if (a != std::wstring::npos) key = key.substr(a, b - a + 1);

    if (key == L"space") vk = 0x20;
    else if (key == L"`" || key == L"backtick" || key == L"grave")
        vk = 0xC0;  // VK_OEM_3 (反引号)
    else if (key.size() == 1 && key[0] >= L'a' && key[0] <= L'z')
        vk = static_cast<int>(key[0] - L'a' + 'A');
    else if (key == L"enter") vk = 0x0D;
    else if (key == L"tab") vk = 0x09;
    else vk = 0xC0;  // 默认反引号
}

LRESULT CALLBACK ConfigPanel::wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    ConfigPanel* self = nullptr;
    if (m == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(l);
        self = reinterpret_cast<ConfigPanel*>(cs->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<ConfigPanel*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    }
    if (!self) return DefWindowProcW(h, m, w, l);

    switch (m) {
        case WM_COMMAND: {
            int id = LOWORD(w);
            if (id == 1 && self->addCb_) {  // 添加
                wchar_t buf[256] = {};
                GetWindowTextW(self->hPinyin_, buf, 256);
                std::wstring py = buf;
                GetWindowTextW(self->hKorean_, buf, 256);
                std::wstring kor = buf;
                // 清理拼音为小写无空格
                std::string pinyin;
                for (wchar_t c : py) {
                    if (c >= L'A' && c <= L'Z') pinyin.push_back(char(c - L'A' + 'a'));
                    else if ((c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9'))
                        pinyin.push_back(char(c));
                    else if (c == L' ') pinyin.push_back(' ');
                }
                if (!pinyin.empty() && !kor.empty()) {
                    self->addCb_(pinyin, kor);
                }
            } else if (id == 2 && self->removeCb_) {  // 删除
                wchar_t buf[256] = {};
                GetWindowTextW(self->hPinyin_, buf, 256);
                std::wstring py = buf;
                std::string pinyin;
                for (wchar_t c : py) {
                    if (c >= L'A' && c <= L'Z') pinyin.push_back(char(c - L'A' + 'a'));
                    else if ((c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9'))
                        pinyin.push_back(char(c));
                    else if (c == L' ') pinyin.push_back(' ');
                }
                if (!pinyin.empty()) self->removeCb_(pinyin);
            } else if (id == 4 && self->saveCb_) {  // 保存
                bool fuzzy = (SendMessageW(self->hFuzzy_, BM_GETCHECK, 0, 0) == BST_CHECKED);
                wchar_t buf[256] = {};
                GetWindowTextW(self->hHotkey_, buf, 256);
                int mod = 0, vk = 0xC0;
                self->parseHotkey(buf, mod, vk);
                self->saveCb_(fuzzy, mod, vk);
            } else if (id == 5) {  // 关闭
                ShowWindow(h, SW_HIDE);
            }
            return 0;
        }
        case WM_CLOSE:
            ShowWindow(h, SW_HIDE);
            return 0;
        case WM_DESTROY:
            return 0;
        default:
            return DefWindowProcW(h, m, w, l);
    }
}

}  // namespace hanpinyin
