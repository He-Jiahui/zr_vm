    zr_vm_add_unity_test_target(
            zr_vm_aot_c_typed_call_contracts_test
            ${zr_vm_aot_c_typed_call_contract_sources}
    )
    target_include_directories(zr_vm_aot_c_typed_call_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_c_typed_call_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_shared_library_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_shared_library_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_reflection_construction_shared_library_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_reflection_construction_shared_library.c
    )
    target_include_directories(zr_vm_aot_c_reflection_construction_shared_library_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_reflection_construction_shared_library_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(
            zr_vm_aot_c_reflection_construction_shared_library_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_metadata_binding_loader_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_metadata_binding_loader.c
    )
    target_include_directories(zr_vm_aot_c_metadata_binding_loader_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_metadata_binding_loader_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_metadata_binding_loader_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_metadata_binding_loader_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_descriptor_diagnostics_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_descriptor_diagnostics.c
    )
    target_include_directories(zr_vm_aot_c_descriptor_diagnostics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_descriptor_diagnostics_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_descriptor_diagnostics_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_control_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_control_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_control_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_control_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_control_shared_library_smoke_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_receiver_guard_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_receiver_guard_shared_library.c
    )
    target_include_directories(zr_vm_aot_receiver_guard_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_receiver_guard_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_LLVM_COMPILER="clang"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_receiver_guard_shared_library_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_receiver_guard_shared_library_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_call_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_call_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_call_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_call_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_call_shared_library_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_call_shared_library_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_provider_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_provider_shared_library_smoke.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.c
    )
    target_include_directories(zr_vm_aot_c_provider_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/include
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_provider_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_provider_shared_library_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_provider_shared_library_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_dynamic_deopt_bridge_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c
    )
    target_include_directories(zr_vm_aot_c_dynamic_deopt_bridge_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_dynamic_deopt_bridge_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_dynamic_deopt_bridge_smoke_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_typed_direct_call_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_typed_direct_call_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_typed_direct_call_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_typed_direct_call_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_typed_direct_call_shared_library_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_typed_direct_call_shared_library_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_value_construction_guardrail_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_value_construction_guardrail.c
    )
    target_include_directories(zr_vm_aot_c_value_construction_guardrail_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_value_construction_guardrail_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_C_FLAGS="${CMAKE_C_FLAGS}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_value_construction_guardrail_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_value_construction_guardrail_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/tests/parser
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_typed_direct_call_bool_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/tests/parser
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/tests/parser
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/tests/parser
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_typed_direct_call_arithmetic_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_typed_direct_call_bitwise_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_value_type_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_value_type_shared_library_smoke.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
    )
    target_include_directories(zr_vm_aot_c_value_type_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/tests/container
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_iteration/include
    )
    target_compile_definitions(zr_vm_aot_c_value_type_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_value_type_shared_library_smoke_test)
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_aot_c_value_type_shared_library_smoke_test PRIVATE zr_vm_lib_iteration_shared)
    else ()
        target_link_libraries(zr_vm_aot_c_value_type_shared_library_smoke_test PRIVATE zr_vm_lib_iteration_static)
    endif ()
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_value_type_shared_library_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_logical_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_logical_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_logical_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_logical_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_logical_shared_library_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_logical_shared_library_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_generic_bool_equality_local_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_generic_bool_equality_local_smoke.c
    )
    target_include_directories(zr_vm_aot_c_generic_bool_equality_local_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_generic_bool_equality_local_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_generic_bool_equality_local_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_generic_bool_equality_local_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_generic_equality_stack_copy_local_smoke.c
    )
    target_include_directories(zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_generic_not_equal_stack_copy_jump_if_local_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_generic_not_equal_stack_copy_jump_if_local_smoke.c
    )
    target_include_directories(zr_vm_aot_c_generic_not_equal_stack_copy_jump_if_local_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_generic_not_equal_stack_copy_jump_if_local_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_generic_not_equal_stack_copy_jump_if_local_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_generic_not_equal_stack_copy_jump_if_local_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_generic_call_result_stack_copy_equality_local_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_generic_call_result_stack_copy_equality_local_smoke.c
    )
    target_include_directories(zr_vm_aot_c_generic_call_result_stack_copy_equality_local_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_generic_call_result_stack_copy_equality_local_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_generic_call_result_stack_copy_equality_local_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_generic_call_result_stack_copy_equality_local_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_generic_jump_if_bool_local_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_generic_jump_if_bool_local_smoke.c
    )
    target_include_directories(zr_vm_aot_c_generic_jump_if_bool_local_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_generic_jump_if_bool_local_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_generic_jump_if_bool_local_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_generic_jump_if_bool_local_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_generic_logical_not_numeric_local_smoke.c
    )
    target_include_directories(zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test)
    if (UNIX AND NOT APPLE)
        target_link_libraries(zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test PRIVATE dl)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_global_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_global_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_global_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_global_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_global_shared_library_smoke_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_ownership_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_ownership_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_ownership_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_ownership_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_ownership_shared_library_smoke_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_super_array_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_super_array_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_super_array_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_super_array_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_super_array_shared_library_smoke_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_iterator_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_iterator_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_iterator_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_iterator_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_iterator_shared_library_smoke_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_scope_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_scope_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_scope_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_scope_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_scope_shared_library_smoke_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_float_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_float_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_float_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_float_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_float_shared_library_smoke_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_generic_numeric_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_generic_numeric_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_generic_numeric_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_generic_numeric_shared_library_smoke_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_generic_monomorphization_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_generic_monomorphization.c
    )
    target_include_directories(zr_vm_aot_c_generic_monomorphization_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_generic_monomorphization_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_generic_monomorphization_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_generic_reference_sharing_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_generic_reference_sharing.c
    )
    target_include_directories(zr_vm_aot_c_generic_reference_sharing_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_generic_reference_sharing_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_generic_reference_sharing_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_generic_call_typed_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_generic_call_typed.c
    )
    target_include_directories(zr_vm_aot_c_generic_call_typed_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_generic_call_typed_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_generic_call_typed_test)

    zr_vm_add_unity_test_target(
            zr_vm_aot_c_power_shared_library_smoke_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_aot_c_power_shared_library_smoke.c
    )
    target_include_directories(zr_vm_aot_c_power_shared_library_smoke_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_compile_definitions(zr_vm_aot_c_power_shared_library_smoke_test PRIVATE
            ZR_VM_TESTS_C_COMPILER="${CMAKE_C_COMPILER}"
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
            ZR_VM_TESTS_BUILD_LIB_DIR="${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_aot_c_power_shared_library_smoke_test)

    zr_vm_add_unity_test_target(
            zr_vm_typed_bool_logical_not_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_typed_bool_logical_not.c
    )
    target_include_directories(zr_vm_typed_bool_logical_not_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_typed_bool_logical_not_test)

    zr_vm_add_unity_test_target(
            zr_vm_typed_bool_conditional_jump_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_typed_bool_conditional_jump.c
    )
    target_include_directories(zr_vm_typed_bool_conditional_jump_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_typed_bool_conditional_jump_test)

    zr_vm_add_unity_test_target(
            zr_vm_typed_numeric_neg_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_typed_numeric_neg.c
    )
    target_include_directories(zr_vm_typed_numeric_neg_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_typed_numeric_neg_test)

    zr_vm_add_unity_test_target(
            zr_vm_typed_numeric_conversion_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_typed_numeric_conversion.c
    )
    target_include_directories(zr_vm_typed_numeric_conversion_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_typed_numeric_conversion_test)

    zr_vm_add_unity_test_target(
            zr_vm_value_type_runtime_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_value_type_runtime.c
    )
    target_include_directories(zr_vm_value_type_runtime_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_value_type_runtime_test)

    zr_vm_add_unity_test_target(
            zr_vm_debug_metadata_test
            ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_metadata.c
    )
    target_include_directories(zr_vm_debug_metadata_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_debug_metadata_test)

    zr_vm_add_unity_test_target(
            zr_vm_debug_trace_test
            ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_trace.c
    )
    target_include_directories(zr_vm_debug_trace_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_debug_trace_test)

    zr_vm_add_unity_test_target(
            zr_vm_debug_traceback_test
            ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_traceback.c
    )
    target_include_directories(zr_vm_debug_traceback_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_debug_traceback_test)

    zr_vm_add_unity_test_target(
            zr_vm_debug_disassemble_test
            ${CMAKE_SOURCE_DIR}/tests/debug/test_disassemble.c
    )
    target_include_directories(zr_vm_debug_disassemble_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_debug_disassemble_test)

    zr_vm_add_unity_test_target(
            zr_vm_debug_heap_summary_test
            ${CMAKE_SOURCE_DIR}/tests/debug/test_heap_summary.c
    )
    target_include_directories(zr_vm_debug_heap_summary_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_debug_heap_summary_test)

    zr_vm_add_unity_test_target(
            zr_vm_debug_hook_core_test
            ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_hook_core.c
    )
    target_include_directories(zr_vm_debug_hook_core_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_debug_hook_core_test)

