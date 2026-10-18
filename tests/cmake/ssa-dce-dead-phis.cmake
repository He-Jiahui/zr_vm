# Optional CMAKE_PROJECT_INCLUDE hook: defer only in the exact standalone project.
if (NOT TARGET zr_vm_ssa_sccp_pooled_int64_private)
    get_filename_component(_zr_dp_standalone "${CMAKE_CURRENT_LIST_DIR}/ssa-execir-owned-direct-validation" ABSOLUTE)
    if (PROJECT_NAME STREQUAL "zr_ssa_execir_owned_validation" AND
            CMAKE_CURRENT_SOURCE_DIR STREQUAL _zr_dp_standalone AND
            CMAKE_SOURCE_DIR STREQUAL _zr_dp_standalone)
        cmake_language(DEFER CALL include "${CMAKE_CURRENT_SOURCE_DIR}/../ssa-dce-dead-phis.cmake")
    endif ()
    return()
endif ()
# Finite IR-only dead-PHI DCE target. Include after pooled providers.
if (TARGET zr_vm_ssa_dce_dead_phis_test)
    return()
endif ()
if (NOT TARGET zr_vm_ssa_sccp_pooled_int64_private OR
        NOT TARGET zr_vm_ssa_execir_owner_private_core OR
        NOT DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT OR
        NOT COMMAND zr_ssa_execir_owner_apply_settings)
    message(FATAL_ERROR "Dead-PHI metadata target requires real pooled/Core providers")
endif ()
set(_zr_dp_root "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}")
set(_zr_dp_target zr_vm_ssa_dce_dead_phis_test)
add_executable(${_zr_dp_target}
        "${_zr_dp_root}/tests/parser/test_ssa_dce_dead_phis.c"
        "${_zr_dp_root}/tests/parser/support/ssa_dce_dead_phis_fixture.c"
        "${_zr_dp_root}/tests/parser/support/ssa_dce_dead_phis_faults.c"
        "${_zr_dp_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_dp_root}/tests/third_party/zr_unity/Unity/src/unity.c")
target_include_directories(${_zr_dp_target} PRIVATE
        "${_zr_dp_root}" "${_zr_dp_root}/zr_vm_common/include"
        "${_zr_dp_root}/zr_vm_core/include" "${_zr_dp_root}/zr_vm_core/src/zr_vm_core"
        "${_zr_dp_root}/zr_vm_parser/include" "${_zr_dp_root}/zr_vm_parser/src/zr_vm_parser"
        "${_zr_dp_root}/tests" "${_zr_dp_root}/tests/core/support"
        "${_zr_dp_root}/tests/third_party/zr_unity/Unity/src")
target_compile_definitions(${_zr_dp_target} PRIVATE ZR_LIBRARY_TYPE_STATIC=1
        _CRT_SECURE_NO_WARNINGS ZR_CURRENT_MODULE="zr_vm_ssa_dce_dead_phis_test"
        UNITY_INCLUDE_CONFIG_H UNITY_USE_FLUSH_STDOUT)
set_target_properties(${_zr_dp_target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
zr_ssa_execir_owner_apply_settings(${_zr_dp_target})
# Only fixture/faults include the shim explicitly after stdlib. Production
# OBJECT providers inherit their already owned forced-include configuration.
if (MSVC)
    target_compile_options(${_zr_dp_target} PRIVATE /UNDEBUG)
    target_link_options(${_zr_dp_target} PRIVATE /INCREMENTAL:NO /OPT:NOREF /OPT:NOICF
            /VERBOSE:LIB /STACK:8388608 "/MAP:${CMAKE_BINARY_DIR}/ssa-dce-dead-phis.map")
else ()
    target_compile_options(${_zr_dp_target} PRIVATE -UNDEBUG)
    target_link_options(${_zr_dp_target} PRIVATE "-Wl,-Map,${CMAKE_BINARY_DIR}/ssa-dce-dead-phis.map")
endif ()
target_link_libraries(${_zr_dp_target} PRIVATE zr_vm_ssa_sccp_pooled_int64_private zr_vm_ssa_execir_owner_private_core)
if (WIN32)
    target_link_libraries(${_zr_dp_target} PRIVATE kernel32)
endif ()
add_test(NAME ssa_dce_dead_phis COMMAND ${_zr_dp_target})
set_tests_properties(ssa_dce_dead_phis PROPERTIES LABELS "ssa;parser;metadata-validation"
        TIMEOUT 300 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
set(_zr_dp_graph "target\tabsolute_source\tsha256\n")
set(_zr_dp_settings "compiler\t${CMAKE_C_COMPILER}\n")
foreach (_target IN ITEMS zr_vm_ssa_execir_owner_private_core zr_vm_ssa_sccp_pooled_int64_private ${_zr_dp_target})
    get_target_property(_sources ${_target} SOURCES)
    foreach (_source IN LISTS _sources)
        get_filename_component(_absolute "${_source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(SHA256 "${_absolute}" _hash)
        string(APPEND _zr_dp_graph "${_target}\t${_absolute}\t${_hash}\n")
    endforeach ()
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
            LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
        get_target_property(_value ${_target} ${_property})
        if (_value STREQUAL "_value-NOTFOUND")
            set(_value "")
        endif ()
        string(APPEND _zr_dp_settings "${_target}.${_property}\t${_value}\n")
    endforeach ()
endforeach ()
file(WRITE "${CMAKE_BINARY_DIR}/dce-dead-phis-target-sources.tsv" "${_zr_dp_graph}")
file(WRITE "${CMAKE_BINARY_DIR}/dce-dead-phis-settings.tsv" "${_zr_dp_settings}")
