// HanPinyin · CTextService 实现（生命周期 / QI / 各 TSF sink 连接 / 组合管理 / 配置）

#include "tsf_text_service.h"
#include "tsf_core_wrapper.h"
#include "tsf_candidate_ui.h"
#include "tsf_langbar.h"
#include "tsf_display_attribute.h"
#include "tsf_edit_sessions.h"
#include "tsf_guid.h"
#include "../platform/logger.h"
#include <new>

#include <windows.h>
#include <msctf.h>

namespace hanpinyin {
namespace tsf {

// 来自 tsf_module.cpp 的模块级状态
extern HINSTANCE g_hInstance;
extern void DllAddRef();
extern void DllRelease();

CTextService::CTextService() {
    m_cRef = 1;
    DllAddRef();
    m_pCore = new (std::nothrow) CCoreWrapper();
    m_pDAInfo = new (std::nothrow) CDisplayAttributeInfo();
}

CTextService::~CTextService() {
    if (m_pCandidateUI) { delete m_pCandidateUI; m_pCandidateUI = nullptr; }
    if (m_pCore) { delete m_pCore; m_pCore = nullptr; }
    if (m_pLangBar) { m_pLangBar->Release(); m_pLangBar = nullptr; }
    if (m_pDAInfo) { m_pDAInfo->Release(); m_pDAInfo = nullptr; }
    if (m_pComposition) { m_pComposition->Release(); m_pComposition = nullptr; }
    if (m_pFocusContext) { m_pFocusContext->Release(); m_pFocusContext = nullptr; }
    if (m_pThreadMgr) { m_pThreadMgr->Release(); m_pThreadMgr = nullptr; }
    DllRelease();
}

// ---------------- IUnknown ----------------
STDMETHODIMP CTextService::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;

