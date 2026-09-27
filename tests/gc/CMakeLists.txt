# GC 测试的旧目录级入口；当前顶层 tests/CMakeLists.txt 直接注册同名目标。
# TODO: 若再次启用目录级构建，先核对与顶层目标及 Unity 链接规则是否重复。

# 添加测试源文件
set(GC_TEST_SOURCES
    gc_tests.c
    gc_test_utils.c
)

# 创建测试可执行文件
add_executable(zr_vm_gc_test ${GC_TEST_SOURCES})

# TODO: 测试需要 library 头类型与 lib_system GC API；启用本目录前核对传递链接闭包与头目录。
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

# TODO: 仓库内没有 zr_configure_unity_test 定义；启用本目录前须替换，否则配置报 unknown command。
zr_configure_unity_test(zr_vm_gc_test)

# 添加测试目标
enable_testing()
add_test(NAME gc_tests COMMAND zr_vm_gc_test)


