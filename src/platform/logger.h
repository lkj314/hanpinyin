// HanPinyin · 极简文件日志（写入 exe 同目录 hanpinyin.log）
// 用于在无控制台 GUI 程序中捕获启动/初始化失败，便于排查"双击无反应"。

#pragma once

#include <windows.h>
#include <fstream>
#include <string>
#include <ctime>

namespace hanpinyin {

inline std::wstring hp_log_path() {
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p = buf;
    auto pos = p.find_last_of(L'\\');
    if (pos != std::wstring::npos) p = p.substr(0, pos + 1);
    return p + L"hanpinyin.log";
}

inline void hp_log(const std::string& msg) {
    std::ofstream f(hp_log_path(), std::ios::app);
    if (!f) return;
    char tbuf[32] = {0};
    time_t now = time(nullptr);
    tm lt = {};
    localtime_s(&lt, &now);
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &lt);
    f << "[" << tbuf << "] " << msg << "\n";
}

}  // namespace hanpinyin
