# HanPinyin 顶层构建（MSVC 14.44 / CMake >= 3.20）
# 目标：
#   hanpinyin_core      STATIC 纯逻辑核心库（不含 windows.h，可被无窗口环境编译）
#   hanpinyin_tsf       SHARED TSF TIP COM DLL（上屏走 ITfRange::SetText 官方通道）
#   hanpinyin_config    WIN32 设置 / 注册 exe（托盘 + regsvr32 自注册/卸载 + 配置面板）
#   <5 个单元测试>      纯逻辑单测（仅链 hanpinyin_core）
cmake_minimum_required(VERSION 3.20)

# 强制 x64（与 64 位目标进程同位数；TSF 控件需一致）
set(CMAKE_GENERATOR_PLATFORM "x64" CACHE STRING "" FORCE)

project(HanPinyin LANGUAGES CXX)

# ---------- 全局编译选项 ----------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 静态链接 MSVC 运行时（/MT）：避免目标机器缺少 VC_redist 时双击无反应
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 源文件与执行字符集均按 UTF-8 读取（韩文宽字面量 L\"...\" 才能正确转 UTF-16）
add_compile_options(/utf-8 /EHsc /permissive-)
# 日志级别宏（可用环境变量 HANPINYIN_LOG 覆盖）
add_compile_definitions(HP_LOG_LEVEL=1 NOMINMAX)

# ========== 纯逻辑核心库（严禁 windows.h）==========
add_library(hanpinyin_core STATIC
    src/core/types.cpp
    src/core/pinyin_segmenter.cpp
    src/core/fuzzy_normalizer.cpp
    src/core/trie.cpp
    src/core/dictionary.cpp
    src/core/user_dict.cpp
    src/core/candidate_manager.cpp
    src/core/config_model.cpp
)
target_include_directories(hanpinyin_core PUBLIC src/core)

# ========== TSF TIP COM DLL（hanpinyin_tsf.dll）==========
# TSF 文本输入处理器：手写 COM（无 ATL），上屏经 ITfRange::SetText。
add_library(hanpinyin_tsf SHARED
    src/tsf/tsf_module.cpp
    src/tsf/tsf_guid.cpp
    src/tsf/tsf_iids.cpp          # TSF 系统 GUID 实体（现代 SDK 已移除 msctf.lib）
    src/tsf/tsf_class_factory.cpp
    src/tsf/tsf_text_service.cpp
    src/tsf/tsf_key_event_sink.cpp
    src/tsf/tsf_composition.cpp
    src/tsf/tsf_edit_sessions.cpp
    src/tsf/tsf_candidate_ui.cpp
    src/tsf/tsf_langbar.cpp
    src/tsf/tsf_display_attribute.cpp
    src/tsf/tsf_registry.cpp
    src/tsf/tsf_core_wrapper.cpp
    src/platform/d2d_renderer.cpp
    src/tsf/hanpinyin_tsf.def          # 模块定义：导出 4 个 Dll* 函数
)
target_include_directories(hanpinyin_tsf PRIVATE
    src/core src/tsf src/platform)
target_link_libraries(hanpinyin_tsf PRIVATE
    hanpinyin_core
    d2d1.lib dwrite.lib user32.lib gdi32.lib imm32.lib
    ole32.lib oleaut32.lib advapi32.lib uuid.lib shell32.lib
)

# ========== 设置 / 注册 exe（hanpinyin_config.exe）==========
# 承载：系统托盘 + 自注册/卸载（regsvr32）+ 配置面板（写 HKCU\Software\HanPinyin）。
add_executable(hanpinyin_config WIN32
    src/app/main.cpp
    src/platform/app_context.cpp
    src/platform/config_panel.cpp
)
target_include_directories(hanpinyin_config PRIVATE src/core src/platform)
target_link_libraries(hanpinyin_config PRIVATE
    hanpinyin_core
    user32.lib gdi32.lib shell32.lib advapi32.lib ole32.lib imm32.lib
)

# ========== 单元测试（子目录）==========
enable_testing()
add_subdirectory(tests)
