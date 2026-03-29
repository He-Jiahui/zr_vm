# ZR-VM Instructions Tests CMakeLists.txt

# 添加指令测试源文件
set(INSTRUCTIONS_TEST_SOURCES
    test_instructions.c
)

# 创建指令测试可执行文件
add_executable(zr_vm_instructions_test ${INSTRUCTIONS_TEST_SOURCES})

# 链接必要的库
if (BUILD_SHARED_LIB)
    target_link_libraries(zr_vm_instructions_test PRIVATE zr_vm_core_shared)
else()
    target_link_libraries(zr_vm_instructions_test PRIVATE zr_vm_core_static)
endif()

# 包含头文件目录
target_include_directories(zr_vm_instructions_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

zr_configure_unity_test(zr_vm_instructions_test)

# 添加测试目标
enable_testing()
add_test(NAME instructions_tests COMMAND zr_vm_instructions_test)








