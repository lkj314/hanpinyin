// HanPinyin · 编辑会话（ITfEditSession）
// 所有对目标文本存储的写操作（ITfRange::SetText / 起止组合）必须在
// DoEditSession 内完成。上屏经官方通道，绝不使用 SendInput。

#pragma once

#include <windows.h>
#include <msctf.h>
#include <string>

namespace hanpinyin {
namespace tsf {

class CTextService;  // 前置声明

// 编辑会话基类：持有文本服务指针与上下文（AddRef），实现 IUnknown + ITfEditSession
class CEditSessionBase : public ITfEditSession {
protected:
    CTextService* m_pTextService;
    ITfContext* m_pContext;  // AddRef 于构造，Release 于析构
    LONG m_cRef;

    CEditSessionBase(CTextService* pTextService, ITfContext* pContext);
    virtual ~CEditSessionBase();

public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    STDMETHODIMP DoEditSession(TfEditCookie ec) override = 0;
};

// 起组合：在光标处 StartComposition 并写入拼音组合串（带显示属性高亮）
class CStartCompositionEditSession : public CEditSessionBase {
public:
    CStartCompositionEditSession(CTextService* ts, ITfContext* ctx,
                                 const std::wstring& text,
                                 ITfComposition** ppComp);
    STDMETHODIMP DoEditSession(TfEditCookie ec) override;

private:
    std::wstring m_text;
    ITfComposition** m_ppComp;
};

// 更新组合：对已存在的组合 range 改写拼音串（翻字母 / 退格）
class CUpdateCompositionEditSession : public CEditSessionBase {
public:
    CUpdateCompositionEditSession(CTextService* ts, ITfContext* ctx,
                                 const std::wstring& text,
                                 ITfComposition* pComp);
    STDMETHODIMP DoEditSession(TfEditCookie ec) override;

private:
    std::wstring m_text;
    ITfComposition* m_pComp;
};

// 提交：将组合 range 改写为韩文并 EndComposition（上屏）
class CCommitTextEditSession : public CEditSessionBase {
public:
    CCommitTextEditSession(CTextService* ts, ITfContext* ctx,
                           const std::wstring& korean, ITfComposition* pComp);
    STDMETHODIMP DoEditSession(TfEditCookie ec) override;

private:
    std::wstring m_text;
    ITfComposition* m_pComp;
};

// 取消：仅 EndComposition（Esc 取消，拼音从文档移除）
class CEndCompositionEditSession : public CEditSessionBase {
public:
    CEndCompositionEditSession(CTextService* ts, ITfContext* ctx,
                               ITfComposition* pComp);
    STDMETHODIMP DoEditSession(TfEditCookie ec) override;

private:
    ITfComposition* m_pComp;
};

}  // namespace tsf
}  // namespace hanpinyin
