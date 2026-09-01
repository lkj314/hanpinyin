# HanPinyin 纯逻辑单元测试
# 仅链接 hanpinyin_core，可在无 Win32 环境下编译运行（core 不含 windows.h）。
#
# 注意：每个测试 .cpp 自带 int main()，因此必须分别编译为独立可执行目标，
# 否则全部编入同一可执行文件会因多个 main 触发 LNK2005（P0-3 修复）。
set(HP_CORE_TESTS
    test_trie
    test_pinyin_segmenter
    test_fuzzy_normalizer
    test_candidate_manager
    test_viterbi
)

foreach(t ${HP_CORE_TESTS})
    add_executable(${t} ${t}.cpp)
    target_link_libraries(${t} PRIVATE hanpinyin_core)
    add_test(NAME ${t} COMMAND ${t})
endforeach()