    if (riid == IID_IUnknown) {
        *ppv = static_cast<ITfTextInputProcessorEx*>(this);
    } else if (riid == IID_ITfTextInputProcessor) {
        *ppv = static_cast<ITfTextInputProcessor*>(this);
    } else if (riid == IID_ITfTextInputProcessorEx) {
        *ppv = static_cast<ITfTextInputProcessorEx*>(this);
    } else if (riid == IID_ITfKeyEventSink) {
        *ppv = static_cast<ITfKeyEventSink*>(this);
    } else if (riid == IID_ITfThreadMgrEventSink) {
        *ppv = static_cast<ITfThreadMgrEventSink*>(this);
    } else if (riid == IID_ITfTextEditSink) {
        *ppv = static_cast<ITfTextEditSink*>(this);
    } else if (riid == IID_ITfDisplayAttributeProvider) {
        *ppv = static_cast<ITfDisplayAttributeProvider*>(this);
    } else if (riid == IID_ITfActiveLanguageProfileNotifySink) {
        *ppv = static_cast<ITfActiveLanguageProfileNotifySink*>(this);
    } else if (riid == IID_ITfCompositionSink) {
        *ppv = static_cast<ITfCompositionSink*>(this);
    } else {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CTextService::AddRef() {
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CTextService::Release() {
    ULONG cr = InterlockedDecrement(&m_cRef);
    DllRelease();
    if (cr == 0) delete this;
    return cr;
}

// ---------------- ITfTextInputProcessor(Ex) ----------------
STDMETHODIMP CTextService::Activate(ITfThreadMgr* ptim, TfClientId tid) {
    // SDK 26100：Activate 以传值方式接收客户端 ID，直接转给 ActivateEx。
    return ActivateEx(ptim, tid, 0);
}

STDMETHODIMP CTextService::ActivateEx(ITfThreadMgr* ptim, TfClientId tid,
                                      DWORD dwFlags) {
    (void)dwFlags;
    if (!ptim) return E_INVALIDARG;

    m_pThreadMgr = ptim;
    ptim->AddRef();
    m_clientId = tid;
    m_bActivated = TRUE;

    // 候选窗（自绘 popup，无焦点）
    if (!m_pCandidateUI) m_pCandidateUI = new (std::nothrow) CCandidateWindow();
    if (m_pCandidateUI) m_pCandidateUI->create();

    // 加载配置 + 词库
    _LoadConfig();

    // 连接各 sink
    _InitThreadMgrSink();
    _InitActiveLanguageProfileNotifySink();
    _InitLangBar();

    // 对当前焦点上下文连接 TextEditSink
    ITfContext* pCtx = _GetFocusContext();
    if (pCtx) {
        _InitTextEditSink(pCtx);
        pCtx->Release();
    }

    hp_log("[CTextService] ActivateEx ok");
    return S_OK;
}

STDMETHODIMP CTextService::Deactivate() {
    _UninitLangBar();
    _UninitActiveLanguageProfileNotifySink();
    _UninitTextEditSink();
    _UninitThreadMgrSink();

    if (m_pCandidateUI) {
        m_pCandidateUI->Hide();
        delete m_pCandidateUI;
        m_pCandidateUI = nullptr;
    }
    if (m_pComposition) {
        m_pComposition->Release();
        m_pComposition = nullptr;
    }
    m_wzPinyin.clear();
    m_iPage = 0;
    m_candidates = hanpinyin::CandidateList{};
    m_bActivated = FALSE;

    if (m_pThreadMgr) {
        m_pThreadMgr->Release();
        m_pThreadMgr = nullptr;
    }
    return S_OK;
}

// ---------------- sink 连接 ----------------
HRESULT CTextService::_InitThreadMgrSink() {
    if (!m_pThreadMgr) return E_FAIL;

    ITfSource* pSource = nullptr;
    if (SUCCEEDED(m_pThreadMgr->QueryInterface(IID_ITfSource,
                                               reinterpret_cast<void**>(&pSource)))) {
        pSource->AdviseSink(
            IID_ITfThreadMgrEventSink,
            static_cast<ITfThreadMgrEventSink*>(this),
            &m_dwThreadMgrSinkCookie);
        pSource->Release();
    }

    ITfKeystrokeMgr* pKeyMgr = nullptr;
    if (SUCCEEDED(m_pThreadMgr->QueryInterface(
            IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&pKeyMgr)))) {
        pKeyMgr->AdviseKeyEventSink(m_clientId,
                                    static_cast<ITfKeyEventSink*>(this), TRUE);
        pKeyMgr->Release();
    }
    return S_OK;
}

void CTextService::_UninitThreadMgrSink() {
    if (!m_pThreadMgr) return;
    if (m_dwThreadMgrSinkCookie) {
        ITfSource* pSource = nullptr;
        if (SUCCEEDED(m_pThreadMgr->QueryInterface(
                IID_ITfSource, reinterpret_cast<void**>(&pSource)))) {
            pSource->UnadviseSink(m_dwThreadMgrSinkCookie);
            pSource->Release();
        }
        m_dwThreadMgrSinkCookie = 0;
    }
    // KeystrokeMgr 无 cookie，直接 UnadviseKeyEventSink
    ITfKeystrokeMgr* pKeyMgr = nullptr;
    if (SUCCEEDED(m_pThreadMgr->QueryInterface(
            IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&pKeyMgr)))) {
        pKeyMgr->UnadviseKeyEventSink(m_clientId);
        pKeyMgr->Release();
    }
}

// ---------------- ITfThreadMgrEventSink ----------------
// 本实现的上下文获取走 _GetFocusContext()（m_pThreadMgr->GetFocus），
// 因此这些线程管理器事件回调无需维护文档/上下文状态，统一置为成功空实现。
STDMETHODIMP CTextService::OnInitDocumentMgr(ITfDocumentMgr* /*pdim*/) {
    return S_OK;
}

STDMETHODIMP CTextService::OnUninitDocumentMgr(ITfDocumentMgr* /*pdim*/) {
    return S_OK;
}

STDMETHODIMP CTextService::OnSetFocus(ITfDocumentMgr* /*pdimFocus*/,
                                      ITfDocumentMgr* /*pdimPrevFocus*/) {
    return S_OK;
}

STDMETHODIMP CTextService::OnPushContext(ITfContext* /*pic*/) {
    return S_OK;
}

STDMETHODIMP CTextService::OnPopContext(ITfContext* /*pic*/) {
    return S_OK;
}

HRESULT CTextService::_InitActiveLanguageProfileNotifySink() {
    if (!m_pThreadMgr) return E_FAIL;
    ITfSource* pSource = nullptr;
    if (SUCCEEDED(m_pThreadMgr->QueryInterface(IID_ITfSource,
                                               reinterpret_cast<void**>(&pSource)))) {
        pSource->AdviseSink(
            IID_ITfActiveLanguageProfileNotifySink,
            static_cast<ITfActiveLanguageProfileNotifySink*>(this),
            &m_dwActiveLangProfileCookie);
        pSource->Release();
    }
    return S_OK;
}

void CTextService::_UninitActiveLanguageProfileNotifySink() {
    if (!m_pThreadMgr || !m_dwActiveLangProfileCookie) return;
    ITfSource* pSource = nullptr;
    if (SUCCEEDED(m_pThreadMgr->QueryInterface(IID_ITfSource,
                                               reinterpret_cast<void**>(&pSource)))) {
        pSource->UnadviseSink(m_dwActiveLangProfileCookie);
        pSource->Release();
    }
    m_dwActiveLangProfileCookie = 0;
}

HRESULT CTextService::_InitTextEditSink(ITfContext* pContext) {
    if (!pContext) return E_FAIL;
    // 若已连接旧上下文，先断开
    if (m_pFocusContext) _UninitTextEditSink();

    ITfSource* pSource = nullptr;
    if (SUCCEEDED(pContext->QueryInterface(IID_ITfSource,
                                           reinterpret_cast<void**>(&pSource)))) {
        pSource->AdviseSink(IID_ITfTextEditSink,
                            static_cast<ITfTextEditSink*>(this),
                            &m_dwTextEditSinkCookie);
        pSource->Release();
    }
    pContext->AddRef();
    m_pFocusContext = pContext;
    return S_OK;
}

void CTextService::_UninitTextEditSink() {
    if (m_pFocusContext && m_dwTextEditSinkCookie) {
        ITfSource* pSource = nullptr;
        if (SUCCEEDED(m_pFocusContext->QueryInterface(
                IID_ITfSource, reinterpret_cast<void**>(&pSource)))) {
            pSource->UnadviseSink(m_dwTextEditSinkCookie);
            pSource->Release();
        }
        m_dwTextEditSinkCookie = 0;
    }
    if (m_pFocusContext) {
        m_pFocusContext->Release();
        m_pFocusContext = nullptr;
    }
}

HRESULT CTextService::_InitLangBar() {
    if (m_pLangBar) return S_OK;
    m_pLangBar = new (std::nothrow) CLangBarButton(this);
    if (!m_pLangBar) return E_OUTOFMEMORY;
    m_pLangBar->Update();
    if (m_pThreadMgr) {
        ITfLangBarItemMgr* pMgr = nullptr;
        if (SUCCEEDED(m_pThreadMgr->QueryInterface(
                IID_ITfLangBarItemMgr, reinterpret_cast<void**>(&pMgr)))) {
            pMgr->AddItem(m_pLangBar);  // AddItem AddRef
            pMgr->Release();
        }
    }
    return S_OK;
}

void CTextService::_UninitLangBar() {
    if (!m_pLangBar) return;
    if (m_pThreadMgr) {
        ITfLangBarItemMgr* pMgr = nullptr;
        if (SUCCEEDED(m_pThreadMgr->QueryInterface(
                IID_ITfLangBarItemMgr, reinterpret_cast<void**>(&pMgr)))) {
            pMgr->RemoveItem(m_pLangBar);  // Release
            pMgr->Release();
        }
    }
    m_pLangBar->Release();  // 释放创建引用
    m_pLangBar = nullptr;
}

// ---------------- 组合管理 ----------------
void CTextService::_StartComposition(ITfContext* pContext) {
    if (!pContext || !m_pCore) return;
    CStartCompositionEditSession* s =
        new (std::nothrow) CStartCompositionEditSession(this, pContext,
                                                        m_wzPinyin, &m_pComposition);
    if (!s) return;
    HRESULT hr = E_FAIL;
    pContext->RequestEditSession(m_clientId, s,
                                 TF_ES_ASYNC | TF_ES_READWRITE, &hr);
    s->Release();
}

void CTextService::_UpdateComposition(ITfContext* pContext) {
    if (!pContext || !m_pComposition) return;
    CUpdateCompositionEditSession* s =
        new (std::nothrow) CUpdateCompositionEditSession(this, pContext, m_wzPinyin,
                                                         m_pComposition);
    if (!s) return;
    HRESULT hr = E_FAIL;
    pContext->RequestEditSession(m_clientId, s,
                                 TF_ES_ASYNC | TF_ES_READWRITE, &hr);
    s->Release();
}

void CTextService::_EndComposition(ITfContext* pContext) {
    if (!pContext) return;
    if (m_pComposition) {
        CEndCompositionEditSession* s =
            new (std::nothrow) CEndCompositionEditSession(this, pContext,
                                                         m_pComposition);
        if (s) {
            HRESULT hr = E_FAIL;
            pContext->RequestEditSession(m_clientId, s,
                                         TF_ES_ASYNC | TF_ES_READWRITE, &hr);
            s->Release();
        }
        // 释放 StartComposition 时取得的一处引用，避免悬空/泄漏
        m_pComposition->Release();
        m_pComposition = nullptr;
    }
    m_wzPinyin.clear();
}

void CTextService::_CommitText(const std::wstring& korean) {
    if (korean.empty()) return;
    m_wzPinyin.clear();
    ITfContext* ctx = _GetFocusContext();
    if (ctx) {
        if (m_pComposition) {
            CCommitTextEditSession* s =
                new (std::nothrow) CCommitTextEditSession(this, ctx, korean,
                                                          m_pComposition);
            if (s) {
                HRESULT hr = E_FAIL;
                ctx->RequestEditSession(m_clientId, s,
                                        TF_ES_ASYNC | TF_ES_READWRITE, &hr);
                s->Release();
            }
        }
        ctx->Release();
    }
    if (m_pComposition) {
        m_pComposition->Release();
        m_pComposition = nullptr;
    }
    _ShowCandidate();  // pinyin 已空 → 隐藏候选窗
}

// ---------------- 拼音累积 / 热键路由 ----------------
void CTextService::_HandlePinyinKey(WCHAR ch) {
    if (ch == L'\'' || ch == L'-' || (ch >= L'a' && ch <= L'z')) {
        m_wzPinyin += ch;
    } else {
        return;
    }
    if (m_pCore) m_candidates = m_pCore->Process(m_wzPinyin);
    m_iPage = 0;
    ITfContext* ctx = _GetFocusContext();
    if (ctx) {
        if (m_pComposition) _UpdateComposition(ctx);
        else _StartComposition(ctx);
        ctx->Release();
    }
}

void CTextService::_SelectCandidate(int n) {
    const int pageSize = hanpinyin::CandidateManager::kPageSize;
    int idx = m_iPage * pageSize + (n - 1);
    if (idx < 0 || idx >= static_cast<int>(m_candidates.items.size())) return;
    const hanpinyin::Candidate& c = m_candidates.items[idx];
    if (c.korean.empty()) return;
    _CommitText(c.korean);
}

void CTextService::_NextPage() {
    if (m_candidates.hasMore) {
        ++m_iPage;
        _ShowCandidate();
    }
}

void CTextService::_PrevPage() {
    if (m_iPage > 0) {
        --m_iPage;
        _ShowCandidate();
    }
}

void CTextService::_Backspace() {
    if (m_wzPinyin.empty()) {
        ITfContext* ctx = _GetFocusContext();
        if (ctx) { _EndComposition(ctx); ctx->Release(); }
        return;
    }
    m_wzPinyin.pop_back();
    if (m_pCore) {
        m_candidates = m_wzPinyin.empty()
                           ? hanpinyin::CandidateList{}
                           : m_pCore->Process(m_wzPinyin);
    }
    m_iPage = 0;
    ITfContext* ctx = _GetFocusContext();
    if (ctx) {
        if (m_wzPinyin.empty()) {
            _EndComposition(ctx);
        } else if (m_pComposition) {
            _UpdateComposition(ctx);
        }
        ctx->Release();
    }
    _ShowCandidate();
}

void CTextService::_ShowCandidate() {
    if (!m_pCandidateUI) return;
    if (m_wzPinyin.empty()) {
        m_pCandidateUI->Hide();
        return;
    }
    m_candidates.page = m_iPage;  // 供 D2DRenderer 内部分页
    m_pCandidateUI->Show(m_candidates, m_caretRect);
}

void CTextService::_ClearPinyin() {
    m_wzPinyin.clear();
    m_iPage = 0;
    m_candidates = hanpinyin::CandidateList{};
}

// ---------------- 配置 / 路径 ----------------
void CTextService::_LoadConfig() {
    std::wstring mainP = _DataPath(L"data\\main_dict.json");
    std::wstring phraseP = _DataPath(L"data\\phrases.json");
    std::wstring userP = _DataPath(L"data\\user_dict.json");
    if (m_pCore) m_pCore->LoadDict(mainP, phraseP, userP);

    HKEY hk = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\HanPinyin", 0, KEY_READ,
                      &hk) == ERROR_SUCCESS) {
        DWORD enabled = 1, english = 0, fuzzy = 1, sz = sizeof(DWORD);
        RegQueryValueExW(hk, L"Enabled", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&enabled), &sz);
        RegQueryValueExW(hk, L"EnglishMode", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&english), &sz);
        RegQueryValueExW(hk, L"FuzzyOn", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&fuzzy), &sz);
        m_bEnabled = enabled ? TRUE : FALSE;
        m_bEnglishMode = english ? TRUE : FALSE;
        if (m_pCore) m_pCore->SetFuzzyEnabled(fuzzy ? true : false);
        RegCloseKey(hk);
    } else {
        if (m_pCore) m_pCore->SetFuzzyEnabled(true);
    }
}

