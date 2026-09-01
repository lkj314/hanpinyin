// HanPinyin · 编辑会话实现（SetText 上屏核心路径）

#include "tsf_edit_sessions.h"
#include "tsf_text_service.h"
#include <windows.h>
#include <msctf.h>

namespace hanpinyin {
namespace tsf {

// ---------------- 基类 ----------------
CEditSessionBase::CEditSessionBase(CTextService* pTextService,
                                   ITfContext* pContext)
    : m_pTextService(pTextService), m_cRef(1) {
    m_pContext = pContext;
    if (m_pContext) m_pContext->AddRef();
}

CEditSessionBase::~CEditSessionBase() {
    if (m_pContext) {
        m_pContext->Release();
        m_pContext = nullptr;
    }
}

STDMETHODIMP CEditSessionBase::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == IID_ITfEditSession) {
        *ppv = static_cast<ITfEditSession*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CEditSessionBase::AddRef() {
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CEditSessionBase::Release() {
    ULONG cr = InterlockedDecrement(&m_cRef);
    if (cr == 0) delete this;
    return cr;
}

// ---------------- CStartCompositionEditSession ----------------
CStartCompositionEditSession::CStartCompositionEditSession(
    CTextService* ts, ITfContext* ctx, const std::wstring& text,
    ITfComposition** ppComp)
    : CEditSessionBase(ts, ctx), m_text(text), m_ppComp(ppComp) {}

STDMETHODIMP CStartCompositionEditSession::DoEditSession(TfEditCookie ec) {
    ITfContextComposition* pCtxComp = nullptr;
    if (FAILED(m_pContext->QueryInterface(IID_ITfContextComposition,
                                           reinterpret_cast<void**>(&pCtxComp)))) {
        return E_FAIL;
    }

    TF_SELECTION sel = {};
    ULONG fetched = 0;
    if (FAILED(m_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel,
                                        &fetched)) ||
        fetched != 1) {
        pCtxComp->Release();
        return E_FAIL;
    }
    ITfRange* pRange = sel.range;  // GetSelection 已 AddRef

    // 取得 ITfCompositionSink（即 CTextService 自身，经多重继承调整指针）
    ITfCompositionSink* pSink = nullptr;
    m_pTextService->QueryInterface(IID_ITfCompositionSink,
                                   reinterpret_cast<void**>(&pSink));

    HRESULT hr = pCtxComp->StartComposition(ec, pRange, pSink, m_ppComp);
    if (pSink) pSink->Release();

    if (SUCCEEDED(hr) && m_ppComp && *m_ppComp) {
        // 官方通道写组合串（拼音 inline）
        pRange->SetText(ec, 0, m_text.c_str(), static_cast<LONG>(m_text.size()));
        m_pTextService->ApplyDisplayAttribute(m_pContext, ec, pRange);
    }

    pRange->Release();
    pCtxComp->Release();

    // 计算光标矩形并刷新候选窗（含显示属性高亮）
    m_pTextService->OnCompositionRendered(ec, m_pContext);
    return S_OK;
}

// ---------------- CUpdateCompositionEditSession ----------------
CUpdateCompositionEditSession::CUpdateCompositionEditSession(
    CTextService* ts, ITfContext* ctx, const std::wstring& text,
    ITfComposition* pComp)
    : CEditSessionBase(ts, ctx), m_text(text), m_pComp(pComp) {}

STDMETHODIMP CUpdateCompositionEditSession::DoEditSession(TfEditCookie ec) {
    if (!m_pComp) return S_OK;
    ITfRange* pRange = nullptr;
    if (FAILED(m_pComp->GetRange(&pRange)) || !pRange) return S_OK;

    pRange->SetText(ec, 0, m_text.c_str(), static_cast<LONG>(m_text.size()));
    m_pTextService->ApplyDisplayAttribute(m_pContext, ec, pRange);
    pRange->Release();

    m_pTextService->OnCompositionRendered(ec, m_pContext);
    return S_OK;
}

// ---------------- CCommitTextEditSession ----------------
CCommitTextEditSession::CCommitTextEditSession(CTextService* ts,
                                               ITfContext* ctx,
                                               const std::wstring& korean,
                                               ITfComposition* pComp)
    : CEditSessionBase(ts, ctx), m_text(korean), m_pComp(pComp) {}

STDMETHODIMP CCommitTextEditSession::DoEditSession(TfEditCookie ec) {
    if (!m_pComp) return S_OK;
    ITfRange* pRange = nullptr;
    if (SUCCEEDED(m_pComp->GetRange(&pRange)) && pRange) {
        // 官方通道把韩文写进目标文本存储（绝不用 SendInput）
        pRange->SetText(ec, 0, m_text.c_str(), static_cast<LONG>(m_text.size()));
        pRange->Release();
    }
    m_pComp->EndComposition(ec);

    // pinyin 已被 CTextService::_CommitText 清空，此处刷新会隐藏候选窗
    m_pTextService->OnCompositionRendered(ec, m_pContext);
    return S_OK;
}

// ---------------- CEndCompositionEditSession ----------------
CEndCompositionEditSession::CEndCompositionEditSession(CTextService* ts,
                                                       ITfContext* ctx,
                                                       ITfComposition* pComp)
    : CEditSessionBase(ts, ctx), m_pComp(pComp) {}

STDMETHODIMP CEndCompositionEditSession::DoEditSession(TfEditCookie ec) {
    if (m_pComp) {
        m_pComp->EndComposition(ec);
    }
    m_pTextService->OnCompositionRendered(ec, m_pContext);
    return S_OK;
}

}  // namespace tsf
}  // namespace hanpinyin
