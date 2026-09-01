// HanPinyin · 显示属性（组合串高亮）
// CDisplayAttributeInfo：ITfDisplayAttributeInfo，描述高亮样式（浅蓝底 + 虚线）。
// CEnumDisplayAttributeInfo：枚举器，仅含本输入法的显示属性。

#pragma once

#include <windows.h>
#include <msctf.h>

namespace hanpinyin {
namespace tsf {

// 单个显示属性信息
class CDisplayAttributeInfo : public ITfDisplayAttributeInfo {
public:
    CDisplayAttributeInfo() : m_cRef(1) {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfDisplayAttributeInfo
    STDMETHODIMP GetGUID(GUID* pguid) override;
    STDMETHODIMP GetDescription(BSTR* pbstrDesc) override;
    STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* pda) override;
    STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE* pda) override;
    STDMETHODIMP Reset() override;

private:
    ULONG m_cRef;
};

// 枚举器：仅含一个显示属性项
class CEnumDisplayAttributeInfo : public IEnumTfDisplayAttributeInfo {
public:
    CEnumDisplayAttributeInfo(CDisplayAttributeInfo* pInfo)
        : m_cRef(1), m_index(0) {
        m_pInfo = pInfo;
        if (m_pInfo) m_pInfo->AddRef();
    }
    virtual ~CEnumDisplayAttributeInfo() {
        if (m_pInfo) m_pInfo->Release();
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IEnumTfDisplayAttributeInfo
    STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo** ppEnum) override;
    STDMETHODIMP Next(ULONG uCount, ITfDisplayAttributeInfo** ppInfo,
                      ULONG* puFetched) override;
    STDMETHODIMP Reset() override;
    STDMETHODIMP Skip(ULONG uCount) override;

private:
    ULONG m_cRef;
    CDisplayAttributeInfo* m_pInfo;
    ULONG m_index;
};

}  // namespace tsf
}  // namespace hanpinyin
