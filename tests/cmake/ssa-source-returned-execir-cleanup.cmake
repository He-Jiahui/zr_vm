# Finite cleanup worker, actual ledgers and Unity. No lifecycle/Source providers.
if (TARGET zr_vm_ssa_source_returned_execir_cleanup_test)
    return()
endif ()
if (DEFINED ZR_SSA_RETURNED_EXEC_CLEANUP_SOURCE_ROOT)
    set(_zr_cleanup_root "${ZR_SSA_RETURNED_EXEC_CLEANUP_SOURCE_ROOT}")
else ()
    set(_zr_cleanup_root "${CMAKE_SOURCE_DIR}")
endif ()
set(ZR_SSA_RETURNED_EXEC_CLEANUP_EXPECTED_PHASE "GREEN" CACHE STRING "Reviewed cleanup protocol: RED or GREEN")
set_property(CACHE ZR_SSA_RETURNED_EXEC_CLEANUP_EXPECTED_PHASE PROPERTY STRINGS RED GREEN)
if (NOT ZR_SSA_RETURNED_EXEC_CLEANUP_EXPECTED_PHASE MATCHES "^(RED|GREEN)$")
    message(FATAL_ERROR "Select the exact reviewed RED or GREEN cleanup protocol")
endif ()
add_executable(zr_vm_ssa_source_returned_execir_cleanup_test
        "${_zr_cleanup_root}/tests/parser/test_ssa_source_returned_execir_cleanup.c"
        "${_zr_cleanup_root}/tests/parser/support/ssa_source_returned_execir_cleanup.c"
        "${_zr_cleanup_root}/tests/parser/support/ssa_source_returned_crt_ledger.c"
        "${_zr_cleanup_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_cleanup_root}/tests/third_party/zr_unity/Unity/src/unity.c")
target_include_directories(zr_vm_ssa_source_returned_execir_cleanup_test PRIVATE
        "${_zr_cleanup_root}/tests"
        "${_zr_cleanup_root}/tests/parser/support"
        "${_zr_cleanup_root}/tests/core/support"
        "${_zr_cleanup_root}/tests/third_party/zr_unity/Unity/src"
        "${_zr_cleanup_root}/zr_vm_common/include"
        "${_zr_cleanup_root}/zr_vm_core/include")
target_compile_definitions(zr_vm_ssa_source_returned_execir_cleanup_test PRIVATE
        ZR_LIBRARY_TYPE_STATIC=1 ZR_DEBUG=1 _CRT_SECURE_NO_WARNINGS
        UNITY_INCLUDE_CONFIG_H UNITY_USE_FLUSH_STDOUT)
if (WIN32)
    target_compile_definitions(zr_vm_ssa_source_returned_execir_cleanup_test PRIVATE ZR_PLATFORM_WIN=1)
    if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_definitions(zr_vm_ssa_source_returned_execir_cleanup_test PRIVATE ZR_PLATFORM_WIN_USE_MSVC=1)
    endif ()
