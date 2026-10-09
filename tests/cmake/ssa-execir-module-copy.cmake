# Copy a whole Core storage module with one same-identity replacement function.
if (TARGET zr_vm_ssa_exec_ir_module_copy_test)
    return()
endif ()
if (NOT TARGET zr_vm_ssa_execir_owner_private_core)
    message(FATAL_ERROR "Module-copy validation requires the existing private Core objects")
endif ()
if (DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT)
    set(_zr_module_copy_root "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}")
else ()
    set(_zr_module_copy_root "${CMAKE_SOURCE_DIR}")
endif ()
add_library(zr_vm_ssa_execir_module_copy_private OBJECT
        "${_zr_module_copy_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c")
add_executable(zr_vm_ssa_exec_ir_module_copy_test
        "${_zr_module_copy_root}/tests/core/test_ssa_exec_ir_module_copy.c"
        "${_zr_module_copy_root}/tests/core/support/ssa_exec_ir_module_copy_fixture.c"
        "${_zr_module_copy_root}/tests/core/support/ssa_exec_ir_owned_module_fixture.c"
        "${_zr_module_copy_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_module_copy_root}/tests/third_party/zr_unity/Unity/src/unity.c")
foreach (_target IN ITEMS zr_vm_ssa_execir_module_copy_private
        zr_vm_ssa_exec_ir_module_copy_test)
    target_include_directories(${_target} PRIVATE
            "${_zr_module_copy_root}/zr_vm_common/include"
            "${_zr_module_copy_root}/zr_vm_core/include"
            "${_zr_module_copy_root}/tests"
            "${_zr_module_copy_root}/tests/third_party/zr_unity/Unity/src")
    target_compile_definitions(${_target} PRIVATE
            ZR_LIBRARY_TYPE_STATIC=1 _CRT_SECURE_NO_WARNINGS)
    if (COMMAND zr_ssa_execir_owner_apply_settings)
        zr_ssa_execir_owner_apply_settings(${_target})
    endif ()
endforeach ()
target_compile_definitions(zr_vm_ssa_execir_module_copy_private PRIVATE
        ZR_CURRENT_MODULE="zr_vm_core")
target_compile_definitions(zr_vm_ssa_exec_ir_module_copy_test PRIVATE
        ZR_CURRENT_MODULE="zr_vm_ssa_exec_ir_module_copy_test"
        UNITY_INCLUDE_CONFIG_H UNITY_INCLUDE_DOUBLE UNITY_DOUBLE_PRECISION=1e-6
        UNITY_USE_FLUSH_STDOUT)
if (MSVC)
    target_compile_options(zr_vm_ssa_execir_module_copy_private PRIVATE
            "/FI${_zr_module_copy_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
    target_link_options(zr_vm_ssa_exec_ir_module_copy_test PRIVATE /STACK:8388608)
else ()
    target_compile_options(zr_vm_ssa_execir_module_copy_private PRIVATE
            -include "${_zr_module_copy_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
endif ()
target_link_libraries(zr_vm_ssa_exec_ir_module_copy_test PRIVATE
        $<TARGET_OBJECTS:zr_vm_ssa_execir_owner_private_core>
        $<TARGET_OBJECTS:zr_vm_ssa_execir_module_copy_private>)
if (MSVC)
    set(_zr_module_copy_map "ssa-exec-ir-module-copy.map")
    if (DEFINED ZR_SSA_EXECIR_MODULE_COPY_MAP_NAME)
        if (NOT ZR_SSA_EXECIR_MODULE_COPY_MAP_NAME MATCHES "^[A-Za-z0-9_.-]+\\.map$")
            message(FATAL_ERROR "Module-copy map must be a map basename")
        endif ()
        set(_zr_module_copy_map "${ZR_SSA_EXECIR_MODULE_COPY_MAP_NAME}")
    endif ()
    target_link_options(zr_vm_ssa_exec_ir_module_copy_test PRIVATE
            "/MAP:${CMAKE_BINARY_DIR}/${_zr_module_copy_map}")
endif ()
add_test(NAME ssa_exec_ir_module_copy COMMAND zr_vm_ssa_exec_ir_module_copy_test)
set_tests_properties(ssa_exec_ir_module_copy PROPERTIES
        LABELS "ssa;core-storage;module-copy" TIMEOUT 120
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1;ZR_VM_PROFILE_INSTRUCTIONS=0;ZR_VM_PROFILE_SLOWPATHS=0;ZR_VM_PROFILE_HELPERS=0;ZR_VM_PROFILE_MEMORY=0")
