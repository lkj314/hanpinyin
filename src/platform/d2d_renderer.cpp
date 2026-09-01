// HanPinyin · Direct2D 渲染器实现

#include "d2d_renderer.h"
#include "../core/types.h"
#include "../core/candidate_manager.h"
#include <cwchar>

namespace hanpinyin {

D2DRenderer::D2DRenderer() = default;

D2DRenderer::~D2DRenderer() {
    destroy();
}

std::wstring D2DRenderer::toWide(const std::string& s) {
    return hanpinyin::utf8_to_wstring(s);
}

bool D2DRenderer::init(HWND hwnd) {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory_);
    if (FAILED(hr)) return false;

    RECT rc;
    GetClientRect(hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(static_cast<UINT32>(rc.right - rc.left),
                                   static_cast<UINT32>(rc.bottom - rc.top));

    hr = factory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, size),
        &rt_);
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(&writeFactory_));
    if (FAILED(hr)) return false;

    hr = writeFactory_->CreateTextFormat(
        L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"", &format_);
    if (FAILED(hr)) return false;
    format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    // CreateSolidColorBrush 接受 const D2D1_COLOR_F&（引用），以具名局部变量传入，
    // 避免对临时量取地址/生命周期的疑虑，风格也与下方矩形处理保持一致。
    D2D1_COLOR_F cBg = D2D1::ColorF(0.12f, 0.14f, 0.20f, 1.0f);
    rt_->CreateSolidColorBrush(cBg, &bgBrush_);
    D2D1_COLOR_F cText = D2D1::ColorF(0.95f, 0.97f, 1.0f, 1.0f);
    rt_->CreateSolidColorBrush(cText, &textBrush_);
    D2D1_COLOR_F cAccent = D2D1::ColorF(0.30f, 0.75f, 1.0f, 1.0f);
    rt_->CreateSolidColorBrush(cAccent, &accentBrush_);
    D2D1_COLOR_F cHl = D2D1::ColorF(0.20f, 0.28f, 0.40f, 1.0f);
    rt_->CreateSolidColorBrush(cHl, &hlBrush_);

    hwnd_ = hwnd;  // 记录 HWND，便于后续布局/缩放使用
    return true;
}

void D2DRenderer::resize(int w, int h) {
    if (rt_) {
        rt_->Resize(D2D1::SizeU(static_cast<UINT32>(w), static_cast<UINT32>(h)));
    }
}

void D2DRenderer::render(const std::string& pinyinLine, const CandidateList& list) {
    if (!rt_) return;

    rt_->BeginDraw();

    // 半透明深色背景（配合窗口 WS_EX_LAYERED 的 LWA_ALPHA 形成悬浮面板质感）
    D2D1_SIZE_F sz = rt_->GetSize();
    D2D1_RECT_F rcBg = D2D1::RectF(0, 0, sz.width, sz.height);
    rt_->FillRectangle(rcBg, bgBrush_);

    float padX = 10.0f;
    float padY = 6.0f;
    float lineH = 22.0f;

    // 拼音输入行
    std::wstring line = toWide(pinyinLine);
    if (!line.empty()) {
        D2D1_RECT_F rcLine = D2D1::RectF(padX, padY, sz.width - padX, padY + lineH);
        rt_->DrawText(line.c_str(), static_cast<UINT32>(line.size()), format_,
                      &rcLine, accentBrush_);
    }

    // 候选列表
    // 翻页：从当前页偏移处开始绘制，每页最多 kPageSize 条（P1-3 修复）
    float y = padY + lineH + 4.0f;
    const int start = list.page * CandidateManager::kPageSize;
    int shown = 0;
    for (int idx = start; idx < static_cast<int>(list.items.size()); ++idx) {
        const auto& c = list.items[idx];
        if (shown >= CandidateManager::kPageSize) break;
        int num = shown + 1;
        std::wstring kor = c.korean;
        std::wstring py = toWide(c.source_pinyin);
        std::wstring entry = std::to_wstring(num) + L". " + kor + L" (" + py + L")";

        D2D1_RECT_F rc = D2D1::RectF(padX, y, sz.width - padX, y + lineH);
        if (num == hover_) {
            rt_->FillRectangle(rc, hlBrush_);
        }
        rt_->DrawText(entry.c_str(), static_cast<UINT32>(entry.size()), format_, &rc,
                      textBrush_);
        y += lineH;
        ++shown;
    }

    // 翻页提示
    if (list.hasMore) {
        std::wstring more = L"Tab ▶";
        D2D1_RECT_F rcMore = D2D1::RectF(padX, y, sz.width - padX, y + lineH);
        rt_->DrawText(more.c_str(), static_cast<UINT32>(more.size()), format_,
                      &rcMore, accentBrush_);
    }

    HRESULT hr = rt_->EndDraw();
    if (FAILED(hr)) {
        // 渲染目标可能失效，下次重绘会重建（此处不做复杂处理）
    }
}

void D2DRenderer::destroy() {
    if (hlBrush_) { hlBrush_->Release(); hlBrush_ = nullptr; }
    if (accentBrush_) { accentBrush_->Release(); accentBrush_ = nullptr; }
    if (textBrush_) { textBrush_->Release(); textBrush_ = nullptr; }
    if (bgBrush_) { bgBrush_->Release(); bgBrush_ = nullptr; }
    if (format_) { format_->Release(); format_ = nullptr; }
    if (writeFactory_) { writeFactory_->Release(); writeFactory_ = nullptr; }
    if (rt_) { rt_->Release(); rt_ = nullptr; }
    if (factory_) { factory_->Release(); factory_ = nullptr; }
}

}  // namespace hanpinyin
