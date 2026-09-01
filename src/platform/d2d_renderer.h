// HanPinyin · 悬浮窗渲染器（严格 Direct2D + DirectWrite，无 GDI 分支）
// 渲染：半透明背景 + 拼音输入行 + 候选列表（编号 1~5，悬停高亮）。

#pragma once

#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include "types.h"

namespace hanpinyin {

class D2DRenderer {
public:
    D2DRenderer();
    ~D2DRenderer();

    // 绑定到悬浮窗 HWND，创建 D2D 工厂 / 渲染目标 / 文本格式 / 画刷
    bool init(HWND hwnd);

    // 渲染一帧：拼音输入行 + 候选列表
    void render(const std::string& pinyinLine, const CandidateList& list);

    // 窗口尺寸变化（MoveWindow 触发 WM_SIZE）时同步缩放 D2D 渲染目标
    void resize(int w, int h);

    // 设置当前悬停高亮的候选项序号（1-based，-1 表示无）
    void setHover(int idx) { hover_ = idx; }

    void destroy();

private:
    ID2D1Factory* factory_ = nullptr;
    ID2D1HwndRenderTarget* rt_ = nullptr;
    IDWriteFactory* writeFactory_ = nullptr;
    IDWriteTextFormat* format_ = nullptr;
    ID2D1SolidColorBrush* bgBrush_ = nullptr;
    ID2D1SolidColorBrush* textBrush_ = nullptr;
    ID2D1SolidColorBrush* accentBrush_ = nullptr;
    ID2D1SolidColorBrush* hlBrush_ = nullptr;
    int hover_ = -1;
    HWND hwnd_ = nullptr;

    // 将 UTF-8 字符串转为 wstring 用于 DrawText
    static std::wstring toWide(const std::string& s);
};

}  // namespace hanpinyin
