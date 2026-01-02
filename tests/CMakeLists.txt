# ZR-VM Tests CMakeLists.txt

# 添加测试源文件
set(TEST_SOURCES
gc_tests.c
gc_test_utils.c
)

# 创建测试可执行文件
add_executable(${PROJECT_NAME}_test ${TEST_SOURCES})

# 包含Unity测试框架
target_include_directories(${PROJECT_NAME}_test PRIVATE ${CMAKE_SOURCE_DIR}/third_party/zr_c_json/cJSON/tests/unity/src)

# 添加Unity源文件
set(UNITY_SOURCES
    ${CMAKE_SOURCE_DIR}/third_party/zr_c_json/cJSON/tests/unity/src/unity.c
)

# 链接必要的库
target_link_libraries(${PROJECT_NAME}_test PRIVATE zr_vm_core_shared)

# 添加Unity源文件到目标
target_sources(${PROJECT_NAME}_test PRIVATE ${UNITY_SOURCES})

# 包含头文件目录
target_include_directories(${PROJECT_NAME}_test PRIVATE 
${CMAKE_SOURCE_DIR}/zr_vm_core/include
${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

# 显式启用UNITY输出
# 确保UNITY输出到标准输出(stdout)并启用缓冲区刷新
# UNITY_OUTPUT_CHAR默认使用putchar输出到stdout
# 定义UNITY_USE_FLUSH_STDOUT以启用UNITY_OUTPUT_FLUSH()来刷新输出缓冲区
target_compile_definitions(${PROJECT_NAME}_test PRIVATE
    UNITY_USE_FLUSH_STDOUT
)

# 添加测试目标
enable_testing()
add_test(NAME gc_tests COMMAND ${PROJECT_NAME}_test)