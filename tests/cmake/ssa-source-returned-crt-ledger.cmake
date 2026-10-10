# Actual extracted CRT ledger only: main, ledger, Unity; no lifecycle providers.
if (TARGET zr_vm_ssa_source_returned_crt_ledger_test)
    return()
endif ()
if (DEFINED ZR_SSA_RETURNED_CRT_LEDGER_SOURCE_ROOT)
    set(_zr_crt_root "${ZR_SSA_RETURNED_CRT_LEDGER_SOURCE_ROOT}")
else ()
    set(_zr_crt_root "${CMAKE_SOURCE_DIR}")
endif ()
add_executable(zr_vm_ssa_source_returned_crt_ledger_test
        "${_zr_crt_root}/tests/parser/test_ssa_source_returned_crt_ledger.c"
        "${_zr_crt_root}/tests/parser/support/ssa_source_returned_crt_ledger.c"
        "${_zr_crt_root}/tests/third_party/zr_unity/Unity/src/unity.c")
target_include_directories(zr_vm_ssa_source_returned_crt_ledger_test PRIVATE
        "${_zr_crt_root}/tests"
        "${_zr_crt_root}/tests/parser/support"
        "${_zr_crt_root}/tests/third_party/zr_unity/Unity/src"
        "${_zr_crt_root}/zr_vm_common/include"
        "${_zr_crt_root}/zr_vm_core/include")
target_compile_definitions(zr_vm_ssa_source_returned_crt_ledger_test PRIVATE
        ZR_LIBRARY_TYPE_STATIC=1 ZR_DEBUG=1
        _CRT_SECURE_NO_WARNINGS UNITY_INCLUDE_CONFIG_H UNITY_USE_FLUSH_STDOUT)
if (WIN32)
    target_compile_definitions(zr_vm_ssa_source_returned_crt_ledger_test PRIVATE ZR_PLATFORM_WIN=1)
    if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_definitions(zr_vm_ssa_source_returned_crt_ledger_test PRIVATE ZR_PLATFORM_WIN_USE_MSVC=1)
    endif ()
endif ()
set_target_properties(zr_vm_ssa_source_returned_crt_ledger_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
        MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(zr_vm_ssa_source_returned_crt_ledger_test PRIVATE /W4 /UNDEBUG)
else ()
    target_compile_options(zr_vm_ssa_source_returned_crt_ledger_test PRIVATE -Wall -Wextra -UNDEBUG)
endif ()
if (ZR_SSA_RETURNED_CRT_LEDGER_UBSAN)
    if (NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "Finite returned CRT ledger UBSan requires Clang")
    endif ()
    if (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(zr_vm_ssa_source_returned_crt_ledger_test PRIVATE
                /clang:-fsanitize=undefined /clang:-fno-sanitize-recover=all)
    else ()
        target_compile_options(zr_vm_ssa_source_returned_crt_ledger_test PRIVATE
                -fsanitize=undefined -fno-sanitize-recover=all)
        target_link_options(zr_vm_ssa_source_returned_crt_ledger_test PRIVATE
                -fsanitize=undefined -fno-sanitize-recover=all)
    endif ()
endif ()
add_test(NAME ssa_source_returned_crt_ledger COMMAND zr_vm_ssa_source_returned_crt_ledger_test)
set_tests_properties(ssa_source_returned_crt_ledger PROPERTIES
        LABELS "ssa;returned-crt;test-ledger" TIMEOUT 180
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
if (DEFINED ZR_SSA_RETURNED_CRT_LEDGER_SOURCE_ROOT)
    if (DEFINED ZR_SSA_RETURNED_CRT_LEDGER_MAP_NAME)
        if (NOT ZR_SSA_RETURNED_CRT_LEDGER_MAP_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9_.-]*\\.map$")
            message(FATAL_ERROR "Returned CRT ledger map must be a basename in the build directory")
        endif ()
        if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            target_link_options(zr_vm_ssa_source_returned_crt_ledger_test PRIVATE
                    /INCREMENTAL:NO "/MAP:${CMAKE_BINARY_DIR}/${ZR_SSA_RETURNED_CRT_LEDGER_MAP_NAME}")
        else ()
            target_link_options(zr_vm_ssa_source_returned_crt_ledger_test PRIVATE
                    "-Wl,-Map,${CMAKE_BINARY_DIR}/${ZR_SSA_RETURNED_CRT_LEDGER_MAP_NAME}")
        endif ()
    endif ()
    set(_zr_crt_graph "target\tabsolute_source\tsha256\n")
    get_target_property(_zr_crt_sources zr_vm_ssa_source_returned_crt_ledger_test SOURCES)
    foreach (_source IN LISTS _zr_crt_sources)
        file(SHA256 "${_source}" _hash)
        string(APPEND _zr_crt_graph "zr_vm_ssa_source_returned_crt_ledger_test\t${_source}\t${_hash}\n")
    endforeach ()
    set(_zr_crt_settings "compiler\t${CMAKE_C_COMPILER}\ncompiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\ngenerator\t${CMAKE_GENERATOR}\nbuild_type\t${CMAKE_BUILD_TYPE}\nlinker\t${CMAKE_LINKER}\ntoolchain_file\t${CMAKE_TOOLCHAIN_FILE}\nmake_rules_override_c\t${CMAKE_USER_MAKE_RULES_OVERRIDE_C}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\nubsan\t${ZR_SSA_RETURNED_CRT_LEDGER_UBSAN}\n")
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
            LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
        get_target_property(_value zr_vm_ssa_source_returned_crt_ledger_test ${_property})
        if (_value STREQUAL "_value-NOTFOUND")
            set(_value "")
        endif ()
        string(APPEND _zr_crt_settings "zr_vm_ssa_source_returned_crt_ledger_test.${_property}\t${_value}\n")
    endforeach ()
    file(WRITE "${CMAKE_BINARY_DIR}/returned-crt-ledger-target-sources.tsv" "${_zr_crt_graph}")
    file(WRITE "${CMAKE_BINARY_DIR}/returned-crt-ledger-settings.tsv" "${_zr_crt_settings}")
endif ()
