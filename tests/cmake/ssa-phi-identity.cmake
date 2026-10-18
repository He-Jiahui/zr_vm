# Opt-in PHI identity validation using the owned standalone providers.
if (NOT TARGET zr_vm_ssa_sccp_pooled_bool_test)
    get_filename_component(_zr_pi_standalone "${CMAKE_CURRENT_LIST_DIR}/ssa-execir-owned-direct-validation" ABSOLUTE)
    if (PROJECT_NAME STREQUAL "zr_ssa_execir_owned_validation" AND
            CMAKE_CURRENT_SOURCE_DIR STREQUAL _zr_pi_standalone AND
            CMAKE_SOURCE_DIR STREQUAL _zr_pi_standalone)
        include("${CMAKE_CURRENT_LIST_DIR}/ssa-sccp-pooled-bool.cmake")
        cmake_language(DEFER CALL include "${CMAKE_CURRENT_SOURCE_DIR}/../ssa-phi-identity.cmake")
    endif ()
    return()
endif ()
if (TARGET zr_vm_ssa_phi_identity_test)
    return()
endif ()
if (NOT TARGET zr_vm_ssa_sccp_pooled_int64_private OR
        NOT TARGET zr_vm_ssa_execir_owner_private_core OR
        NOT DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT OR
        NOT COMMAND zr_ssa_execir_owner_apply_settings)
    message(FATAL_ERROR "PHI identity requires the owned Parser/Core providers")
endif ()
set(_zr_pi_root "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}")
set(_zr_pi_target zr_vm_ssa_phi_identity_test)
foreach (_provider IN ITEMS zr_vm_ssa_sccp_pooled_int64_private
        zr_vm_ssa_gc_map_parser_consumers_private)
    if (TARGET ${_provider})
        target_sources(${_provider} PRIVATE
                "${_zr_pi_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.c"
                "${_zr_pi_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.c")
    endif ()
endforeach ()
add_executable(${_zr_pi_target}
        "${_zr_pi_root}/tests/parser/test_ssa_phi_identity.c"
        "${_zr_pi_root}/tests/parser/support/ssa_phi_identity_fixture.c"
        "${_zr_pi_root}/tests/parser/support/ssa_phi_identity_faults.c"
        "${_zr_pi_root}/tests/parser/support/ssa_phi_identity_builders.c"
        "${_zr_pi_root}/tests/parser/support/ssa_phi_identity_edge_cases.c"
        "${_zr_pi_root}/tests/parser/support/ssa_phi_identity_metadata.c"
        "${_zr_pi_root}/tests/parser/support/ssa_phi_identity_oracle.c"
        "${_zr_pi_root}/tests/parser/support/ssa_phi_identity_budget.c"
        "${_zr_pi_root}/tests/parser/support/ssa_phi_identity_transactions.c"
        "${_zr_pi_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_pi_root}/tests/third_party/zr_unity/Unity/src/unity.c")
target_include_directories(${_zr_pi_target} PRIVATE
        "${_zr_pi_root}" "${_zr_pi_root}/zr_vm_common/include"
        "${_zr_pi_root}/zr_vm_core/include" "${_zr_pi_root}/zr_vm_core/src/zr_vm_core"
        "${_zr_pi_root}/zr_vm_parser/include" "${_zr_pi_root}/zr_vm_parser/src/zr_vm_parser"
        "${_zr_pi_root}/tests" "${_zr_pi_root}/tests/core/support"
        "${_zr_pi_root}/tests/third_party/zr_unity/Unity/src")
target_compile_definitions(${_zr_pi_target} PRIVATE ZR_LIBRARY_TYPE_STATIC=1
        _CRT_SECURE_NO_WARNINGS ZR_CURRENT_MODULE="zr_vm_ssa_phi_identity_test"
        UNITY_INCLUDE_CONFIG_H UNITY_USE_FLUSH_STDOUT)
