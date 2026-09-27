# ZR-VM Module System Tests CMakeLists.txt
# 当前根 tests/CMakeLists.txt 直接注册 zr_vm_module_system_test，未包含此文件。
# 以下为独立子目录构建入口；其链接和包含路径不能作为当前 CTest 证据。

# 添加模块系统测试源文件
set(MODULE_TEST_SOURCES
    test_module_system.c
)

# 创建模块系统测试可执行文件
add_executable(zr_vm_module_system_test ${MODULE_TEST_SOURCES})

# 链接必要的库
if (BUILD_SHARED_LIB)
    target_link_libraries(zr_vm_module_system_test PRIVATE zr_vm_parser_shared zr_vm_core_shared)
else()
    target_link_libraries(zr_vm_module_system_test PRIVATE zr_vm_parser_static zr_vm_core_static)
endif()

# 包含头文件目录
target_include_directories(zr_vm_module_system_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

# TODO: 仓库内无 zr_configure_unity_test 定义；若重新纳入子目录构建，
# 先提供此 helper 或按根 tests/CMakeLists.txt 的现行路径配置，否则 CMake 配置会在此处失败。
zr_configure_unity_test(zr_vm_module_system_test)

# 添加测试目标
enable_testing()
add_test(NAME module_system_tests COMMAND zr_vm_module_system_test)
