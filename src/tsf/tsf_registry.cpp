// HanPinyin · 注册表辅助实现

#include "tsf_registry.h"
#include "tsf_guid.h"
#include <string>

namespace hanpinyin {
namespace tsf {

namespace {

// 写 REG_SZ（含子键递归创建）
bool SetStr(HKEY root, const wchar_t* subKey, const wchar_t* valueName,
            const wchar_t* data) {
    HKEY hKey = nullptr;
    LONG res = RegCreateKeyExW(root, subKey, 0, nullptr,
                               REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                               &hKey, nullptr);
    if (res != ERROR_SUCCESS) return false;
    res = RegSetValueExW(hKey, valueName, 0, REG_SZ,
                         reinterpret_cast<const BYTE*>(data),
                         static_cast<DWORD>((wcslen(data) + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return res == ERROR_SUCCESS;
}

// 写 REG_DWORD
bool SetDword(HKEY root, const wchar_t* subKey, const wchar_t* valueName,
              DWORD data) {
    HKEY hKey = nullptr;
    LONG res = RegCreateKeyExW(root, subKey, 0, nullptr,
                               REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                               &hKey, nullptr);
    if (res != ERROR_SUCCESS) return false;
    res = RegSetValueExW(hKey, valueName, 0, REG_DWORD,
                         reinterpret_cast<const BYTE*>(&data), sizeof(DWORD));
    RegCloseKey(hKey);
    return res == ERROR_SUCCESS;
}

// 删除键（递归，含自身）。忽略“不存在”的错误。
void DeleteKeyRecursive(HKEY root, const wchar_t* subKey) {
    // 先尝试直接删除（空键）；失败则枚举子键递归。
    LONG res = RegDeleteTreeW(root, subKey);
    (void)res;
}

std::wstring ClsidString() {
    const GUID& g = CLSID_HanPinyinTextService;
    wchar_t buf[64] = {};
    swprintf_s(buf, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
               g.Data1, g.Data2, g.Data3,
               g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
               g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return std::wstring(buf);
}

std::wstring ProfileString() {
    const GUID& g = GUID_HanPinyinLangProfile;
    wchar_t buf[64] = {};
    swprintf_s(buf, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
               g.Data1, g.Data2, g.Data3,
               g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
               g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return std::wstring(buf);
}

std::wstring CatKeyboardString() {
    // GUID_TFCAT_TIP_KEYBOARD 正确值（与 msctf.h / tsf_iids.cpp 第 78-79 行一致）
    return std::wstring(L"{34745C63-B2F0-4784-8B67-5E12C8701A31}");
}

}  // namespace

bool RegisterTSF(const wchar_t* dllPath) {
    std::wstring clsid = ClsidString();
    std::wstring profile = ProfileString();

    // 1) HKCR\CLSID\{clsid}
    std::wstring clsidKey = L"CLSID\\" + clsid;
    if (!SetStr(HKEY_CLASSES_ROOT, clsidKey.c_str(), nullptr,
                L"HanPinyin Text Service"))
        return false;

    // 2) HKCR\CLSID\{clsid}\InprocServer32
    std::wstring inprocKey = clsidKey + L"\\InprocServer32";
    if (!SetStr(HKEY_CLASSES_ROOT, inprocKey.c_str(), nullptr, dllPath))
        return false;
    if (!SetStr(HKEY_CLASSES_ROOT, inprocKey.c_str(), L"ThreadingModel",
                L"Apartment"))
        return false;

    // 3) HKLM\SOFTWARE\Microsoft\CTF\TIP\{clsid}（TIP 根键，与微软拼音一致：无值）
    std::wstring tipKey = L"SOFTWARE\\Microsoft\\CTF\\TIP\\" + clsid;
    if (!SetStr(HKEY_LOCAL_MACHINE, tipKey.c_str(), nullptr, L""))
        return false;

    // 4) Category 子键：Category\Category\{GUID_TFCAT_TIP_KEYBOARD}
    //    必须以 TIP 的 CLSID 作为「命名值」（值为空）登记，TSF 管理器才会枚举到本 TIP。
    std::wstring catKey = tipKey + L"\\Category\\Category\\" + CatKeyboardString();
    if (!SetStr(HKEY_LOCAL_MACHINE, catKey.c_str(), clsid.c_str(), L""))
        return false;

    // 5) LanguageProfile\{0x00000804}\{profile} 必填显示值
    std::wstring lpKey =
        tipKey + L"\\LanguageProfile\\0x00000804\\" + profile;
    // 空默认值（与示例一致）
    if (!SetStr(HKEY_LOCAL_MACHINE, lpKey.c_str(), nullptr, L""))
        return false;
    if (!SetStr(HKEY_LOCAL_MACHINE, lpKey.c_str(), L"Description", L"HanPinyin"))
        return false;
    if (!SetStr(HKEY_LOCAL_MACHINE, lpKey.c_str(), L"IconFile", dllPath))
        return false;
    if (!SetDword(HKEY_LOCAL_MACHINE, lpKey.c_str(), L"IconIndex", 0))
        return false;
    if (!SetDword(HKEY_LOCAL_MACHINE, lpKey.c_str(), L"Enable", 1))
        return false;
    if (!SetDword(HKEY_LOCAL_MACHINE, lpKey.c_str(), L"HiddenInSettingUI", 0))
        return false;
    if (!SetDword(HKEY_LOCAL_MACHINE, lpKey.c_str(), L"SubItemInSettingUI", 0))
        return false;

    // 5b) 关联键：把本 TIP 挂到简体中文 locale，否则不会出现在输入法列表
    std::wstring assocKey = lpKey + L"\\Associations";
    if (!SetStr(HKEY_LOCAL_MACHINE, assocKey.c_str(), L"00000804", profile.c_str()))
        return false;

    return true;
}

bool UnregisterTSF() {
    std::wstring clsid = ClsidString();
    std::wstring profile = ProfileString();

    // 删除 LanguageProfile 下的 profile 子键
    std::wstring lpBase = L"SOFTWARE\\Microsoft\\CTF\\TIP\\" + clsid +
                          L"\\LanguageProfile\\0x00000804";
    DeleteKeyRecursive(HKEY_LOCAL_MACHINE, (lpBase + L"\\" + profile).c_str());
    // 删除 Category 子键
    DeleteKeyRecursive(HKEY_LOCAL_MACHINE,
                       (L"SOFTWARE\\Microsoft\\CTF\\TIP\\" + clsid + L"\\Category").c_str());
    // 删除 TIP 根键
    DeleteKeyRecursive(HKEY_LOCAL_MACHINE,
                       (L"SOFTWARE\\Microsoft\\CTF\\TIP\\" + clsid).c_str());

    // 删除 HKCR\CLSID\{clsid}
    DeleteKeyRecursive(HKEY_CLASSES_ROOT, (L"CLSID\\" + clsid).c_str());

    return true;
}

}  // namespace tsf
}  // namespace hanpinyin
