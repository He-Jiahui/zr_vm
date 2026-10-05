# Real source -> SemIR -> ExecIR -> ExecBC VM materialization integration.
if (NOT TARGET zr_vm_ssa_source_execbc_vm_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_source_execbc_vm_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_source_execbc_vm.c)
    target_include_directories(zr_vm_ssa_source_execbc_vm_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_source_execbc_vm_test)
    if (MSVC)
        # The Debug core dispatcher frame exceeds the default Windows reserve.
        target_link_options(zr_vm_ssa_source_execbc_vm_test PRIVATE /STACK:8388608)
    endif ()
    add_test(NAME ssa_source_execbc_vm
            COMMAND zr_vm_ssa_source_execbc_vm_test)
    set_tests_properties(ssa_source_execbc_vm PROPERTIES
            LABELS "ssa" TIMEOUT 120)
endif ()

if (NOT TARGET zr_vm_ssa_source_script_entry_tokens_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_source_script_entry_tokens_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_source_script_entry_tokens.c)
    target_include_directories(zr_vm_ssa_source_script_entry_tokens_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_source_script_entry_tokens_test)
    if (MSVC)
        target_link_options(zr_vm_ssa_source_script_entry_tokens_test PRIVATE /STACK:8388608)
    endif ()
    add_test(NAME ssa_source_script_entry_tokens
            COMMAND zr_vm_ssa_source_script_entry_tokens_test)
    set_tests_properties(ssa_source_script_entry_tokens PROPERTIES
            LABELS "ssa" TIMEOUT 120)
endif ()

# Ordinary Source_Compile publication of the existing callable return fields.
if (NOT TARGET zr_vm_ssa_source_callable_return_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_source_callable_return_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_source_callable_return.c)
    target_include_directories(zr_vm_ssa_source_callable_return_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_source_callable_return_test)
    if (MSVC)
        target_link_options(zr_vm_ssa_source_callable_return_test PRIVATE /STACK:8388608)
    endif ()
    add_test(NAME ssa_source_callable_return
            COMMAND zr_vm_ssa_source_callable_return_test)
    set_tests_properties(ssa_source_callable_return PROPERTIES
            LABELS "ssa" TIMEOUT 120)
endif ()

# Declaration-backed callable identity after ordinary Source_Compile.
if (NOT TARGET zr_vm_ssa_source_callable_identity_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_source_callable_identity_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_source_callable_identity.c)
    target_include_directories(zr_vm_ssa_source_callable_identity_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_source_callable_identity_test)
    if (MSVC)
        target_link_options(zr_vm_ssa_source_callable_identity_test PRIVATE /STACK:8388608)
    endif ()
    add_test(NAME ssa_source_callable_identity
            COMMAND zr_vm_ssa_source_callable_identity_test)
    set_tests_properties(ssa_source_callable_identity PROPERTIES
            LABELS "ssa" TIMEOUT 120)
endif ()

# RED contract for the canonical identity of an ordinary script entry.
if (NOT TARGET zr_vm_ssa_source_script_entry_identity_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_source_script_entry_identity_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_source_script_entry_identity.c)
    target_include_directories(zr_vm_ssa_source_script_entry_identity_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_source_script_entry_identity_test)
    if (MSVC)
        target_link_options(zr_vm_ssa_source_script_entry_identity_test PRIVATE /STACK:8388608)
    endif ()
    add_test(NAME ssa_source_script_entry_identity
            COMMAND zr_vm_ssa_source_script_entry_identity_test)
    set_tests_properties(ssa_source_script_entry_identity PROPERTIES
            LABELS "ssa" TIMEOUT 120)
endif ()
