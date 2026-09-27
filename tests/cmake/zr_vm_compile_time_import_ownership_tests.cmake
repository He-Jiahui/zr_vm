# 编译期二进制导入的投影须在源对象释放后仍有效，拒绝与部分回收路径也须释放源对象。
# 由 tests/CMakeLists.txt 的解析器测试组包含，使用完整的 parser/core/library 链接集合。
zr_vm_add_unity_test_target(zr_vm_compile_time_import_ownership_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_compile_time_import_ownership.c)
target_include_directories(zr_vm_compile_time_import_ownership_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser)
zr_vm_link_parser_core_plus_library(zr_vm_compile_time_import_ownership_test)
add_test(NAME compile_time_import_ownership COMMAND $<TARGET_FILE:zr_vm_compile_time_import_ownership_test>)
set_tests_properties(compile_time_import_ownership PROPERTIES ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
