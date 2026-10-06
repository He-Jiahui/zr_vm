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

# Parse-only SCRIPT/RETURN source range and CRLF token boundary contract.
if (NOT TARGET zr_vm_ssa_source_range_identity_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_source_range_identity_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_source_range_identity.c)
    target_include_directories(zr_vm_ssa_source_range_identity_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_source_range_identity_test)
    if (MSVC)
        target_link_options(zr_vm_ssa_source_range_identity_test PRIVATE /STACK:8388608)
    endif ()
    add_test(NAME ssa_source_range_identity
            COMMAND zr_vm_ssa_source_range_identity_test)
    set_tests_properties(ssa_source_range_identity PROPERTIES
            LABELS "ssa" TIMEOUT 120)
endif ()

# Actual template AST roots retain their saved token range after lexer advance.
if (NOT TARGET zr_vm_template_literal_source_range_test)
    zr_vm_add_unity_test_target(zr_vm_template_literal_source_range_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_template_literal_source_range.c)
    target_include_directories(zr_vm_template_literal_source_range_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    zr_vm_link_parser_core_plus_library(zr_vm_template_literal_source_range_test)
    if (MSVC)
        target_link_options(zr_vm_template_literal_source_range_test PRIVATE /STACK:8388608)
    endif ()
    add_test(NAME template_literal_source_range
            COMMAND zr_vm_template_literal_source_range_test)
    set_tests_properties(template_literal_source_range PROPERTIES
            LABELS "parser;ssa" TIMEOUT 120)
endif ()

# The ordinary build already defines this unchanged parser consumer target.
if (TARGET zr_vm_expression_fragment_parser_test AND NOT TEST expression_fragment_parser)
    add_test(NAME expression_fragment_parser
            COMMAND zr_vm_expression_fragment_parser_test)
    set_tests_properties(expression_fragment_parser PROPERTIES
            LABELS "parser;ssa" TIMEOUT 120)
endif ()

# Shared CoreExecIR compaction of proved inert source temporary places.
if (NOT TARGET zr_vm_ssa_dead_source_places_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_dead_source_places_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_dead_source_places.c)
    target_sources(zr_vm_ssa_dead_source_places_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/parser/support/ssa_literal_script_fixture.c)
    target_include_directories(zr_vm_ssa_dead_source_places_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_dead_source_places_test)
    if (MSVC)
        target_link_options(zr_vm_ssa_dead_source_places_test PRIVATE /STACK:8388608)
    endif ()
    add_test(NAME ssa_dead_source_places
            COMMAND zr_vm_ssa_dead_source_places_test)
    set_tests_properties(ssa_dead_source_places PROPERTIES
            LABELS "ssa" TIMEOUT 120)
endif ()

# Explicit host i64 storage rows from the actual canonical literal fixture.
if (NOT TARGET zr_vm_ssa_host_primitive_layout_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_host_primitive_layout_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_host_primitive_layout.c)
    target_sources(zr_vm_ssa_host_primitive_layout_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/parser/support/ssa_literal_script_fixture.c)
    target_include_directories(zr_vm_ssa_host_primitive_layout_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_host_primitive_layout_test)
    if (MSVC)
        target_link_options(zr_vm_ssa_host_primitive_layout_test PRIVATE /STACK:8388608)
    endif ()
    add_test(NAME ssa_host_primitive_layout
            COMMAND zr_vm_ssa_host_primitive_layout_test)
    set_tests_properties(ssa_host_primitive_layout PROPERTIES
            LABELS "ssa" TIMEOUT 120)
endif ()

# Limited host AOT target from actual canonical no-argument i64 source facts.
if (NOT TARGET zr_vm_ssa_host_noargs_i64_aot_target_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_host_noargs_i64_aot_target_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_host_noargs_i64_aot_target.c)
    target_sources(zr_vm_ssa_host_noargs_i64_aot_target_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/parser/support/ssa_literal_script_fixture.c)
    target_include_directories(zr_vm_ssa_host_noargs_i64_aot_target_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_host_noargs_i64_aot_target_test)
    if (MSVC)
        target_link_options(zr_vm_ssa_host_noargs_i64_aot_target_test PRIVATE /STACK:8388608)
    endif ()
    add_test(NAME ssa_host_noargs_i64_aot_target
            COMMAND zr_vm_ssa_host_noargs_i64_aot_target_test)
    set_tests_properties(ssa_host_noargs_i64_aot_target PROPERTIES
            LABELS "ssa;aot" TIMEOUT 120)
endif ()

# Primitive frame attachment after actual source compaction and explicit rows.
if (NOT TARGET zr_vm_ssa_primitive_source_frame_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_primitive_source_frame_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_primitive_source_frame.c)
    target_sources(zr_vm_ssa_primitive_source_frame_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/parser/support/ssa_literal_script_fixture.c)
    target_include_directories(zr_vm_ssa_primitive_source_frame_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_primitive_source_frame_test)
    if (MSVC)
        target_link_options(zr_vm_ssa_primitive_source_frame_test PRIVATE /STACK:8388608)
    endif ()
    add_test(NAME ssa_primitive_source_frame
            COMMAND zr_vm_ssa_primitive_source_frame_test)
    set_tests_properties(ssa_primitive_source_frame PROPERTIES
            LABELS "ssa" TIMEOUT 120)
endif ()

# Metadata capacity and final alignment refusals on actual source graphs.
if (NOT TARGET zr_vm_ssa_primitive_source_frame_storage_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_primitive_source_frame_storage_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_primitive_source_frame_storage.c)
    target_sources(zr_vm_ssa_primitive_source_frame_storage_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/parser/support/ssa_literal_script_fixture.c)
    target_include_directories(zr_vm_ssa_primitive_source_frame_storage_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_primitive_source_frame_storage_test)
    if (MSVC)
        target_link_options(zr_vm_ssa_primitive_source_frame_storage_test PRIVATE /STACK:8388608)
    endif ()
    add_test(NAME ssa_primitive_source_frame_storage
            COMMAND zr_vm_ssa_primitive_source_frame_storage_test)
    set_tests_properties(ssa_primitive_source_frame_storage PROPERTIES
            LABELS "ssa" TIMEOUT 120)
endif ()
