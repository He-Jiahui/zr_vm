# Shared by the finite standalone entry and an explicit normal-graph registration.
function(zr_vm_add_integer_remainder_tests)
    set(_target zr_vm_execution_integer_remainder_test)
    if (TARGET ${_target})
        return()
    endif ()

    get_filename_component(_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    set(_unity "${_root}/tests/third_party/zr_unity/Unity/src")
    add_executable(${_target}
            "${_root}/tests/core/test_execution_integer_remainder.c"
            "${_unity}/unity.c")
    target_include_directories(${_target} PRIVATE
            "${_root}/zr_vm_core/include"
            "${_root}/zr_vm_core/src/zr_vm_core"
            "${_root}/zr_vm_common/include"
            "${_unity}")
    target_compile_definitions(${_target} PRIVATE _CRT_SECURE_NO_WARNINGS UNITY_SUPPORT_64)
    set_target_properties(${_target} PROPERTIES
            C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
            MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    if (MSVC)
        target_compile_options(${_target} PRIVATE /UNDEBUG /utf-8 /W4)
    else ()
        target_compile_options(${_target} PRIVATE -UNDEBUG -Wall -Wextra)
    endif ()

    if (CMAKE_C_COMPILER_ID MATCHES "Clang")
        if (MSVC)
            target_compile_options(${_target} PRIVATE
                    /clang:-fsanitize=undefined /clang:-fno-sanitize-recover=all)
            # v35 already uses the clang driver for linking. An explicit runtime
            # also supports the normal graph's direct MSVC-style linker rule.
            if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
                message(FATAL_ERROR "The finite Windows remainder fixture requires x64")
            endif ()
            get_filename_component(_compiler_bin "${CMAKE_C_COMPILER}" DIRECTORY)
            string(REGEX MATCH "^[0-9]+" _clang_major "${CMAKE_C_COMPILER_VERSION}")
            get_filename_component(_ubsan_runtime
                    "${_compiler_bin}/../lib/clang/${_clang_major}/lib/windows/clang_rt.ubsan_standalone-x86_64.lib"
                    ABSOLUTE)
            if (NOT EXISTS "${_ubsan_runtime}")
                message(FATAL_ERROR "Missing Clang UBSan runtime: ${_ubsan_runtime}")
            endif ()
            target_link_libraries(${_target} PRIVATE "${_ubsan_runtime}")
        else ()
            target_compile_options(${_target} PRIVATE -fsanitize=undefined -fno-sanitize-recover=all)
            target_link_options(${_target} PRIVATE -fsanitize=undefined -fno-sanitize-recover=all)
        endif ()
    endif ()

    foreach (_case IN ITEMS fast_min_divisor fast_defined_cases fast_protocol)
        set(_test "execution_integer_remainder_${_case}")
        add_test(NAME "${_test}" COMMAND ${_target} "--case=${_case}")
        set_tests_properties("${_test}" PROPERTIES
                LABELS "integer-remainder" TIMEOUT 30
                ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=0:symbolize=0:log_path=stderr")
    endforeach ()
endfunction()
