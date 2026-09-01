// HanPinyin · DLL 模块入口与 COM 导出
// DllMain / DllGetClassObject / DllCanUnloadNow / DllRegisterServer /
// DllUnregisterServer；DLL 级引用计数 g_cRefDll 控制卸载。

#include <windows.h>
#include <msctf.h>
#include <new>
#include <olectl.h>
#include "tsf_guid.h"
#include "tsf_class_factory.h"
#include "tsf_registry.h"

namespace hanpinyin {
namespace tsf {

// DLL 级引用计数：所有 COM 对象 AddRef 时 +1，Release 归零时 -1。
// 仅当 g_cRefDll == 0 且无可活动的 TIP 实例时，DllCanUnloadNow 才放行。
volatile LONG g_cRefDll = 0;
HINSTANCE g_hInstance = nullptr;

void DllAddRef() {
    InterlockedIncrement(&g_cRefDll);
}

void DllRelease() {
    InterlockedDecrement(&g_cRefDll);
}

}  // namespace tsf
}  // namespace hanpinyin

using namespace hanpinyin::tsf;

// ---------------- DllMain ----------------
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason,
                               LPVOID lpReserved) {
    (void)lpReserved;
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hInstance = hInstance;
    } else if (dwReason == DLL_PROCESS_DETACH) {
        g_hInstance = nullptr;
    }
    return TRUE;
}

// ---------------- DllGetClassObject ----------------
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid,
                                            LPVOID* ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;

    if (rclsid != CLSID_HanPinyinTextService) return CLASS_E_CLASSNOTAVAILABLE;

    CClassFactory* pFactory = new (std::nothrow) CClassFactory();
    if (!pFactory) return E_OUTOFMEMORY;

    HRESULT hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release();
    return hr;
}

// ---------------- DllCanUnloadNow ----------------
extern "C" HRESULT WINAPI DllCanUnloadNow() {
    // g_cRefDll 为 0（无任何 COM 对象被外部持有）时方可卸载
    return (g_cRefDll == 0) ? S_OK : S_FALSE;
}

// ---------------- DllRegisterServer ----------------
extern "C" HRESULT WINAPI DllRegisterServer() {
    wchar_t dllPath[MAX_PATH] = {0};
    if (!g_hInstance ||
        GetModuleFileNameW(g_hInstance, dllPath, MAX_PATH) == 0) {
        return SELFREG_E_CLASS;
    }
    if (!RegisterTSF(dllPath)) {
        return SELFREG_E_CLASS;
    }
    // 注册成功后广播 WM_SETTINGCHANGE，刷新系统输入法列表（无需重启即出现在添加键盘）。
    DWORD_PTR res = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        reinterpret_cast<LPARAM>(L"intl"),
                        SMTO_ABORTIFHUNG, 5000, &res);
    return S_OK;
}

// ---------------- DllUnregisterServer ----------------
extern "C" HRESULT WINAPI DllUnregisterServer() {
    if (!UnregisterTSF()) {
        return SELFREG_E_CLASS;
    }
    return S_OK;
}
