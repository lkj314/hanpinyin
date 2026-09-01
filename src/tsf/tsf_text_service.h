// HanPinyin · TSF 文本输入处理器（CTextService）
// 单类实现全部 TSF 回调接口（多继承）：
//   ITfTextInputProcessorEx / ITfKeyEventSink / ITfThreadMgrEventSink /
//   ITfTextEditSink / ITfDisplayAttributeProvider /
//   ITfActiveLanguageProfileNotifySink / ITfCompositionSink
// 持有：引擎(CCoreWrapper) / 候选窗(CCandidateWindow) / 语言栏(CLangBarButton) /
//       显示属性(CDisplayAttributeInfo) / 组合状态。

#pragma once

#include <windows.h>
#include <msctf.h>
#include <string>
#include <vector>

#include "../core/types.h"  // hanpinyin::CandidateList

namespace hanpinyin {
namespace tsf {

class CCandidateWindow;
class CCoreWrapper;
class CLangBarButton;
class CDisplayAttributeInfo;

class CTextService
    : public ITfTextInputProcessorEx,
      public ITfKeyEventSink,
      public ITfThreadMgrEventSink,
      public ITfTextEditSink,
      public ITfDisplayAttributeProvider,
      public ITfActiveLanguageProfileNotifySink,
      public ITfCompositionSink {
public:
    CTextService();
    virtual ~CTextService();

    // ---------------- IUnknown ----------------
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ---------------- ITfTextInputProcessor(Ex) ----------------
    STDMETHODIMP Activate(ITfThreadMgr* ptim, TfClientId tid) override;
    STDMETHODIMP ActivateEx(ITfThreadMgr* ptim, TfClientId tid,
                            DWORD dwFlags) override;
    STDMETHODIMP Deactivate() override;

    // ---------------- ITfKeyEventSink ----------------
    STDMETHODIMP OnSetFocus(BOOL fForeground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                               BOOL* pfEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                             BOOL* pfEaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                           BOOL* pfEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                         BOOL* pfEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* pic, REFGUID rguid,
                                BOOL* pfEaten) override;

    // ---------------- ITfThreadMgrEventSink ----------------
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* pdim) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* pdim) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* pdimFocus,
                            ITfDocumentMgr* pdimPrev) override;
    STDMETHODIMP OnPushContext(ITfContext* pic) override;
    STDMETHODIMP OnPopContext(ITfContext* pic) override;

    // ---------------- ITfTextEditSink ----------------
    STDMETHODIMP OnEndEdit(ITfContext* pic, TfEditCookie ecRead,
                           ITfEditRecord* pEditRecord) override;

    // ---------------- ITfDisplayAttributeProvider ----------------
    STDMETHODIMP EnumDisplayAttributeInfo(
        IEnumTfDisplayAttributeInfo** ppEnum) override;
    STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid,
                                         ITfDisplayAttributeInfo** ppInfo) override;

    // ---------------- ITfActiveLanguageProfileNotifySink ----------------
    STDMETHODIMP OnActivated(REFCLSID clsid, REFGUID guidProfile,
                             BOOL fActivated) override;

    // ---------------- ITfCompositionSink ----------------
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ec,
                                         ITfComposition* pComposition) override;

    // ---------------- 供编辑会话回调的内部辅助 ----------------
    // 编辑会话写入组合串后调用：计算光标矩形并刷新候选窗（含显示属性高亮）。
    void OnCompositionRendered(TfEditCookie ec, ITfContext* pContext);
    // 由编辑会话在 SetText 时调用：为组合 range 附加自定义显示属性（高亮）。
    void ApplyDisplayAttribute(ITfContext* pContext, TfEditCookie ec,
                               ITfRange* pRange);
    // 由语言栏按钮调用：切换 开/关。
    void SetEnabled(BOOL enabled);
    BOOL IsEnabled() const { return m_bEnabled; }
    // 由语言栏按钮调用：切换 中/英 模式。
    void SetEnglishMode(BOOL english);
    BOOL IsEnglishMode() const { return m_bEnglishMode; }

private:
    // 连接 / 断开各 sink
    HRESULT _InitThreadMgrSink();
    void _UninitThreadMgrSink();
    HRESULT _InitActiveLanguageProfileNotifySink();
    void _UninitActiveLanguageProfileNotifySink();
    HRESULT _InitTextEditSink(ITfContext* pContext);
    void _UninitTextEditSink();
    HRESULT _InitLangBar();
    void _UninitLangBar();

    // 组合管理（经编辑会话异步执行）
    void _StartComposition(ITfContext* pContext);
    void _UpdateComposition(ITfContext* pContext);
    void _EndComposition(ITfContext* pContext);
    void _CommitText(const std::wstring& korean);

    // 拼音累积 / 热键路由
    void _HandlePinyinKey(WCHAR ch);
    void _SelectCandidate(int n);
    void _NextPage();
    void _PrevPage();
    void _Backspace();
    void _ShowCandidate();
    void _ClearPinyin();

    // 配置加载（HKCU\Software\HanPinyin + 同目录 data/main_dict.json）
    void _LoadConfig();

    // 取当前焦点上下文（AddRef，调用方需 Release）
    ITfContext* _GetFocusContext();

    // 刷新候选窗口的光标矩形（由 GetTextExt 计算）
    void _UpdateCaretRect(TfEditCookie ec, ITfContext* pContext);

    // 计算 DLL 同目录的 data 路径
    std::wstring _DataPath(const wchar_t* fileName) const;

private:
    ITfThreadMgr* m_pThreadMgr = nullptr;
    TfClientId m_clientId = 0;
    DWORD m_dwThreadMgrSinkCookie = 0;
    DWORD m_dwActiveLangProfileCookie = 0;
    DWORD m_dwTextEditSinkCookie = 0;

    ITfContext* m_pFocusContext = nullptr;   // 当前焦点上下文（AddRef）
    ITfComposition* m_pComposition = nullptr;

    CCandidateWindow* m_pCandidateUI = nullptr;
    CCoreWrapper* m_pCore = nullptr;
    CLangBarButton* m_pLangBar = nullptr;
    CDisplayAttributeInfo* m_pDAInfo = nullptr;

    BOOL m_bActivated = FALSE;
    BOOL m_bEnabled = TRUE;
    BOOL m_bEnglishMode = FALSE;

    std::wstring m_wzPinyin;       // 累积拼音（UTF-16）
    int m_iPage = 0;               // 候选页（0-based）
    hanpinyin::CandidateList m_candidates;  // 当前候选（完整列表）
    RECT m_caretRect = {0, 0, 0, 0};

    ULONG m_cRef = 1;
    bool m_bEditSessionPending = false;
};

}  // namespace tsf
}  // namespace hanpinyin
