zr_vm_add_unity_test_target(zr_vm_compile_time_test
        ${CMAKE_SOURCE_DIR}/tests/compileTime/test_compile_time_execution.c)
zr_vm_link_parser_core_plus_library(zr_vm_compile_time_test)
add_test(NAME compile_time_execution COMMAND $<TARGET_FILE:zr_vm_compile_time_test>)

zr_vm_add_unity_test_target(zr_vm_comptime_contract_test
        ${CMAKE_SOURCE_DIR}/tests/compileTime/test_comptime_contract.c
        ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c)
zr_vm_link_parser_core_plus_library(zr_vm_comptime_contract_test)
add_test(NAME comptime_contract COMMAND $<TARGET_FILE:zr_vm_comptime_contract_test>)

zr_vm_add_unity_test_target(zr_vm_comptime_runtime_contract_test
        ${CMAKE_SOURCE_DIR}/tests/compileTime/test_comptime_runtime_contract.c)
target_include_directories(zr_vm_comptime_runtime_contract_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler)
zr_vm_link_parser_core_plus_library(zr_vm_comptime_runtime_contract_test)
add_test(NAME comptime_runtime_contract COMMAND $<TARGET_FILE:zr_vm_comptime_runtime_contract_test>)

zr_vm_add_unity_test_target(zr_vm_declaration_transform_contract_test
        ${CMAKE_SOURCE_DIR}/tests/compileTime/test_declaration_transform_contract.c
        ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c)
zr_vm_link_parser_core_plus_library(zr_vm_declaration_transform_contract_test)
add_test(NAME declaration_transform_contract
        COMMAND $<TARGET_FILE:zr_vm_declaration_transform_contract_test>)

zr_vm_add_unity_test_target(zr_vm_decorator_pipeline_test
        ${CMAKE_SOURCE_DIR}/tests/decorators/test_compile_time_member_decorators.c)
zr_vm_link_parser_core_plus_library(zr_vm_decorator_pipeline_test)
add_test(NAME decorator_pipeline COMMAND $<TARGET_FILE:zr_vm_decorator_pipeline_test>)

set_tests_properties(compile_time_execution comptime_contract
        comptime_runtime_contract declaration_transform_contract decorator_pipeline
        PROPERTIES TIMEOUT 60 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1")
