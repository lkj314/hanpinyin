// HanPinyin · 候选窗（TSF 拥有的自绘 popup，复用 CD2DRenderer）
// 无焦点窗口（WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW），由 TSF 经 GetTextExt
// 计算的光标矩形定位。纵向列 1-9 候选项（数字前缀）+ 底部翻页提示。

#pragma once

#include <windows.h>
#include <string>
#include "types.h"                   // hanpinyin::CandidateList（与 d2d_renderer.h 内 include 形式一致，避免重复包含）
#include "d2d_renderer.h" // CD2DRenderer（复用，src/platform 已在 include 路径）

namespace hanpinyin {
namespace tsf {

class CCandidateWindow {
public:
    CCandidateWindow();
    ~CCandidateWindow();

    // 创建无焦点 popup 窗口并初始化 D2D 渲染器
    bool create();
    // 显示候选（list 为完整候选列表，内部按 list.page 分页）
    void Show(const hanpinyin::CandidateList& list, const RECT& caretRect);
    // 隐藏
    void Hide();
    // 设置组合拼音行（用于绘制拼音输入行）
    void SetPinyin(const std::wstring& pinyin) { m_pinyinLine = pinyin; }
    // 翻页（由 CTextService 直接重绘，无需本类状态）
    bool isVisible() const;

private:
    static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l);

    void renderFrame();  // WM_PAINT 时绘制
    int windowHeight(const hanpinyin::CandidateList& list) const;

    hanpinyin::D2DRenderer m_renderer;
    HWND m_hWnd = nullptr;
    hanpinyin::CandidateList m_list;
    std::wstring m_pinyinLine;
    RECT m_caretRect = {0, 0, 0, 0};

    static const int kWidth = 340;
    static const int kHeaderH = 32;
    static const int kItemH = 22;
    static const int kPadY = 6;
    static const int kPageSize = 5;
};

}  // namespace tsf
}  // namespace hanpinyin
