zr_vm_add_unity_test_target(zr_vm_file_list_test
        ${CMAKE_SOURCE_DIR}/tests/library/test_file_list.c)
target_include_directories(zr_vm_file_list_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include)
zr_vm_link_parser_core_plus_library(zr_vm_file_list_test)
add_test(NAME file_list COMMAND $<TARGET_FILE:zr_vm_file_list_test>)
set_tests_properties(file_list PROPERTIES ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
