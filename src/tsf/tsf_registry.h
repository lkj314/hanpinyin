// HanPinyin · 注册表辅助（CTF TIP + HKCR CLSID 写入/删除）
// 由 DllRegisterServer / DllUnregisterServer 调用。需管理员权限（设置 exe 以 runas 启动 regsvr32）。

#pragma once

#include <windows.h>

namespace hanpinyin {
namespace tsf {

// 写 HKCR\CLSID + HKLM\CTF\TIP 注册项；dllPath 为 DLL 绝对路径。返回 TRUE 表示全部成功。
bool RegisterTSF(const wchar_t* dllPath);

// 删除上述注册项。返回 TRUE 表示全部成功（即便部分键原本不存在也返回 TRUE）。
bool UnregisterTSF();

}  // namespace tsf
}  // namespace hanpinyin