ITfContext* CTextService::_GetFocusContext() {
    if (!m_pThreadMgr) return nullptr;
    ITfDocumentMgr* pDocMgr = nullptr;
    if (FAILED(m_pThreadMgr->GetFocus(&pDocMgr)) || !pDocMgr) return nullptr;
    ITfContext* pContext = nullptr;
    pDocMgr->GetBase(&pContext);
    pDocMgr->Release();
    return pContext;  // 调用方需 Release
}

std::wstring CTextService::_DataPath(const wchar_t* fileName) const {
    wchar_t buf[MAX_PATH] = {0};
    if (g_hInstance) GetModuleFileNameW(g_hInstance, buf, MAX_PATH);
    std::wstring p = buf;
    auto pos = p.find_last_of(L'\\');
    if (pos != std::wstring::npos) p = p.substr(0, pos + 1);
    return p + fileName;
}

// ---------------- 编辑会话回调 ----------------
void CTextService::OnCompositionRendered(TfEditCookie ec, ITfContext* pContext) {
    _UpdateCaretRect(ec, pContext);
    _ShowCandidate();
}

void CTextService::_UpdateCaretRect(TfEditCookie ec, ITfContext* pContext) {
    if (!pContext) return;
    ITfContextView* pView = nullptr;
    if (FAILED(pContext->GetActiveView(&pView)) || !pView) return;

    ITfRange* pRange = nullptr;
    TF_SELECTION sel = {};
    ULONG fetched = 0;
    if (SUCCEEDED(pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel,
                                         &fetched)) &&
        fetched == 1) {
        pRange = sel.range;
    } else if (m_pComposition) {
        m_pComposition->GetRange(&pRange);
    }

    if (pRange) {
        RECT rc = {0};
        BOOL fClipped = FALSE;
        if (SUCCEEDED(pView->GetTextExt(ec, pRange, &rc, &fClipped))) {
            m_caretRect = rc;
        }
        pRange->Release();
    }
    pView->Release();
}

