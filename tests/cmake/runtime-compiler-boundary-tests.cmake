# Runtime, parser, AOT and CLI boundary regressions.
if (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static)
    zr_vm_add_unity_test_target(zr_vm_execution_integer_boundaries_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_execution_integer_boundaries.c)
    target_include_directories(zr_vm_execution_integer_boundaries_test PRIVATE ${CMAKE_SOURCE_DIR})
    zr_vm_link_core(zr_vm_execution_integer_boundaries_test)
    add_test(NAME execution_integer_boundaries COMMAND zr_vm_execution_integer_boundaries_test)
    zr_vm_add_unity_test_target(zr_vm_typed_opcode_operands_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_typed_opcode_operands.c)
    target_include_directories(zr_vm_typed_opcode_operands_test PRIVATE ${CMAKE_SOURCE_DIR})
    zr_vm_link_core(zr_vm_typed_opcode_operands_test)
    add_test(NAME typed_opcode_operands COMMAND zr_vm_typed_opcode_operands_test)
    set_tests_properties(typed_opcode_operands PROPERTIES TIMEOUT 30)
endif()

if ((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND UNIX AND NOT APPLE)
    zr_vm_add_unity_test_target(zr_vm_aot_integer_boundaries_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_integer_boundaries.c)
    target_include_directories(zr_vm_aot_integer_boundaries_test PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_compile_definitions(zr_vm_aot_integer_boundaries_test PRIVATE
        ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
        ZR_VM_TESTS_C_FLAGS="${CMAKE_C_FLAGS}"
        ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
        ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}")
    zr_vm_link_parser_core_plus_library(zr_vm_aot_integer_boundaries_test)
    target_link_libraries(zr_vm_aot_integer_boundaries_test PRIVATE dl)
    add_test(NAME aot_integer_boundaries COMMAND zr_vm_aot_integer_boundaries_test)
    set_tests_properties(aot_integer_boundaries PROPERTIES TIMEOUT 120)
endif()

if (TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static)
    zr_vm_add_unity_test_target(zr_vm_binary_loader_rejection_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_binary_loader_rejection.c)
    target_include_directories(zr_vm_binary_loader_rejection_test PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    zr_vm_link_parser_core(zr_vm_binary_loader_rejection_test)
    add_test(NAME binary_loader_rejection COMMAND zr_vm_binary_loader_rejection_test)
    set_tests_properties(binary_loader_rejection PROPERTIES TIMEOUT 60)
    zr_vm_add_unity_test_target(zr_vm_parser_nesting_limit_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_parser_nesting_limit.c)
    target_include_directories(zr_vm_parser_nesting_limit_test PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    zr_vm_link_parser_core(zr_vm_parser_nesting_limit_test)
    add_test(NAME parser_nesting_limit COMMAND zr_vm_parser_nesting_limit_test)
    set_tests_properties(parser_nesting_limit PROPERTIES TIMEOUT 30)
    zr_vm_add_unity_test_target(zr_vm_flat_expression_compile_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_flat_expression_compile.c)
    target_include_directories(zr_vm_flat_expression_compile_test PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    zr_vm_link_parser_core(zr_vm_flat_expression_compile_test)
    add_test(NAME flat_expression_compile COMMAND zr_vm_flat_expression_compile_test)
    set_tests_properties(flat_expression_compile PROPERTIES TIMEOUT 20)
    zr_vm_add_unity_test_target(zr_vm_compiled_function_lifetime_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_compiled_function_lifetime.c)
    target_include_directories(zr_vm_compiled_function_lifetime_test PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    zr_vm_link_parser_core(zr_vm_compiled_function_lifetime_test)
    add_test(NAME compiled_function_lifetime COMMAND zr_vm_compiled_function_lifetime_test)
    set_tests_properties(compiled_function_lifetime PROPERTIES TIMEOUT 20)
endif()

if(TARGET zr_vm_cli_executable)
    add_test(NAME native_lambda_loops
            COMMAND ${CMAKE_COMMAND} "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_native_lambda_loops.cmake")
    set_tests_properties(native_lambda_loops PROPERTIES TIMEOUT 60)
    add_test(NAME cli_binary_freshness
            COMMAND ${CMAKE_COMMAND} "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
            "-DTEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_binary_freshness.cmake")
    set_tests_properties(cli_binary_freshness PROPERTIES TIMEOUT 120)
    add_test(NAME compiler_constructor_prototype_growth
            COMMAND ${CMAKE_COMMAND}
            "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
            "-DSOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser/constructor_prototype_growth.zr"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_constructor_prototype_growth.cmake")
    set_tests_properties(compiler_constructor_prototype_growth PROPERTIES TIMEOUT 40)
    add_test(NAME runtime_array_bounds
            COMMAND ${CMAKE_COMMAND} "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_array_bounds.cmake")
    set_tests_properties(runtime_array_bounds PROPERTIES TIMEOUT 90)
endif()