endif ()
set_target_properties(zr_vm_ssa_source_returned_execir_cleanup_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
        MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(zr_vm_ssa_source_returned_execir_cleanup_test PRIVATE /W4 /UNDEBUG)
else ()
    target_compile_options(zr_vm_ssa_source_returned_execir_cleanup_test PRIVATE -Wall -Wextra -UNDEBUG)
endif ()
if (ZR_SSA_RETURNED_EXEC_CLEANUP_UBSAN)
    if (NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "Finite cleanup UBSan requires Clang")
    endif ()
    if (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(zr_vm_ssa_source_returned_execir_cleanup_test PRIVATE
                /clang:-fsanitize=undefined /clang:-fno-sanitize-recover=all)
    else ()
        target_compile_options(zr_vm_ssa_source_returned_execir_cleanup_test PRIVATE
                -fsanitize=undefined -fno-sanitize-recover=all)
        target_link_options(zr_vm_ssa_source_returned_execir_cleanup_test PRIVATE
                -fsanitize=undefined -fno-sanitize-recover=all)
    endif ()
endif ()
add_test(NAME ssa_source_returned_execir_cleanup
        COMMAND "${CMAKE_COMMAND}"
        "-DZR_CLEANUP_EXECUTABLE=$<TARGET_FILE:zr_vm_ssa_source_returned_execir_cleanup_test>"
        "-DZR_CLEANUP_EXPECTED_PHASE=${ZR_SSA_RETURNED_EXEC_CLEANUP_EXPECTED_PHASE}"
        "-DZR_CLEANUP_OUTPUT_DIR=${CMAKE_BINARY_DIR}"
        -P "${_zr_cleanup_root}/tests/cmake/ssa-source-returned-execir-cleanup-check.cmake")
set_tests_properties(ssa_source_returned_execir_cleanup PROPERTIES
        LABELS "ssa;returned-execir;cleanup;expected-negative" TIMEOUT 180
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
if (DEFINED ZR_SSA_RETURNED_EXEC_CLEANUP_SOURCE_ROOT)
    if (DEFINED ZR_SSA_RETURNED_EXEC_CLEANUP_MAP_NAME)
        if (NOT ZR_SSA_RETURNED_EXEC_CLEANUP_MAP_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9_.-]*\\.map$")
            message(FATAL_ERROR "Cleanup map must be a basename in the build directory")
        endif ()
        if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            target_link_options(zr_vm_ssa_source_returned_execir_cleanup_test PRIVATE
                    /INCREMENTAL:NO "/MAP:${CMAKE_BINARY_DIR}/${ZR_SSA_RETURNED_EXEC_CLEANUP_MAP_NAME}")
        else ()
            target_link_options(zr_vm_ssa_source_returned_execir_cleanup_test PRIVATE
                    "-Wl,-Map,${CMAKE_BINARY_DIR}/${ZR_SSA_RETURNED_EXEC_CLEANUP_MAP_NAME}")
        endif ()
    endif ()
    set(_zr_cleanup_graph "target\tabsolute_source\tsha256\n")
    get_target_property(_zr_cleanup_sources zr_vm_ssa_source_returned_execir_cleanup_test SOURCES)
    list(LENGTH _zr_cleanup_sources _zr_cleanup_source_count)
    if (NOT _zr_cleanup_source_count EQUAL 5)
        message(FATAL_ERROR "Cleanup gate requires exactly five C translation units")
    endif ()
    foreach (_source IN LISTS _zr_cleanup_sources)
        file(SHA256 "${_source}" _hash)
        string(APPEND _zr_cleanup_graph "zr_vm_ssa_source_returned_execir_cleanup_test\t${_source}\t${_hash}\n")
    endforeach ()
    set(_zr_cleanup_settings "compiler\t${CMAKE_C_COMPILER}\ncompiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\ngenerator\t${CMAKE_GENERATOR}\nbuild_type\t${CMAKE_BUILD_TYPE}\nlinker\t${CMAKE_LINKER}\ntoolchain_file\t${CMAKE_TOOLCHAIN_FILE}\nmake_rules_override_c\t${CMAKE_USER_MAKE_RULES_OVERRIDE_C}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\nubsan\t${ZR_SSA_RETURNED_EXEC_CLEANUP_UBSAN}\nexpected_phase\t${ZR_SSA_RETURNED_EXEC_CLEANUP_EXPECTED_PHASE}\n")
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
            LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
        get_target_property(_value zr_vm_ssa_source_returned_execir_cleanup_test ${_property})
        if (_value STREQUAL "_value-NOTFOUND")
            set(_value "")
        endif ()
        string(APPEND _zr_cleanup_settings "zr_vm_ssa_source_returned_execir_cleanup_test.${_property}\t${_value}\n")
    endforeach ()
    file(WRITE "${CMAKE_BINARY_DIR}/returned-execir-cleanup-target-sources.tsv" "${_zr_cleanup_graph}")
    file(WRITE "${CMAKE_BINARY_DIR}/returned-execir-cleanup-settings.tsv" "${_zr_cleanup_settings}")
endif ()
