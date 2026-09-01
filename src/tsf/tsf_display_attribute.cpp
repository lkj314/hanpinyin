// HanPinyin · 显示属性实现（组合串高亮）

#include "tsf_display_attribute.h"
#include "tsf_guid.h"
#include <new>

namespace hanpinyin {
namespace tsf {

// ---------------- CDisplayAttributeInfo ----------------
STDMETHODIMP CDisplayAttributeInfo::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == IID_ITfDisplayAttributeInfo) {
        *ppv = static_cast<ITfDisplayAttributeInfo*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CDisplayAttributeInfo::AddRef() {
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CDisplayAttributeInfo::Release() {
    ULONG cr = InterlockedDecrement(&m_cRef);
    if (cr == 0) delete this;
    return cr;
}

STDMETHODIMP CDisplayAttributeInfo::GetGUID(GUID* pguid) {
    if (!pguid) return E_INVALIDARG;
    *pguid = GUID_HanPinyinDisplayAttribute;
    return S_OK;
}

STDMETHODIMP CDisplayAttributeInfo::GetDescription(BSTR* pbstrDesc) {
    if (!pbstrDesc) return E_INVALIDARG;
    *pbstrDesc = SysAllocString(L"HanPinyin Composition");
    return S_OK;
}

STDMETHODIMP CDisplayAttributeInfo::GetAttributeInfo(TF_DISPLAYATTRIBUTE* pda) {
    if (!pda) return E_INVALIDARG;
    // 浅蓝底 + 蓝色虚线，文字沿用默认色，使拼音组合串在目标应用内高亮
    pda->crText.type = TF_CT_NONE;                       // 默认文本色
    pda->crBk.type = TF_CT_COLORREF;
    pda->crBk.cr = RGB(200, 228, 255);
    pda->lsStyle = TF_LS_DASH;
    pda->fBoldLine = TRUE;
    pda->crLine.type = TF_CT_COLORREF;
    pda->crLine.cr = RGB(30, 120, 220);
    pda->bAttr = TF_ATTR_INPUT;
    return S_OK;
}

STDMETHODIMP CDisplayAttributeInfo::SetAttributeInfo(
    const TF_DISPLAYATTRIBUTE* /*pda*/) {
    return S_OK;  // 只读，不支持修改
}

STDMETHODIMP CDisplayAttributeInfo::Reset() {
    return S_OK;
}

// ---------------- CEnumDisplayAttributeInfo ----------------
STDMETHODIMP CEnumDisplayAttributeInfo::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == IID_IEnumTfDisplayAttributeInfo) {
        *ppv = static_cast<IEnumTfDisplayAttributeInfo*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CEnumDisplayAttributeInfo::AddRef() {
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CEnumDisplayAttributeInfo::Release() {
    ULONG cr = InterlockedDecrement(&m_cRef);
    if (cr == 0) delete this;
    return cr;
}

STDMETHODIMP CEnumDisplayAttributeInfo::Clone(
    IEnumTfDisplayAttributeInfo** ppEnum) {
    if (!ppEnum) return E_INVALIDARG;
    *ppEnum = new (std::nothrow) CEnumDisplayAttributeInfo(m_pInfo);
    if (!*ppEnum) return E_OUTOFMEMORY;
    (*ppEnum)->AddRef();
    return S_OK;
}

STDMETHODIMP CEnumDisplayAttributeInfo::Next(ULONG uCount,
                                             ITfDisplayAttributeInfo** ppInfo,
                                             ULONG* puFetched) {
    if (!ppInfo) return E_INVALIDARG;
    ULONG fetched = 0;
    for (ULONG i = 0; i < uCount; ++i) {
        if (m_index >= 1) break;  // 仅一个元素
        ppInfo[i] = m_pInfo;
        m_pInfo->AddRef();
        ++m_index;
        ++fetched;
    }
    if (puFetched) *puFetched = fetched;
    return (fetched == uCount) ? S_OK : S_FALSE;
}

STDMETHODIMP CEnumDisplayAttributeInfo::Reset() {
    m_index = 0;
    return S_OK;
}

STDMETHODIMP CEnumDisplayAttributeInfo::Skip(ULONG uCount) {
    m_index += uCount;
    if (m_index > 1) m_index = 1;
    return (uCount > 0) ? S_OK : S_FALSE;
}

}  // namespace tsf
}  // namespace hanpinyin
