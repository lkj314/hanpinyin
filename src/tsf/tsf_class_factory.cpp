// HanPinyin · COM 类厂实现

#include "tsf_class_factory.h"
#include "tsf_text_service.h"  // CTextService 完整定义
#include "tsf_guid.h"
#include <windows.h>
#include <new>

namespace hanpinyin {
namespace tsf {

extern void DllAddRef();
extern void DllRelease();

CClassFactory::CClassFactory() : m_cRef(1) {
    DllAddRef();
}

CClassFactory::~CClassFactory() {
    DllRelease();
}

STDMETHODIMP CClassFactory::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
        *ppv = static_cast<IClassFactory*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CClassFactory::AddRef() {
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CClassFactory::Release() {
    ULONG cr = InterlockedDecrement(&m_cRef);
    if (cr == 0) delete this;
    return cr;
}

STDMETHODIMP CClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid,
                                           void** ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;

    // 不支持聚合
    if (pUnkOuter != nullptr) return CLASS_E_NOAGGREGATION;

    // 仅支持以 TIP 接口查询
    if (riid != IID_IUnknown && riid != IID_ITfTextInputProcessor &&
        riid != IID_ITfTextInputProcessorEx) {
        return E_NOINTERFACE;
    }

    CTextService* pService = new (std::nothrow) CTextService();
    if (!pService) return E_OUTOFMEMORY;

    // 取得请求的接口指针（同时完成 AddRef）
    HRESULT hr = pService->QueryInterface(riid, ppv);
    // 平衡构造时隐含的引用（QI 内部 AddRef 了一次；此处 Release 掉创建引用）
    pService->Release();
    return hr;
}

STDMETHODIMP CClassFactory::LockServer(BOOL fLock) {
    if (fLock) DllAddRef();
    else DllRelease();
    return S_OK;
}

}  // namespace tsf
}  // namespace hanpinyin
