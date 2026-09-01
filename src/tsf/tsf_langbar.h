// HanPinyin · 语言栏按钮（ITfLangBarItemButton）
// 左键点击切换 开/关；显示 "한"(开启) / "A"(关闭或英文模式)。状态变化经
// ITfLangBarItemSink 通知语言栏刷新。

#pragma once

#include <windows.h>
#include <msctf.h>

namespace hanpinyin {
namespace tsf {

class CTextService;  // 前置声明

class CLangBarButton : public ITfLangBarItemButton,
                       public ITfSource {
public:
    CLangBarButton(CTextService* pService);
    virtual ~CLangBarButton();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfLangBarItem
    STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* pInfo) override;
    STDMETHODIMP GetStatus(DWORD* pdwStatus) override;
    STDMETHODIMP Show(BOOL fShow) override;
    STDMETHODIMP GetTooltipString(BSTR* pbstrToolTip) override;

    // ITfLangBarItemButton
    STDMETHODIMP OnClick(TfLBIClick click, POINT pt, const RECT* prc) override;
    STDMETHODIMP InitMenu(ITfMenu* pMenu) override;
    STDMETHODIMP OnMenuSelect(UINT wID) override;
    STDMETHODIMP GetIcon(HICON* phIcon) override;
    STDMETHODIMP GetText(BSTR* pbstrText) override;

    // ITfSource
    STDMETHODIMP AdviseSink(REFIID riid, IUnknown* punk, DWORD* pdwCookie) override;
    STDMETHODIMP UnadviseSink(DWORD dwCookie) override;

    // 刷新语言栏项（状态/文本/提示变化时由 CTextService 调用）
    void Update();

private:
    ULONG m_cRef;
    CTextService* m_pService;
    ITfLangBarItemSink* m_pSink;  // 语言栏管理器的通知接收器（Advise 时获得）
    DWORD m_dwCookie;
};

}  // namespace tsf
}  // namespace hanpinyin