set_target_properties(${_zr_pi_target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
zr_ssa_execir_owner_apply_settings(${_zr_pi_target})
if (MSVC)
    target_compile_options(${_zr_pi_target} PRIVATE /UNDEBUG)
    target_link_options(${_zr_pi_target} PRIVATE /INCREMENTAL:NO /OPT:NOREF /OPT:NOICF
            /VERBOSE:LIB /STACK:8388608 "/MAP:${CMAKE_BINARY_DIR}/ssa-phi-identity.map")
else ()
    target_compile_options(${_zr_pi_target} PRIVATE -UNDEBUG)
    target_link_options(${_zr_pi_target} PRIVATE "-Wl,-Map,${CMAKE_BINARY_DIR}/ssa-phi-identity.map")
endif ()
target_link_libraries(${_zr_pi_target} PRIVATE
        zr_vm_ssa_sccp_pooled_int64_private zr_vm_ssa_execir_owner_private_core)
if (WIN32)
    target_link_libraries(${_zr_pi_target} PRIVATE kernel32)
endif ()
add_test(NAME ssa_phi_identity COMMAND ${_zr_pi_target})
set_tests_properties(ssa_phi_identity PROPERTIES LABELS "ssa;parser;metadata-validation"
        TIMEOUT 300 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
function(zr_ssa_phi_identity_export _test _stem _provider)
    set(_graph "target\tabsolute_source\tsha256\n")
    set(_settings "compiler\t${CMAKE_C_COMPILER}\n")
    if (_stem STREQUAL "sccp-pooled-int64" OR _stem STREQUAL "gc-map-parser-consumers")
        string(APPEND _settings "compiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\ngenerator\t${CMAKE_GENERATOR}\nbuild_type\t${CMAKE_BUILD_TYPE}\nlinker\t${CMAKE_LINKER}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\n")
        set(_targets ${_provider} ${_test} zr_vm_ssa_execir_owner_private_core)
    else ()
        set(_targets zr_vm_ssa_execir_owner_private_core
                ${_provider} ${_test})
    endif ()
    foreach (_target IN LISTS _targets)
        get_target_property(_sources ${_target} SOURCES)
        foreach (_source IN LISTS _sources)
            get_filename_component(_absolute "${_source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            file(SHA256 "${_absolute}" _hash)
            string(APPEND _graph "${_target}\t${_absolute}\t${_hash}\n")
        endforeach ()
        foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
                LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
            get_target_property(_value ${_target} ${_property})
            if (_value STREQUAL "_value-NOTFOUND")
                set(_value "")
            endif ()
            string(APPEND _settings "${_target}.${_property}\t${_value}\n")
        endforeach ()
    endforeach ()
    file(WRITE "${CMAKE_BINARY_DIR}/${_stem}-target-sources.tsv" "${_graph}")
    file(WRITE "${CMAKE_BINARY_DIR}/${_stem}-settings.tsv" "${_settings}")
endfunction()
# Earlier hooks export before this hook extends their shared provider. Refresh
# their generated descriptions from the final target properties as well.
foreach (_test IN ITEMS zr_vm_ssa_sccp_pooled_int64_test
        zr_vm_ssa_sccp_branch_sensitive_pool_test zr_vm_ssa_dce_dead_phis_test
        zr_vm_ssa_sccp_pooled_bool_test zr_vm_ssa_phi_identity_test)
    if (TARGET ${_test})
        string(REGEX REPLACE "^zr_vm_ssa_(.*)_test$" "\\1" _stem "${_test}")
        string(REPLACE "_" "-" _stem "${_stem}")
        zr_ssa_phi_identity_export(${_test} ${_stem} zr_vm_ssa_sccp_pooled_int64_private)
    endif ()
endforeach ()
if (TARGET zr_vm_ssa_gc_map_parser_consumers_test)
    zr_ssa_phi_identity_export(zr_vm_ssa_gc_map_parser_consumers_test
            gc-map-parser-consumers zr_vm_ssa_gc_map_parser_consumers_private)
endif ()
