# ZR-VM Language Server Tests CMakeLists.txt
# TODO: 标准构建未引入本文件；恢复注册前核对与 tests/CMakeLists.txt 重名的目标及 stdio smoke 测试。
# 七个独立 C 回归目标分别对应下方 CTest；标准构建使用 tests/CMakeLists.txt 的集中注册链。
set(SYMBOL_TABLE_TEST_SOURCES
    test_symbol_table.c
)

add_executable(zr_vm_language_server_symbol_table_test ${SYMBOL_TABLE_TEST_SOURCES})

# 引用追踪用例独立成目标，使其失败与符号表和语义分析回归分开定位。
set(REFERENCE_TRACKER_TEST_SOURCES
    test_reference_tracker.c
)

add_executable(zr_vm_language_server_reference_tracker_test ${REFERENCE_TRACKER_TEST_SOURCES})

# 语义分析用例单独建目标，以便在完整 LSP 协议测试之前验证语义层。
set(SEMANTIC_ANALYZER_TEST_SOURCES
    test_semantic_analyzer.c
)

add_executable(zr_vm_language_server_semantic_analyzer_test ${SEMANTIC_ANALYZER_TEST_SOURCES})

# 增量解析回归有自己的可执行目标和 CTest，避免与协议测试混合。
set(INCREMENTAL_PARSER_TEST_SOURCES
    test_incremental_parser.c
)

add_executable(zr_vm_language_server_incremental_parser_test ${INCREMENTAL_PARSER_TEST_SOURCES})

# LSP 接口用例读取仓库夹具，因而在下方配置源码根路径宏。
set(LSP_INTERFACE_TEST_SOURCES
    test_lsp_interface.c
)

add_executable(zr_vm_language_server_lsp_interface_test ${LSP_INTERFACE_TEST_SOURCES})
# 项目功能用例依赖仓库夹具；此目录目前仅传递源码根路径。
set(LSP_PROJECT_FEATURE_TEST_SOURCES
    test_lsp_project_features.c
)

add_executable(zr_vm_language_server_lsp_project_features_test ${LSP_PROJECT_FEATURE_TEST_SOURCES})
# 编辑器特性用例独立运行，便于定位接口和项目功能以外的回归。
set(LSP_ADVANCED_EDITOR_FEATURE_TEST_SOURCES
    test_lsp_advanced_editor_features.c
)

add_executable(zr_vm_language_server_lsp_advanced_editor_features_test ${LSP_ADVANCED_EDITOR_FEATURE_TEST_SOURCES})

find_program(NODE_EXECUTABLE node)

# 这些目标直接包含 LSP、parser 和 core 头文件；跨库用例还需数学、系统及容器头文件。
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
    ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
    ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
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
    ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
    ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
)

target_include_directories(zr_vm_language_server_lsp_project_features_test PRIVATE
    ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
    ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
)

target_include_directories(zr_vm_language_server_lsp_advanced_editor_features_test PRIVATE
    ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
    ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
)

target_compile_definitions(zr_vm_language_server_lsp_interface_test PRIVATE
    ZR_VM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)
# TODO: 项目用例还使用插件夹具路径；恢复本目录时应与集中注册的编译宏保持一致。
target_compile_definitions(zr_vm_language_server_lsp_project_features_test PRIVATE
    ZR_VM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)

# 链接库形态遵循父工程 BUILD_SHARED_LIB；跨库用例还需要数学、系统和容器库。
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
        zr_vm_core_shared
        zr_vm_lib_math_shared
        zr_vm_lib_system_shared
        zr_vm_lib_container_shared)
    target_link_libraries(zr_vm_language_server_incremental_parser_test PRIVATE 
        zr_vm_language_server_shared 
        zr_vm_parser_shared 
        zr_vm_core_shared)
    target_link_libraries(zr_vm_language_server_lsp_interface_test PRIVATE 
        zr_vm_language_server_shared 
        zr_vm_parser_shared 
        zr_vm_core_shared
        zr_vm_lib_math_shared
        zr_vm_lib_system_shared
        zr_vm_lib_container_shared)
    target_link_libraries(zr_vm_language_server_lsp_project_features_test PRIVATE
        zr_vm_language_server_shared
        zr_vm_parser_shared
        zr_vm_core_shared
        zr_vm_lib_math_shared
        zr_vm_lib_system_shared
        zr_vm_lib_container_shared)
    target_link_libraries(zr_vm_language_server_lsp_advanced_editor_features_test PRIVATE
        zr_vm_language_server_shared
        zr_vm_parser_shared
        zr_vm_core_shared
        zr_vm_lib_math_shared
        zr_vm_lib_system_shared
        zr_vm_lib_container_shared)
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
        zr_vm_core_static
        zr_vm_lib_math_static
        zr_vm_lib_system_static
        zr_vm_lib_container_static)
    target_link_libraries(zr_vm_language_server_incremental_parser_test PRIVATE 
        zr_vm_language_server_static 
        zr_vm_parser_static 
        zr_vm_core_static)
    target_link_libraries(zr_vm_language_server_lsp_interface_test PRIVATE 
        zr_vm_language_server_static 
        zr_vm_parser_static 
        zr_vm_core_static
        zr_vm_lib_math_static
        zr_vm_lib_system_static
        zr_vm_lib_container_static)
    target_link_libraries(zr_vm_language_server_lsp_project_features_test PRIVATE
        zr_vm_language_server_static
        zr_vm_parser_static
        zr_vm_core_static
        zr_vm_lib_math_static
        zr_vm_lib_system_static
        zr_vm_lib_container_static)
    target_link_libraries(zr_vm_language_server_lsp_advanced_editor_features_test PRIVATE
        zr_vm_language_server_static
        zr_vm_parser_static
        zr_vm_core_static
        zr_vm_lib_math_static
        zr_vm_lib_system_static
        zr_vm_lib_container_static)
endif()

# 只有父工程纳入本目录时，这些独立 CTest 才会注册；根配置已统一启用测试。
enable_testing()
add_test(NAME language_server_symbol_table_tests COMMAND zr_vm_language_server_symbol_table_test)
add_test(NAME language_server_reference_tracker_tests COMMAND zr_vm_language_server_reference_tracker_test)
add_test(NAME language_server_semantic_analyzer_tests COMMAND zr_vm_language_server_semantic_analyzer_test)
add_test(NAME language_server_incremental_parser_tests COMMAND zr_vm_language_server_incremental_parser_test)
add_test(NAME language_server_lsp_interface_tests COMMAND zr_vm_language_server_lsp_interface_test)
add_test(NAME language_server_lsp_project_features_tests COMMAND zr_vm_language_server_lsp_project_features_test)
add_test(NAME language_server_lsp_advanced_editor_features_tests COMMAND zr_vm_language_server_lsp_advanced_editor_features_test)
# Node 与 stdio 目标同时可用时，以生成的服务端可执行文件运行协议冒烟脚本。
if (NODE_EXECUTABLE AND TARGET zr_vm_language_server_stdio)
    add_test(
        NAME language_server_stdio_smoke
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/stdio_smoke.js
            $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
        NAME language_server_stdio_minimal_open_smoke
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/stdio_minimal_open_smoke.js
            $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
        NAME language_server_stdio_position_encoding_smoke
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/stdio_position_encoding_smoke.js
            $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
        NAME language_server_stdio_type_hierarchy_smoke
        COMMAND
            ${NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/stdio_type_hierarchy_smoke.js
            $<TARGET_FILE:zr_vm_language_server_stdio>
    )
endif()
