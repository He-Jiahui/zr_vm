# Keep the complete backend service source list shared by repository and finite tests.
function(zr_vm_add_ssa_backend_test suite)
    if (NOT suite STREQUAL "service" AND NOT suite STREQUAL "admission")
        message(FATAL_ERROR "Unknown SSA backend test suite: ${suite}")
    endif ()

    set(_target "zr_vm_ssa_backend_${suite}_test")
    if (TARGET ${_target})
        return()
    endif ()

    get_filename_component(_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    add_executable(${_target}
            "${_root}/tests/core/test_ssa_backend_${suite}.c"
            "${_root}/zr_vm_core/src/zr_vm_core/execution/execution_backend.c"
            "${_root}/zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c"
            "${_root}/zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c"
            "${_root}/zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c")
    target_include_directories(${_target} PRIVATE
            "${_root}/zr_vm_core/include"
            "${_root}/zr_vm_common/include")
    target_compile_definitions(${_target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    if (suite STREQUAL "service")
        if (MSVC)
            target_compile_options(${_target} PRIVATE /UNDEBUG)
        else ()
            target_compile_options(${_target} PRIVATE -UNDEBUG)
        endif ()
    endif ()
    set_target_properties(${_target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    add_test(NAME "ssa_backend_${suite}" COMMAND ${_target})
    set_tests_properties("ssa_backend_${suite}" PROPERTIES LABELS "ssa")
endfunction()
