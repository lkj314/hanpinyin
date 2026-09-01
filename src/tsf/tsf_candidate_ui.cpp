// HanPinyin · 候选窗实现

#include "tsf_candidate_ui.h"
#include "../core/types.h"
#include <windowsx.h>

namespace hanpinyin {
namespace tsf {

CCandidateWindow::CCandidateWindow() = default;

CCandidateWindow::~CCandidateWindow() {
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
    // CD2DRenderer 析构自动释放 D2D 资源
}

bool CCandidateWindow::create() {
    const wchar_t* cls = L"HanPinyinTsfCandidateCls";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &CCandidateWindow::wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = cls;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(IDC_ARROW));
    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        cls, L"", WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, kWidth, kHeaderH,
        nullptr, nullptr, GetModuleHandleW(nullptr), this);
    if (!m_hWnd) return false;

    // 整体半透明，与 D2D 深色背景叠加形成悬浮质感
    SetLayeredWindowAttributes(m_hWnd, RGB(0, 0, 0), 235, LWA_ALPHA);

    if (!m_renderer.init(m_hWnd)) {
        return false;
    }
    return true;
}

int CCandidateWindow::windowHeight(const hanpinyin::CandidateList& list) const {
    int rows = 0;
    int start = list.page * kPageSize;
    for (int i = start;
         i < static_cast<int>(list.items.size()) && rows < kPageSize; ++i) {
        ++rows;
    }
    int h = kHeaderH + rows * kItemH + (list.hasMore ? kItemH : 0) + kPadY;
    return h;
}

void CCandidateWindow::Show(const hanpinyin::CandidateList& list,
                            const RECT& caretRect) {
    if (!m_hWnd) return;
    m_list = list;
    m_caretRect = caretRect;

    int h = windowHeight(list);
    int x = (m_caretRect.left != 0 || m_caretRect.bottom != 0)
                ? m_caretRect.left
                : 200;
    int y = (m_caretRect.left != 0 || m_caretRect.bottom != 0)
                ? m_caretRect.bottom + 2
                : 200;

    // 避免超出屏幕底边
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (y + h > screenH) y = m_caretRect.top - h - 4;

    MoveWindow(m_hWnd, x, y, kWidth, h, TRUE);
    ShowWindow(m_hWnd, SW_SHOWNA);
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

void CCandidateWindow::Hide() {
    if (m_hWnd) ShowWindow(m_hWnd, SW_HIDE);
}

bool CCandidateWindow::isVisible() const {
    return m_hWnd && IsWindowVisible(m_hWnd);
}

void CCandidateWindow::renderFrame() {
    // 拼音输入行转 UTF-8（CD2DRenderer 接受 UTF-8）
    std::string line = hanpinyin::wstring_to_utf8(m_pinyinLine);
    m_renderer.render(line, m_list);
}

LRESULT CALLBACK CCandidateWindow::wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    CCandidateWindow* self = nullptr;
    if (m == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(l);
        self = reinterpret_cast<CCandidateWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<CCandidateWindow*>(
            GetWindowLongPtrW(h, GWLP_USERDATA));
    }
    if (!self) return DefWindowProcW(h, m, w, l);

    switch (m) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(h, &ps);
            self->renderFrame();
            EndPaint(h, &ps);
            return 0;
        }
        case WM_SIZE: {
            RECT rc;
            GetClientRect(h, &rc);
            int ww = rc.right - rc.left;
            int hh = rc.bottom - rc.top;
            if (ww > 0 && hh > 0) self->m_renderer.resize(ww, hh);
            return 0;
        }
        case WM_DESTROY:
            return 0;
        default:
            return DefWindowProcW(h, m, w, l);
    }
}

}  // namespace tsf
}  // namespace hanpinyin
