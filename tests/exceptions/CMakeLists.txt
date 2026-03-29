# ZR-VM Exceptions Tests CMakeLists.txt

# 添加异常测试源文件
set(EXCEPTIONS_TEST_SOURCES
    test_exceptions.c
    ${CMAKE_SOURCE_DIR}/tests/test_support.c
)

# 创建异常测试可执行文件
add_executable(zr_vm_exceptions_test ${EXCEPTIONS_TEST_SOURCES})

# 链接必要的库
if (BUILD_SHARED_LIB)
    target_link_libraries(zr_vm_exceptions_test PRIVATE zr_vm_core_shared zr_vm_parser_shared)
else()
    target_link_libraries(zr_vm_exceptions_test PRIVATE zr_vm_core_static zr_vm_parser_static)
endif()

# 包含头文件目录
target_include_directories(zr_vm_exceptions_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

zr_configure_unity_test(zr_vm_exceptions_test)

# 添加测试目标
enable_testing()
add_test(NAME exceptions_tests COMMAND zr_vm_exceptions_test)





