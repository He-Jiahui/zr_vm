# ZR-VM Parser Tests CMakeLists.txt

# 添加测试源文件
set(PARSER_TEST_SOURCES
    test_parser.c
)

# 创建测试可执行文件
add_executable(zr_vm_parser_test ${PARSER_TEST_SOURCES})

# 添加指令执行测试源文件
set(INSTRUCTION_EXECUTION_TEST_SOURCES
    test_instruction_execution.c
)

# 创建指令执行测试可执行文件
add_executable(zr_vm_instruction_execution_test ${INSTRUCTION_EXECUTION_TEST_SOURCES})

# 添加类型推断测试源文件
set(TYPE_INFERENCE_TEST_SOURCES
    test_type_inference.c
)

# 创建类型推断测试可执行文件
add_executable(zr_vm_type_inference_test ${TYPE_INFERENCE_TEST_SOURCES})

# 添加编译器功能测试源文件
set(COMPILER_FEATURES_TEST_SOURCES
    test_compiler_features.c
)

# 创建编译器功能测试可执行文件
add_executable(zr_vm_compiler_features_test ${COMPILER_FEATURES_TEST_SOURCES})

# 包含Unity测试框架
target_include_directories(zr_vm_parser_test PRIVATE ${CMAKE_SOURCE_DIR}/third_party/zr_c_json/cJSON/tests/unity/src)

# 添加Unity源文件
set(UNITY_SOURCES
    ${CMAKE_SOURCE_DIR}/third_party/zr_c_json/cJSON/tests/unity/src/unity.c
)

# 链接必要的库
if (BUILD_SHARED_LIB)
    target_link_libraries(zr_vm_parser_test PRIVATE zr_vm_parser_shared zr_vm_core_shared)
    target_link_libraries(zr_vm_instruction_execution_test PRIVATE zr_vm_parser_shared zr_vm_core_shared)
    target_link_libraries(zr_vm_type_inference_test PRIVATE zr_vm_parser_shared zr_vm_core_shared)
    target_link_libraries(zr_vm_compiler_features_test PRIVATE zr_vm_parser_shared zr_vm_core_shared)
else()
    target_link_libraries(zr_vm_parser_test PRIVATE zr_vm_parser_static zr_vm_core_static)
    target_link_libraries(zr_vm_instruction_execution_test PRIVATE zr_vm_parser_static zr_vm_core_static)
    target_link_libraries(zr_vm_type_inference_test PRIVATE zr_vm_parser_static zr_vm_core_static)
    target_link_libraries(zr_vm_compiler_features_test PRIVATE zr_vm_parser_static zr_vm_core_static)
endif()

# 添加Unity源文件到目标
target_sources(zr_vm_parser_test PRIVATE ${UNITY_SOURCES})
target_sources(zr_vm_instruction_execution_test PRIVATE ${UNITY_SOURCES})
target_sources(zr_vm_type_inference_test PRIVATE ${UNITY_SOURCES})
target_sources(zr_vm_compiler_features_test PRIVATE ${UNITY_SOURCES})

# 包含Unity测试框架
target_include_directories(zr_vm_instruction_execution_test PRIVATE ${CMAKE_SOURCE_DIR}/third_party/zr_c_json/cJSON/tests/unity/src)
target_include_directories(zr_vm_type_inference_test PRIVATE ${CMAKE_SOURCE_DIR}/third_party/zr_c_json/cJSON/tests/unity/src)
target_include_directories(zr_vm_compiler_features_test PRIVATE ${CMAKE_SOURCE_DIR}/third_party/zr_c_json/cJSON/tests/unity/src)

# 包含头文件目录
target_include_directories(zr_vm_parser_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

target_include_directories(zr_vm_instruction_execution_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

target_include_directories(zr_vm_type_inference_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

target_include_directories(zr_vm_compiler_features_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

# 显式启用UNITY输出
target_compile_definitions(zr_vm_parser_test PRIVATE
    UNITY_USE_FLUSH_STDOUT
)

target_compile_definitions(zr_vm_instruction_execution_test PRIVATE
    UNITY_USE_FLUSH_STDOUT
)

target_compile_definitions(zr_vm_type_inference_test PRIVATE
    UNITY_USE_FLUSH_STDOUT
)

target_compile_definitions(zr_vm_compiler_features_test PRIVATE
    UNITY_USE_FLUSH_STDOUT
)

# 复制测试文件到构建目录的多个位置，确保测试可执行文件可以找到
# 1. 复制到当前构建目录（tests/parser/）
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_simple.zr
    ${CMAKE_CURRENT_BINARY_DIR}/test_simple.zr
    COPYONLY
)

# 2. 复制到 bin 目录（可执行文件运行的位置）
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_simple.zr
    ${CMAKE_BINARY_DIR}/bin/test_simple.zr
    COPYONLY
)

# 3. 复制到 tests/parser 目录结构（用于 tests/parser/test_simple.zr 路径）
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/parser)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_simple.zr
    ${CMAKE_BINARY_DIR}/tests/parser/test_simple.zr
    COPYONLY
)

# 添加测试目标
enable_testing()
add_test(NAME parser_tests COMMAND zr_vm_parser_test)
add_test(NAME instruction_execution_tests COMMAND zr_vm_instruction_execution_test)
add_test(NAME type_inference_tests COMMAND zr_vm_type_inference_test)
add_test(NAME compiler_features_tests COMMAND zr_vm_compiler_features_test)

