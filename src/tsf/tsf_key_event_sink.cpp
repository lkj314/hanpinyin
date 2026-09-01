// HanPinyin · ITfKeyEventSink 实现（收键 + 拼音累积 / 热键路由）
// SDK 26100：ITfKeyEventSink 方法首参为 ITfContext* pic（取代旧版 TfEditCookie ec）。

#include "tsf_text_service.h"
#include <windows.h>
#include <msctf.h>

namespace hanpinyin {
namespace tsf {

// 决定是否吃掉按键：仅当 已激活 && 开启 && 中文模式 且 属于本输入法处理的键
STDMETHODIMP CTextService::OnSetFocus(BOOL /*fForeground*/) {
    // 焦点切换无需特殊处理；组合态由 ThreadMgrEventSink 维护。
    return S_OK;
}

STDMETHODIMP CTextService::OnTestKeyDown(ITfContext* /*pic*/, WPARAM wParam,
                                         LPARAM lParam, BOOL* pfEaten) {
    (void)lParam;
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    if (!m_bActivated || !m_bEnabled || m_bEnglishMode) return S_OK;

    const int vk = static_cast<int>(wParam);
    if (vk == VK_OEM_3 && (GetKeyState(VK_CONTROL) & 0x8000)) {  // Ctrl+`
        *pfEaten = TRUE;
    } else if (vk >= '1' && vk <= '9') {                         // 选词
        *pfEaten = TRUE;
    } else if (vk == VK_ESCAPE || vk == VK_TAB || vk == VK_BACK) {
        *pfEaten = TRUE;
    } else if (vk >= 'a' && vk <= 'z') {                         // 拼音字母
        *pfEaten = TRUE;
    }
    return S_OK;
}

STDMETHODIMP CTextService::OnKeyDown(ITfContext* /*pic*/, WPARAM wParam,
                                     LPARAM lParam, BOOL* pfEaten) {
    (void)lParam;
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    if (!m_bActivated || !m_bEnabled || m_bEnglishMode) return S_OK;

    const int vk = static_cast<int>(wParam);

    // Ctrl+` 切换 开/关
    if (vk == VK_OEM_3 && (GetKeyState(VK_CONTROL) & 0x8000)) {
        SetEnabled(m_bEnabled ? FALSE : TRUE);
        *pfEaten = TRUE;
        return S_OK;
    }

    // 数字 1-9 选词
    if (vk >= '1' && vk <= '9') {
        _SelectCandidate(vk - '0');
        *pfEaten = TRUE;
        return S_OK;
    }

    // Esc 取消组合
    if (vk == VK_ESCAPE) {
        ITfContext* ctx = _GetFocusContext();
        if (ctx) { _EndComposition(ctx); ctx->Release(); }
        *pfEaten = TRUE;
        return S_OK;
    }

    // Tab 翻页（Shift+Tab 上一页）
    if (vk == VK_TAB) {
        if (GetKeyState(VK_SHIFT) & 0x8000) _PrevPage();
        else _NextPage();
        *pfEaten = TRUE;
        return S_OK;
    }

    // Backspace 删除
    if (vk == VK_BACK) {
        _Backspace();
        *pfEaten = TRUE;
        return S_OK;
    }

    // 拼音字母 / 音节分隔符
    if ((vk >= 'a' && vk <= 'z') || vk == VK_OEM_3 /* 反引号不入拼音 */) {
        if (vk >= 'a' && vk <= 'z') _HandlePinyinKey(static_cast<WCHAR>(vk));
        *pfEaten = TRUE;
        return S_OK;
    }

    return S_OK;
}

STDMETHODIMP CTextService::OnKeyUp(ITfContext* /*pic*/, WPARAM wParam,
                                   LPARAM lParam, BOOL* pfEaten) {
    (void)wParam;
    (void)lParam;
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnTestKeyUp(ITfContext* /*pic*/, WPARAM wParam,
                                       LPARAM lParam, BOOL* pfEaten) {
    (void)wParam;
    (void)lParam;
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnPreservedKey(ITfContext* /*pic*/, REFGUID rguid,
                                          BOOL* pfEaten) {
    (void)rguid;
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;  // 本输入法不定义 preserved key（热键在 OnKeyDown 处理）
    return S_OK;
}

}  // namespace tsf
}  // namespace hanpinyin