void CTextService::ApplyDisplayAttribute(ITfContext* pContext, TfEditCookie ec,
                                          ITfRange* pRange) {
    if (!pContext || !pRange || !m_pDAInfo) return;
    ITfProperty* pProp = nullptr;
    if (FAILED(pContext->GetProperty(GUID_PROP_ATTRIBUTE, &pProp)) || !pProp)
        return;

    VARIANT var;
    VariantInit(&var);
    var.vt = VT_UNKNOWN;
    var.punkVal = m_pDAInfo;  // SetValue 会 AddRef
    pProp->SetValue(ec, pRange, &var);
    pProp->Release();
    VariantClear(&var);
}

void CTextService::SetEnabled(BOOL enabled) {
    m_bEnabled = enabled;
    if (m_pLangBar) m_pLangBar->Update();
}

void CTextService::SetEnglishMode(BOOL english) {
    m_bEnglishMode = english;
    if (m_pLangBar) m_pLangBar->Update();
}

// ---------------- ITfDisplayAttributeProvider ----------------
STDMETHODIMP CTextService::EnumDisplayAttributeInfo(
    IEnumTfDisplayAttributeInfo** ppEnum) {
    if (!ppEnum) return E_INVALIDARG;
    *ppEnum = nullptr;
    if (!m_pDAInfo) return E_FAIL;
    CEnumDisplayAttributeInfo* pEnum =
        new (std::nothrow) CEnumDisplayAttributeInfo(m_pDAInfo);
    if (!pEnum) return E_OUTOFMEMORY;
    *ppEnum = pEnum;
    pEnum->AddRef();
    return S_OK;
}

STDMETHODIMP CTextService::GetDisplayAttributeInfo(
    REFGUID guid, ITfDisplayAttributeInfo** ppInfo) {
    if (!ppInfo) return E_INVALIDARG;
    *ppInfo = nullptr;
    if (guid != GUID_HanPinyinDisplayAttribute) return E_INVALIDARG;
    if (!m_pDAInfo) return E_FAIL;
    *ppInfo = m_pDAInfo;
    m_pDAInfo->AddRef();
    return S_OK;
}

// ---------------- ITfActiveLanguageProfileNotifySink ----------------
STDMETHODIMP CTextService::OnActivated(REFCLSID clsid, REFGUID guidProfile,
                                       BOOL fActivated) {
    (void)guidProfile;
    if (clsid == CLSID_HanPinyinTextService) {
        m_bActivated = fActivated;
        if (m_pLangBar) m_pLangBar->Update();
    }
    return S_OK;
}

}  // namespace tsf
}  // namespace hanpinyin
