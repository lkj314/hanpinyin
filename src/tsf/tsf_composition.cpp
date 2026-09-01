// HanPinyin · ITfTextEditSink + ITfCompositionSink 实现

#include "tsf_text_service.h"
#include "tsf_candidate_ui.h"
#include <windows.h>
#include <msctf.h>

namespace hanpinyin {
namespace tsf {

// 目标文本存储发生编辑后回调。此处仅作占位：组合串高亮/状态由编辑会话维护。
STDMETHODIMP CTextService::OnEndEdit(ITfContext* pic, TfEditCookie ecRead,
                                     ITfEditRecord* pEditRecord) {
    (void)pic;
    (void)ecRead;
    (void)pEditRecord;
    return S_OK;
}

// 组合被外部（如用户移动光标/切换）终止时回调。将内部组合指针置空，避免悬空。
STDMETHODIMP CTextService::OnCompositionTerminated(TfEditCookie ec,
                                                   ITfComposition* pComposition) {
    (void)ec;
    if (pComposition && pComposition == m_pComposition) {
        m_pComposition->Release();
        m_pComposition = nullptr;
    }
    // 组合结束后清空拼音缓冲与候选窗，回到初始状态
    _ClearPinyin();
    if (m_pCandidateUI) m_pCandidateUI->Hide();
    return S_OK;
}

}  // namespace tsf
}  // namespace hanpinyin
