# ZR-VM Meta Method Tests CMakeLists.txt

# 添加元方法测试源文件
set(META_TEST_SOURCES
    test_meta.c
)

# 创建元方法测试可执行文件
add_executable(zr_vm_meta_test ${META_TEST_SOURCES})

# 包含Unity测试框架
target_include_directories(zr_vm_meta_test PRIVATE ${CMAKE_SOURCE_DIR}/third_party/zr_c_json/cJSON/tests/unity/src)

# 添加Unity源文件
set(UNITY_SOURCES
    ${CMAKE_SOURCE_DIR}/third_party/zr_c_json/cJSON/tests/unity/src/unity.c
)

# 链接必要的库
if (BUILD_SHARED_LIB)
    target_link_libraries(zr_vm_meta_test PRIVATE zr_vm_core_shared)
else()
    target_link_libraries(zr_vm_meta_test PRIVATE zr_vm_core_static)
endif()

# 添加Unity源文件到目标
target_sources(zr_vm_meta_test PRIVATE ${UNITY_SOURCES})

# 包含头文件目录
target_include_directories(zr_vm_meta_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)

# 显式启用UNITY输出
target_compile_definitions(zr_vm_meta_test PRIVATE
    UNITY_USE_FLUSH_STDOUT
)

# 添加测试目标
enable_testing()
add_test(NAME meta_tests COMMAND zr_vm_meta_test)





