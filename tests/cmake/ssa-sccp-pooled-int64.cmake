# Finite pooled-module Int64 SCCP metadata target; activated separately by Root.
if (TARGET zr_vm_ssa_sccp_pooled_int64_test)
    return()
endif ()
if (NOT TARGET zr_vm_ssa_execir_owner_private_core OR
        NOT DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT OR
        NOT COMMAND zr_ssa_execir_owner_apply_settings)
    message(FATAL_ERROR "Pooled SCCP requires the real finite owner Core15 providers")
endif ()
set(_zr_pool_root "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}")
target_include_directories(zr_vm_ssa_execir_owner_private_core PRIVATE
        "${_zr_pool_root}/zr_vm_core/src/zr_vm_core")
set(_zr_pool_providers
        exec_ir/exec_ir_pass_manager.c
        exec_ir/passes/exec_ir_sccp.c
        exec_ir/passes/exec_ir_sccp_owner.c
        exec_ir/passes/exec_ir_copy_aliases.c
        exec_ir/passes/exec_ir_dce.c
        exec_ir/exec_ir_cfg.c
        exec_ir/exec_ir_effects.c
        exec_ir/exec_ir_effects_linear.c
        exec_ir/exec_ir_effect_loops.c
        exec_ir/exec_ir_state_maps.c)
list(TRANSFORM _zr_pool_providers PREPEND
        "${_zr_pool_root}/zr_vm_parser/src/zr_vm_parser/")
# RED does not require a nonexistent provider or private symbol.
set(_zr_pool_owner_available OFF)
if (EXISTS "${_zr_pool_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_constant_materialize.c")
    list(APPEND _zr_pool_providers
            "${_zr_pool_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_constant_materialize.c")
    set(_zr_pool_owner_available ON)
endif ()
add_library(zr_vm_ssa_sccp_pooled_int64_private OBJECT ${_zr_pool_providers})
add_executable(zr_vm_ssa_sccp_pooled_int64_test
        "${_zr_pool_root}/tests/parser/test_ssa_sccp_pooled_int64.c"
        "${_zr_pool_root}/tests/parser/support/ssa_sccp_pooled_int64_fixture.c"
        "${_zr_pool_root}/tests/parser/support/ssa_sccp_pooled_int64_faults.c"
        "${_zr_pool_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_pool_root}/tests/third_party/zr_unity/Unity/src/unity.c")
add_executable(zr_vm_ssa_pass_manager_scalar_metadata_test
        "${_zr_pool_root}/tests/parser/test_ssa_pass_manager_scalar_metadata.c"
        "${_zr_pool_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_pool_root}/tests/third_party/zr_unity/Unity/src/unity.c")
