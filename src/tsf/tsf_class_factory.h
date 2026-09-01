// HanPinyin · COM 类厂（IClassFactory）
// 由 DllGetClassObject 创建，CreateInstance 产出 CTextService（TSF TIP）。

#pragma once

#include <windows.h>
#include <unknwn.h>  // IClassFactory

namespace hanpinyin {
namespace tsf {

class CClassFactory : public IClassFactory {
public:
    CClassFactory();
    virtual ~CClassFactory();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid,
                                void** ppv) override;
    STDMETHODIMP LockServer(BOOL fLock) override;

private:
    ULONG m_cRef;
};

}  // namespace tsf
}  // namespace hanpinyin
