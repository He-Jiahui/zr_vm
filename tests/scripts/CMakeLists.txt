# ZR-VM Scripts Comprehensive Tests CMakeLists.txt

# 添加测试源文件
set(SCRIPTS_TEST_SOURCES
    test_comprehensive.c
    test_utils.c
    ${CMAKE_SOURCE_DIR}/third_party/zr_c_json/cJSON/cJSON.c
)

# 创建测试可执行文件
add_executable(zr_vm_scripts_test ${SCRIPTS_TEST_SOURCES})

# 包含Unity测试框架
target_include_directories(zr_vm_scripts_test PRIVATE ${CMAKE_SOURCE_DIR}/third_party/zr_c_json/cJSON/tests/unity/src)

# 添加Unity源文件
set(UNITY_SOURCES
    ${CMAKE_SOURCE_DIR}/third_party/zr_c_json/cJSON/tests/unity/src/unity.c
)

# 链接必要的库
if (BUILD_SHARED_LIB)
    target_link_libraries(zr_vm_scripts_test PRIVATE zr_vm_parser_shared zr_vm_core_shared)
else()
    target_link_libraries(zr_vm_scripts_test PRIVATE zr_vm_parser_static zr_vm_core_static)
endif()

# 添加Unity源文件到目标
target_sources(zr_vm_scripts_test PRIVATE ${UNITY_SOURCES})

# 包含头文件目录
target_include_directories(zr_vm_scripts_test PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    ${CMAKE_SOURCE_DIR}/third_party/zr_c_json/cJSON
)

# 显式启用UNITY输出
target_compile_definitions(zr_vm_scripts_test PRIVATE
    UNITY_USE_FLUSH_STDOUT
)

# 复制测试用例文件到构建目录
file(GLOB TEST_CASE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/test_cases/*.zr")
foreach(TEST_FILE ${TEST_CASE_FILES})
    get_filename_component(TEST_FILE_NAME ${TEST_FILE} NAME)
    # 复制到当前构建目录
    configure_file(
        ${TEST_FILE}
        ${CMAKE_CURRENT_BINARY_DIR}/${TEST_FILE_NAME}
        COPYONLY
    )
    # 复制到 bin 目录
    configure_file(
        ${TEST_FILE}
        ${CMAKE_BINARY_DIR}/bin/${TEST_FILE_NAME}
        COPYONLY
    )
    # 复制到 tests/scripts/test_cases 目录结构
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/scripts/test_cases)
    configure_file(
        ${TEST_FILE}
        ${CMAKE_BINARY_DIR}/tests/scripts/test_cases/${TEST_FILE_NAME}
        COPYONLY
    )
endforeach()

# 创建输出目录
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/scripts/output/ast)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/scripts/output/intermediate)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/scripts/output/runtime)

# 添加测试目标
enable_testing()
add_test(NAME scripts_tests COMMAND zr_vm_scripts_test)

