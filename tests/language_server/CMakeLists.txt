# ZR-VM Language Server Tests CMakeLists.txt

# 符号表测试
set(SYMBOL_TABLE_TEST_SOURCES
    test_symbol_table.c
)

add_executable(zr_vm_language_server_symbol_table_test ${SYMBOL_TABLE_TEST_SOURCES})

# 引用追踪测试
set(REFERENCE_TRACKER_TEST_SOURCES
    test_reference_tracker.c
)

add_executable(zr_vm_language_server_reference_tracker_test ${REFERENCE_TRACKER_TEST_SOURCES})

# 语义分析器测试
set(SEMANTIC_ANALYZER_TEST_SOURCES
    test_semantic_analyzer.c
)

add_executable(zr_vm_language_server_semantic_analyzer_test ${SEMANTIC_ANALYZER_TEST_SOURCES})

# 增量解析器测试
set(INCREMENTAL_PARSER_TEST_SOURCES
    test_incremental_parser.c
)

add_executable(zr_vm_language_server_incremental_parser_test ${INCREMENTAL_PARSER_TEST_SOURCES})

# LSP 接口测试
set(LSP_INTERFACE_TEST_SOURCES
    test_lsp_interface.c
)

add_executable(zr_vm_language_server_lsp_interface_test ${LSP_INTERFACE_TEST_SOURCES})

# 包含头文件目录
target_include_directories(zr_vm_language_server_symbol_table_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

target_include_directories(zr_vm_language_server_reference_tracker_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

target_include_directories(zr_vm_language_server_semantic_analyzer_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

target_include_directories(zr_vm_language_server_incremental_parser_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

target_include_directories(zr_vm_language_server_lsp_interface_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

# 链接必要的库
if (BUILD_SHARED_LIB)
    target_link_libraries(zr_vm_language_server_symbol_table_test PRIVATE 
        zr_vm_language_server_shared 
        zr_vm_parser_shared 
        zr_vm_core_shared)
    target_link_libraries(zr_vm_language_server_reference_tracker_test PRIVATE 
        zr_vm_language_server_shared 
        zr_vm_parser_shared 
        zr_vm_core_shared)
    target_link_libraries(zr_vm_language_server_semantic_analyzer_test PRIVATE 
        zr_vm_language_server_shared 
        zr_vm_parser_shared 
        zr_vm_core_shared)
    target_link_libraries(zr_vm_language_server_incremental_parser_test PRIVATE 
        zr_vm_language_server_shared 
        zr_vm_parser_shared 
        zr_vm_core_shared)
    target_link_libraries(zr_vm_language_server_lsp_interface_test PRIVATE 
        zr_vm_language_server_shared 
        zr_vm_parser_shared 
        zr_vm_core_shared)
else()
    target_link_libraries(zr_vm_language_server_symbol_table_test PRIVATE 
        zr_vm_language_server_static 
        zr_vm_parser_static 
        zr_vm_core_static)
    target_link_libraries(zr_vm_language_server_reference_tracker_test PRIVATE 
        zr_vm_language_server_static 
        zr_vm_parser_static 
        zr_vm_core_static)
    target_link_libraries(zr_vm_language_server_semantic_analyzer_test PRIVATE 
        zr_vm_language_server_static 
        zr_vm_parser_static 
        zr_vm_core_static)
    target_link_libraries(zr_vm_language_server_incremental_parser_test PRIVATE 
        zr_vm_language_server_static 
        zr_vm_parser_static 
        zr_vm_core_static)
    target_link_libraries(zr_vm_language_server_lsp_interface_test PRIVATE 
        zr_vm_language_server_static 
        zr_vm_parser_static 
        zr_vm_core_static)
endif()

# 添加测试目标
enable_testing()
add_test(NAME language_server_symbol_table_tests COMMAND zr_vm_language_server_symbol_table_test)
add_test(NAME language_server_reference_tracker_tests COMMAND zr_vm_language_server_reference_tracker_test)
add_test(NAME language_server_semantic_analyzer_tests COMMAND zr_vm_language_server_semantic_analyzer_test)
add_test(NAME language_server_incremental_parser_tests COMMAND zr_vm_language_server_incremental_parser_test)
add_test(NAME language_server_lsp_interface_tests COMMAND zr_vm_language_server_lsp_interface_test)
