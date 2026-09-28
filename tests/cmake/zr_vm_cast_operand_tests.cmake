zr_vm_add_unity_test_target(zr_vm_cast_operand_facts_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_cast_operand_facts.c)
zr_vm_link_parser_core_plus_library(zr_vm_cast_operand_facts_test)
add_test(NAME cast_operand_facts COMMAND $<TARGET_FILE:zr_vm_cast_operand_facts_test>)
# 本片段只由 LSP 目标门控 include：先锁定 parser cast 操作数事实，再验证 LSP 查询投影。
# TODO: 纯 parser 构建会连 parser 独立回归一起跳过；需核对测试分层是否有意如此。
zr_vm_add_unity_test_target(zr_vm_language_server_cast_operand_facts_test
        ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_cast_operand_facts.c)
target_include_directories(zr_vm_language_server_cast_operand_facts_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server)
zr_vm_link_language_server(zr_vm_language_server_cast_operand_facts_test)
add_test(NAME language_server_cast_operand_facts
        COMMAND $<TARGET_FILE:zr_vm_language_server_cast_operand_facts_test>)
# 两个层级的同一事实回归都在检测到未定义行为时立即失败，避免依赖 CTest 输出解析。
set_tests_properties(cast_operand_facts language_server_cast_operand_facts PROPERTIES
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
