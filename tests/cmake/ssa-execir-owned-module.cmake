# Own the Core ExecIR storage graph without Parser or runtime providers.
if (TARGET zr_vm_ssa_exec_ir_owned_module_test)
    return()
endif ()
if (DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT)
    set(_zr_execir_owner_root "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}")
else ()
    set(_zr_execir_owner_root "${CMAKE_SOURCE_DIR}")
endif ()
set(_zr_execir_owner_core_sources
        exec_ir.c exec_ir_binding_rows.c exec_ir_verify.c exec_ir_verify_constants.c
        exec_ir_verify_ssa.c exec_ir_verify_effects.c exec_ir_verify_effect_backedges.c
        exec_ir_materialize.c exec_ir_materialize_owners.c exec_ir_state_map_liveness.c
        exec_ir_deopt_aggregate.c exec_ir_state_map_storage.c exec_ir_owner_state.c)
list(TRANSFORM _zr_execir_owner_core_sources PREPEND
        "${_zr_execir_owner_root}/zr_vm_core/src/zr_vm_core/exec_ir/")
list(APPEND _zr_execir_owner_core_sources
        "${_zr_execir_owner_root}/zr_vm_core/src/zr_vm_core/runtime/execution_contract.c")
add_library(zr_vm_ssa_execir_owner_private_core OBJECT
        ${_zr_execir_owner_core_sources}
        "${_zr_execir_owner_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owned_module.c")
add_executable(zr_vm_ssa_exec_ir_owned_module_test
        "${_zr_execir_owner_root}/tests/core/test_ssa_exec_ir_owned_module.c"
        "${_zr_execir_owner_root}/tests/core/support/ssa_exec_ir_owned_module_fixture.c"
        "${_zr_execir_owner_root}/tests/core/support/ssa_exec_ir_owner_capture.c"
        "${_zr_execir_owner_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_execir_owner_root}/tests/third_party/zr_unity/Unity/src/unity.c")
foreach (_zr_execir_owner_target IN ITEMS zr_vm_ssa_execir_owner_private_core
        zr_vm_ssa_exec_ir_owned_module_test)
    target_include_directories(${_zr_execir_owner_target} PRIVATE
            "${_zr_execir_owner_root}/zr_vm_common/include"
            "${_zr_execir_owner_root}/zr_vm_core/include"
            "${_zr_execir_owner_root}/zr_vm_core/src/zr_vm_core"
            "${_zr_execir_owner_root}/tests"
            "${_zr_execir_owner_root}/tests/third_party/zr_unity/Unity/src")
    target_compile_definitions(${_zr_execir_owner_target} PRIVATE
            ZR_LIBRARY_TYPE_STATIC=1 _CRT_SECURE_NO_WARNINGS)
    if (DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT)
        zr_ssa_execir_owner_apply_settings(${_zr_execir_owner_target})
    endif ()
endforeach ()
target_compile_definitions(zr_vm_ssa_execir_owner_private_core PRIVATE
        ZR_CURRENT_MODULE="zr_vm_core")
target_compile_definitions(zr_vm_ssa_exec_ir_owned_module_test PRIVATE
        ZR_CURRENT_MODULE="zr_vm_ssa_exec_ir_owned_module_test"
        UNITY_INCLUDE_CONFIG_H UNITY_USE_FLUSH_STDOUT)
if (MSVC)
    target_compile_options(zr_vm_ssa_execir_owner_private_core PRIVATE
            "/FI${_zr_execir_owner_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
else ()
    target_compile_options(zr_vm_ssa_execir_owner_private_core PRIVATE
            -include "${_zr_execir_owner_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
endif ()
target_link_libraries(zr_vm_ssa_exec_ir_owned_module_test PRIVATE
        zr_vm_ssa_execir_owner_private_core)
if (DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT)
    set(ZR_SSA_EXECIR_OWNER_MAP_NAME "ssa-execir-owned-module-link.map"
            CACHE STRING "Finite owner link map basename")
    if (NOT ZR_SSA_EXECIR_OWNER_MAP_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9_.-]*\\.map$")
        message(FATAL_ERROR "Owner map must be a basename in the build directory")
    endif ()
    target_link_options(zr_vm_ssa_exec_ir_owned_module_test PRIVATE
            /INCREMENTAL:NO /OPT:REF /VERBOSE:LIB
            "/MAP:${CMAKE_BINARY_DIR}/${ZR_SSA_EXECIR_OWNER_MAP_NAME}")
endif ()
add_test(NAME ssa_exec_ir_owned_module COMMAND zr_vm_ssa_exec_ir_owned_module_test)
set_tests_properties(ssa_exec_ir_owned_module PROPERTIES
        LABELS "ssa;core-storage;unique-ownership" TIMEOUT 120)
