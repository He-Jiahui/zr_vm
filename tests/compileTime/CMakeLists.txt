# ZR-VM Compile-Time Execution Tests CMakeLists.txt

# 添加测试源文件
set(COMPILE_TIME_TEST_SOURCES
    test_compile_time_execution.c
)

# 创建测试可执行文件
add_executable(zr_vm_compile_time_test ${COMPILE_TIME_TEST_SOURCES})

# 链接必要的库
if (BUILD_SHARED_LIB)
    target_link_libraries(zr_vm_compile_time_test PRIVATE zr_vm_parser_shared zr_vm_core_shared zr_vm_library_shared)
else()
    target_link_libraries(zr_vm_compile_time_test PRIVATE zr_vm_parser_static zr_vm_core_static zr_vm_library_static)
endif()

# 包含头文件目录
target_include_directories(zr_vm_compile_time_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    ${CMAKE_SOURCE_DIR}/zr_vm_library/include
)

zr_configure_unity_test(zr_vm_compile_time_test)

# 添加测试目标
enable_testing()
add_test(NAME compile_time_tests COMMAND zr_vm_compile_time_test)
