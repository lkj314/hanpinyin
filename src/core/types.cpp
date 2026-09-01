// HanPinyin · 核心公共类型实现
// 仅依赖标准库，实现 UTF-8 与 UTF-16 的双向转换。

#include "types.h"

namespace hanpinyin {

std::wstring utf8_to_wstring(const std::string& utf8) {
    std::wstring out;
    out.reserve(utf8.size());
    size_t i = 0;
    const size_t n = utf8.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        if (c < 0x80) {
            // 1 字节 ASCII
            out.push_back(static_cast<wchar_t>(c));
            ++i;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < n) {
            // 2 字节
            unsigned int cp = ((static_cast<unsigned int>(c & 0x1F)) << 6) |
                              (static_cast<unsigned int>(utf8[i + 1] & 0x3F));
            out.push_back(static_cast<wchar_t>(cp));
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < n) {
            // 3 字节（韩文绝大多数落在此区间）
            unsigned int cp = ((static_cast<unsigned int>(c & 0x0F)) << 12) |
                              ((static_cast<unsigned int>(utf8[i + 1] & 0x3F)) << 6) |
                              (static_cast<unsigned int>(utf8[i + 2] & 0x3F));
            out.push_back(static_cast<wchar_t>(cp));
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < n) {
            // 4 字节 -> UTF-16 代理对
            unsigned int cp = ((static_cast<unsigned int>(c & 0x07)) << 18) |
                              ((static_cast<unsigned int>(utf8[i + 1] & 0x3F)) << 12) |
                              ((static_cast<unsigned int>(utf8[i + 2] & 0x3F)) << 6) |
                              (static_cast<unsigned int>(utf8[i + 3] & 0x3F));
            cp -= 0x10000;
            out.push_back(static_cast<wchar_t>(0xD800 | (cp >> 10)));
            out.push_back(static_cast<wchar_t>(0xDC00 | (cp & 0x3FF)));
            i += 4;
        } else {
            // 非法字节，跳过
            ++i;
        }
    }
    return out;
}

std::string wstring_to_utf8(const std::wstring& ws) {
    std::string out;
    const size_t n = ws.size();
    for (size_t i = 0; i < n; ++i) {
        wchar_t wc = ws[i];
        unsigned int cp = static_cast<unsigned int>(wc);
        // 处理代理对
        if (wc >= 0xD800 && wc <= 0xDBFF && i + 1 < n) {
            wchar_t wc2 = ws[i + 1];
            cp = 0x10000 + ((cp - 0xD800) << 10) + (wc2 - 0xDC00);
            ++i;
        }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

}  // namespace hanpinyin