foreach (_target IN ITEMS zr_vm_ssa_sccp_pooled_int64_private
        zr_vm_ssa_sccp_pooled_int64_test zr_vm_ssa_pass_manager_scalar_metadata_test)
    target_include_directories(${_target} PRIVATE
            "${_zr_pool_root}"
            "${_zr_pool_root}/zr_vm_common/include"
            "${_zr_pool_root}/zr_vm_core/include"
            "${_zr_pool_root}/zr_vm_core/src/zr_vm_core"
            "${_zr_pool_root}/zr_vm_parser/include"
            "${_zr_pool_root}/zr_vm_parser/src/zr_vm_parser"
            "${_zr_pool_root}/tests"
            "${_zr_pool_root}/tests/core/support"
            "${_zr_pool_root}/tests/third_party/zr_unity/Unity/src")
    target_compile_definitions(${_target} PRIVATE ZR_LIBRARY_TYPE_STATIC=1 _CRT_SECURE_NO_WARNINGS)
    set_target_properties(${_target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
            C_EXTENSIONS OFF MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    zr_ssa_execir_owner_apply_settings(${_target})
    if (MSVC)
        target_compile_options(${_target} PRIVATE /UNDEBUG)
    else ()
        target_compile_options(${_target} PRIVATE -UNDEBUG)
    endif ()
endforeach ()
target_compile_definitions(zr_vm_ssa_sccp_pooled_int64_private PRIVATE
        ZR_CURRENT_MODULE="zr_vm_pooled_int64_providers")
target_compile_definitions(zr_vm_ssa_sccp_pooled_int64_test PRIVATE
        ZR_CURRENT_MODULE="zr_vm_ssa_sccp_pooled_int64_test"
        UNITY_INCLUDE_CONFIG_H UNITY_USE_FLUSH_STDOUT)
target_compile_definitions(zr_vm_ssa_pass_manager_scalar_metadata_test PRIVATE
        ZR_CURRENT_MODULE="zr_vm_ssa_pass_manager_scalar_metadata_test"
        UNITY_INCLUDE_CONFIG_H UNITY_USE_FLUSH_STDOUT)
if (_zr_pool_owner_available)
    target_compile_definitions(zr_vm_ssa_sccp_pooled_int64_test PRIVATE ZR_SCCP_POOLED_INT64_OWNER=1)
endif ()
# Fixture/faults include the shim after stdlib. Main, allocator NO_REDIRECT and
# Unity retain ordinary CRT. Only real production providers receive forced FI.
if (MSVC)
    target_compile_options(zr_vm_ssa_sccp_pooled_int64_private PRIVATE
            "/FI${_zr_pool_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
    target_link_options(zr_vm_ssa_sccp_pooled_int64_test PRIVATE
            /INCREMENTAL:NO /OPT:NOREF /OPT:NOICF /VERBOSE:LIB /STACK:8388608
            "/MAP:${CMAKE_BINARY_DIR}/ssa-sccp-pooled-int64.map")
else ()
    target_compile_options(zr_vm_ssa_sccp_pooled_int64_private PRIVATE
            -include "${_zr_pool_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
    target_link_options(zr_vm_ssa_sccp_pooled_int64_test PRIVATE
            "-Wl,-Map,${CMAKE_BINARY_DIR}/ssa-sccp-pooled-int64.map")
endif ()
if (MSVC)
    target_link_options(zr_vm_ssa_pass_manager_scalar_metadata_test PRIVATE
            /INCREMENTAL:NO /OPT:NOREF /OPT:NOICF /VERBOSE:LIB /STACK:8388608
            "/MAP:${CMAKE_BINARY_DIR}/scalar-metadata.map")
else ()
    target_link_options(zr_vm_ssa_pass_manager_scalar_metadata_test PRIVATE
            "-Wl,-Map,${CMAKE_BINARY_DIR}/scalar-metadata.map")
endif ()
target_link_libraries(zr_vm_ssa_pass_manager_scalar_metadata_test PRIVATE
        zr_vm_ssa_sccp_pooled_int64_private zr_vm_ssa_execir_owner_private_core)
if (WIN32)
    target_link_libraries(zr_vm_ssa_pass_manager_scalar_metadata_test PRIVATE kernel32)
endif ()
add_test(NAME ssa_pass_manager_scalar_metadata
        COMMAND zr_vm_ssa_pass_manager_scalar_metadata_test)
set_tests_properties(ssa_pass_manager_scalar_metadata PROPERTIES
        LABELS "ssa;parser;metadata-validation" TIMEOUT 180
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")

target_link_libraries(zr_vm_ssa_sccp_pooled_int64_test PRIVATE
        zr_vm_ssa_sccp_pooled_int64_private zr_vm_ssa_execir_owner_private_core)
if (WIN32)
    target_link_libraries(zr_vm_ssa_sccp_pooled_int64_test PRIVATE kernel32)
endif ()
add_test(NAME ssa_sccp_pooled_int64 COMMAND zr_vm_ssa_sccp_pooled_int64_test)
set_tests_properties(ssa_sccp_pooled_int64 PROPERTIES LABELS "ssa;parser;metadata-validation"
        TIMEOUT 180 ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
set(_zr_pool_graph "target\tabsolute_source\tsha256\n")
set(_zr_pool_settings "compiler\t${CMAKE_C_COMPILER}\ncompiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\ngenerator\t${CMAKE_GENERATOR}\nbuild_type\t${CMAKE_BUILD_TYPE}\nlinker\t${CMAKE_LINKER}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\n")
foreach (_target IN ITEMS zr_vm_ssa_sccp_pooled_int64_private
        zr_vm_ssa_sccp_pooled_int64_test zr_vm_ssa_execir_owner_private_core)
    get_target_property(_sources ${_target} SOURCES)
    foreach (_source IN LISTS _sources)
        get_filename_component(_absolute "${_source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(SHA256 "${_absolute}" _hash)
        string(APPEND _zr_pool_graph "${_target}\t${_absolute}\t${_hash}\n")
    endforeach ()
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
            LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
        get_target_property(_value ${_target} ${_property})
        if (_value STREQUAL "_value-NOTFOUND")
            set(_value "")
        endif ()
        string(APPEND _zr_pool_settings "${_target}.${_property}\t${_value}\n")
    endforeach ()
endforeach ()
file(WRITE "${CMAKE_BINARY_DIR}/sccp-pooled-int64-target-sources.tsv" "${_zr_pool_graph}")
file(WRITE "${CMAKE_BINARY_DIR}/sccp-pooled-int64-settings.tsv" "${_zr_pool_settings}")

# Keep the main pooled graph's three targets unchanged. This separately
# selected regression compiles the actual wrapper and links the same providers.
set(_zr_metadata_target zr_vm_ssa_pass_manager_scalar_metadata_test)
get_target_property(_zr_metadata_sources ${_zr_metadata_target} SOURCES)
set(_zr_metadata_graph "target\tabsolute_source\tsha256\n")
foreach (_source IN LISTS _zr_metadata_sources)
    file(SHA256 "${_source}" _hash)
    string(APPEND _zr_metadata_graph "${_zr_metadata_target}\t${_source}\t${_hash}\n")
endforeach ()
set(_zr_metadata_settings "compiler\t${CMAKE_C_COMPILER}\n")
foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
        LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
    get_target_property(_value ${_zr_metadata_target} ${_property})
    if (_value STREQUAL "_value-NOTFOUND")
        set(_value "")
    endif ()
    string(APPEND _zr_metadata_settings "${_zr_metadata_target}.${_property}\t${_value}\n")
endforeach ()
file(WRITE "${CMAKE_BINARY_DIR}/scalar-metadata-target-sources.tsv" "${_zr_metadata_graph}")
file(WRITE "${CMAKE_BINARY_DIR}/scalar-metadata-settings.tsv" "${_zr_metadata_settings}")
