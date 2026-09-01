// HanPinyin · 语言栏按钮实现

#include "tsf_langbar.h"
#include "tsf_text_service.h"
#include "tsf_guid.h"
#include <windows.h>
#include <msctf.h>

namespace hanpinyin {
namespace tsf {

CLangBarButton::CLangBarButton(CTextService* pService)
    : m_cRef(1), m_pService(pService), m_pSink(nullptr), m_dwCookie(0) {}

CLangBarButton::~CLangBarButton() {
    if (m_pSink) {
        m_pSink->Release();
        m_pSink = nullptr;
    }
}

STDMETHODIMP CLangBarButton::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;
    if (riid == IID_IUnknown) {
        *ppv = static_cast<ITfLangBarItemButton*>(this);
    } else if (riid == IID_ITfLangBarItem) {
        *ppv = static_cast<ITfLangBarItem*>(this);
    } else if (riid == IID_ITfLangBarItemButton) {
        *ppv = static_cast<ITfLangBarItemButton*>(this);
    } else if (riid == IID_ITfSource) {
        *ppv = static_cast<ITfSource*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CLangBarButton::AddRef() {
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CLangBarButton::Release() {
    ULONG cr = InterlockedDecrement(&m_cRef);
    if (cr == 0) delete this;
    return cr;
}

STDMETHODIMP CLangBarButton::GetInfo(TF_LANGBARITEMINFO* pInfo) {
    if (!pInfo) return E_INVALIDARG;
    pInfo->clsidService = CLSID_HanPinyinTextService;
    pInfo->guidItem = GUID_HanPinyinLangBarItem;
    pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY;
    pInfo->ulSort = 1;
    wcscpy_s(pInfo->szDescription, TF_LBI_DESC_MAXLEN, L"HanPinyin");
    return S_OK;
}

STDMETHODIMP CLangBarButton::GetStatus(DWORD* pdwStatus) {
    if (!pdwStatus) return E_INVALIDARG;
    *pdwStatus = 0;  // TF_LBI_STATUS_DEFAULT
    return S_OK;
}

STDMETHODIMP CLangBarButton::Show(BOOL /*fShow*/) {
    return S_OK;
}

STDMETHODIMP CLangBarButton::GetTooltipString(BSTR* pbstrToolTip) {
    if (!pbstrToolTip) return E_INVALIDARG;
    *pbstrToolTip = SysAllocString(L"HanPinyin 拼音输入 (Ctrl+` 开关)");
    return S_OK;
}

STDMETHODIMP CLangBarButton::OnClick(TfLBIClick click, POINT /*pt*/,
                                     const RECT* /*prc*/) {
    if (click == TF_LBI_CLK_LEFT && m_pService) {
        // 左键：切换 开/关
        m_pService->SetEnabled(m_pService->IsEnabled() ? FALSE : TRUE);
        Update();
    }
    return S_OK;
}

STDMETHODIMP CLangBarButton::InitMenu(ITfMenu* /*pMenu*/) {
    return S_OK;
}

STDMETHODIMP CLangBarButton::OnMenuSelect(UINT /*wID*/) {
    return S_OK;
}

STDMETHODIMP CLangBarButton::GetIcon(HICON* phIcon) {
    if (!phIcon) return E_INVALIDARG;
    *phIcon = nullptr;  // 使用文字标签，不提供图标
    return S_OK;
}

STDMETHODIMP CLangBarButton::GetText(BSTR* pbstrText) {
    if (!pbstrText) return E_INVALIDARG;
    std::wstring label =
        (m_pService && m_pService->IsEnabled() && !m_pService->IsEnglishMode())
            ? L"한"
            : L"A";
    *pbstrText = SysAllocString(label.c_str());
    return S_OK;
}

STDMETHODIMP CLangBarButton::AdviseSink(REFIID riid, IUnknown* punk,
                                        DWORD* pdwCookie) {
    if (!pdwCookie) return E_INVALIDARG;
    *pdwCookie = 0;
    if (riid != IID_ITfLangBarItemSink || !punk) return E_NOINTERFACE;
    m_pSink = static_cast<ITfLangBarItemSink*>(punk);
    m_pSink->AddRef();
    m_dwCookie = 1;
    *pdwCookie = 1;
    return S_OK;
}

STDMETHODIMP CLangBarButton::UnadviseSink(DWORD dwCookie) {
    if (dwCookie == 1 && m_pSink) {
        m_pSink->Release();
        m_pSink = nullptr;
        m_dwCookie = 0;
    }
    return S_OK;
}

void CLangBarButton::Update() {
    if (m_pSink) {
        m_pSink->OnUpdate(TF_LBI_STATUS | TF_LBI_TEXT | TF_LBI_TOOLTIP);
    }
}

}  // namespace tsf
}  // namespace hanpinyin
