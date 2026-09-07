zr_vm_add_unity_test_target(zr_vm_cast_operand_facts_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_cast_operand_facts.c)
zr_vm_link_parser_core_plus_library(zr_vm_cast_operand_facts_test)
add_test(NAME cast_operand_facts COMMAND $<TARGET_FILE:zr_vm_cast_operand_facts_test>)

zr_vm_add_unity_test_target(zr_vm_language_server_cast_operand_facts_test
        ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_cast_operand_facts.c)
target_include_directories(zr_vm_language_server_cast_operand_facts_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server)
zr_vm_link_language_server(zr_vm_language_server_cast_operand_facts_test)
add_test(NAME language_server_cast_operand_facts
        COMMAND $<TARGET_FILE:zr_vm_language_server_cast_operand_facts_test>)
set_tests_properties(cast_operand_facts language_server_cast_operand_facts PROPERTIES
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
