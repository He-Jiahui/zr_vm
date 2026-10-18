# Opt-in hook chains Source39 registration without editing the shared root.
if (NOT TARGET zr_vm_ssa_sccp_pooled_int64_private)
    get_filename_component(_zr_pb_standalone "${CMAKE_CURRENT_LIST_DIR}/ssa-execir-owned-direct-validation" ABSOLUTE)
    if (PROJECT_NAME STREQUAL "zr_ssa_execir_owned_validation" AND
            CMAKE_CURRENT_SOURCE_DIR STREQUAL _zr_pb_standalone AND
            CMAKE_SOURCE_DIR STREQUAL _zr_pb_standalone)
        include("${CMAKE_CURRENT_LIST_DIR}/ssa-dce-dead-phis.cmake")
        cmake_language(DEFER CALL include "${CMAKE_CURRENT_SOURCE_DIR}/../ssa-sccp-pooled-bool.cmake")
    endif ()
    return()
endif ()
if (TARGET zr_vm_ssa_sccp_pooled_bool_test)
    return()
endif ()
if (NOT TARGET zr_vm_ssa_execir_owner_private_core OR
        NOT DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT OR
        NOT COMMAND zr_ssa_execir_owner_apply_settings)
    message(FATAL_ERROR "Pooled BOOL requires real pooled Parser11/Core15 providers")
endif ()
set(_zr_pb_root "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}")
set(_zr_pb_target zr_vm_ssa_sccp_pooled_bool_test)
add_executable(${_zr_pb_target}
        "${_zr_pb_root}/tests/parser/test_ssa_sccp_pooled_bool.c"
        "${_zr_pb_root}/tests/parser/support/ssa_sccp_pooled_bool_fixture.c"
        "${_zr_pb_root}/tests/parser/support/ssa_sccp_pooled_bool_faults.c"
        "${_zr_pb_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_pb_root}/tests/third_party/zr_unity/Unity/src/unity.c")
target_include_directories(${_zr_pb_target} PRIVATE
        "${_zr_pb_root}" "${_zr_pb_root}/zr_vm_common/include"
        "${_zr_pb_root}/zr_vm_core/include" "${_zr_pb_root}/zr_vm_core/src/zr_vm_core"
        "${_zr_pb_root}/zr_vm_parser/include" "${_zr_pb_root}/zr_vm_parser/src/zr_vm_parser"
        "${_zr_pb_root}/tests" "${_zr_pb_root}/tests/core/support"
        "${_zr_pb_root}/tests/third_party/zr_unity/Unity/src")
target_compile_definitions(${_zr_pb_target} PRIVATE ZR_LIBRARY_TYPE_STATIC=1
        _CRT_SECURE_NO_WARNINGS ZR_CURRENT_MODULE="zr_vm_ssa_sccp_pooled_bool_test"
        UNITY_INCLUDE_CONFIG_H UNITY_USE_FLUSH_STDOUT)
set_target_properties(${_zr_pb_target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
zr_ssa_execir_owner_apply_settings(${_zr_pb_target})
if (MSVC)
    target_compile_options(${_zr_pb_target} PRIVATE /UNDEBUG)
    target_link_options(${_zr_pb_target} PRIVATE /INCREMENTAL:NO /OPT:NOREF /OPT:NOICF
            /VERBOSE:LIB /STACK:8388608 "/MAP:${CMAKE_BINARY_DIR}/ssa-sccp-pooled-bool.map")
else ()
    target_compile_options(${_zr_pb_target} PRIVATE -UNDEBUG)
    target_link_options(${_zr_pb_target} PRIVATE "-Wl,-Map,${CMAKE_BINARY_DIR}/ssa-sccp-pooled-bool.map")
endif ()
target_link_libraries(${_zr_pb_target} PRIVATE
        zr_vm_ssa_sccp_pooled_int64_private zr_vm_ssa_execir_owner_private_core)
if (WIN32)
    target_link_libraries(${_zr_pb_target} PRIVATE kernel32)
endif ()
add_test(NAME ssa_sccp_pooled_bool COMMAND ${_zr_pb_target})
set_tests_properties(ssa_sccp_pooled_bool PROPERTIES LABELS "ssa;parser;metadata-validation"
        TIMEOUT 300 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
set(_zr_pb_graph "target\tabsolute_source\tsha256\n")
set(_zr_pb_settings "compiler\t${CMAKE_C_COMPILER}\n")
foreach (_target IN ITEMS zr_vm_ssa_execir_owner_private_core
        zr_vm_ssa_sccp_pooled_int64_private ${_zr_pb_target})
    get_target_property(_sources ${_target} SOURCES)
    foreach (_source IN LISTS _sources)
        get_filename_component(_absolute "${_source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(SHA256 "${_absolute}" _hash)
        string(APPEND _zr_pb_graph "${_target}\t${_absolute}\t${_hash}\n")
    endforeach ()
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
            LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
        get_target_property(_value ${_target} ${_property})
        if (_value STREQUAL "_value-NOTFOUND")
            set(_value "")
        endif ()
        string(APPEND _zr_pb_settings "${_target}.${_property}\t${_value}\n")
    endforeach ()
endforeach ()
file(WRITE "${CMAKE_BINARY_DIR}/sccp-pooled-bool-target-sources.tsv" "${_zr_pb_graph}")
file(WRITE "${CMAKE_BINARY_DIR}/sccp-pooled-bool-settings.tsv" "${_zr_pb_settings}")
