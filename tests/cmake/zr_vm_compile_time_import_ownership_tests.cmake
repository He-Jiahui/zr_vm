zr_vm_add_unity_test_target(zr_vm_compile_time_import_ownership_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_compile_time_import_ownership.c)
target_include_directories(zr_vm_compile_time_import_ownership_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser)
zr_vm_link_parser_core_plus_library(zr_vm_compile_time_import_ownership_test)
add_test(NAME compile_time_import_ownership COMMAND $<TARGET_FILE:zr_vm_compile_time_import_ownership_test>)
set_tests_properties(compile_time_import_ownership PROPERTIES ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
