zr_vm_add_unity_test_target(
        zr_vm_language_server_semantic_declaration_budget_test
        ${CMAKE_SOURCE_DIR}/tests/language_server/test_semantic_declaration_budget.c
)
target_include_directories(zr_vm_language_server_semantic_declaration_budget_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
)
zr_vm_link_language_server(zr_vm_language_server_semantic_declaration_budget_test)
add_test(
        NAME language_server_semantic_declaration_budget
        COMMAND $<TARGET_FILE:zr_vm_language_server_semantic_declaration_budget_test>
)
set_tests_properties(language_server_semantic_declaration_budget PROPERTIES
        TIMEOUT 30
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1"
)
