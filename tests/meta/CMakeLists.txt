# 历史目录级配方；当前由 tests/CMakeLists.txt 直接创建同名目标并纳入 core_runtime，
# 顶层没有 add_subdirectory(meta)。重新启用前需先合并两处目标及测试注册。

# 添加元方法测试源文件
set(META_TEST_SOURCES
    test_meta.c
)

# 创建元方法测试可执行文件
add_executable(zr_vm_meta_test ${META_TEST_SOURCES})

# 链接必要的库
if (BUILD_SHARED_LIB)
    target_link_libraries(zr_vm_meta_test PRIVATE zr_vm_core_shared)
else()
    target_link_libraries(zr_vm_meta_test PRIVATE zr_vm_core_static)
endif()

# 包含头文件目录
target_include_directories(zr_vm_meta_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

# TODO: 仓库中没有 zr_configure_unity_test 定义；若启用本目录，需改用顶层
# zr_vm_add_unity_test_target 的 Unity 头目录、配置宏和链接规则。
zr_configure_unity_test(zr_vm_meta_test)

# 添加测试目标
enable_testing()
add_test(NAME meta_tests COMMAND zr_vm_meta_test)








