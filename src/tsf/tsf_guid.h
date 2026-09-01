// HanPinyin · TSF GUID 常量声明
// 所有自定义 GUID 在此声明（extern），定义放在 tsf_guid.cpp（仅一个翻译单元持有实体）。
// 系统 GUID（IID_ITf*、GUID_TFCAT_TIP_KEYBOARD 等）由 <msctf.h> 提供，无需此声明。

#pragma once

#include <windows.h>  // 提供 GUID 完整定义（CLSID_* 以 const GUID 声明）

// 系统 GUID（IID_ITf*、GUID_TFCAT_TIP_KEYBOARD 等）由 <msctf.h> 前向声明，
// 其实体定义在 tsf_iids.cpp（现代 SDK 已移除 msctf.lib）。本文件仅声明自定义 GUID。

// TSF 文本输入处理器（TIP）的 CLSID
extern "C" const GUID CLSID_HanPinyinTextService;

// 语言档 GUID（简体中文 0x0804 下的一个具体输入法配置档）
extern "C" const GUID GUID_HanPinyinLangProfile;

// 组合串高亮用的显示属性（Display Attribute）GUID
extern "C" const GUID GUID_HanPinyinDisplayAttribute;

// 语言栏按钮项 GUID
extern "C" const GUID GUID_HanPinyinLangBarItem;
