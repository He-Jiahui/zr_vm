# ZR-VM GC Tests CMakeLists.txt

# 添加测试源文件
set(GC_TEST_SOURCES
    gc_tests.c
    gc_test_utils.c
)

# 创建测试可执行文件
add_executable(zr_vm_gc_test ${GC_TEST_SOURCES})

# 链接必要的库
if (BUILD_SHARED_LIB)
    target_link_libraries(zr_vm_gc_test PRIVATE zr_vm_core_shared)
else()
    target_link_libraries(zr_vm_gc_test PRIVATE zr_vm_core_static)
endif()

# 包含头文件目录
target_include_directories(zr_vm_gc_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

zr_configure_unity_test(zr_vm_gc_test)

# 添加测试目标
enable_testing()
add_test(NAME gc_tests COMMAND zr_vm_gc_test)


