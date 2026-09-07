zr_vm_add_unity_test_target(zr_vm_binary_metadata_source_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_binary_metadata_source.c)
target_include_directories(zr_vm_binary_metadata_source_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser)
zr_vm_link_parser_core_plus_library(zr_vm_binary_metadata_source_test)
add_test(NAME binary_metadata_source COMMAND $<TARGET_FILE:zr_vm_binary_metadata_source_test>)
set_tests_properties(binary_metadata_source PROPERTIES ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
