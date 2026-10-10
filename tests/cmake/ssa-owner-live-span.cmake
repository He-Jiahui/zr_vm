# Actual test-ledger query only: main, allocator, Unity; no redirected providers.
if (TARGET zr_vm_ssa_exec_ir_owner_live_span_test)
    return()
endif ()
if (DEFINED ZR_SSA_OWNER_LIVE_SPAN_SOURCE_ROOT)
    set(_zr_live_span_root "${ZR_SSA_OWNER_LIVE_SPAN_SOURCE_ROOT}")
else ()
    set(_zr_live_span_root "${CMAKE_SOURCE_DIR}")
endif ()
add_executable(zr_vm_ssa_exec_ir_owner_live_span_test
        "${_zr_live_span_root}/tests/core/test_ssa_exec_ir_owner_live_span.c"
        "${_zr_live_span_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_live_span_root}/tests/third_party/zr_unity/Unity/src/unity.c")
target_include_directories(zr_vm_ssa_exec_ir_owner_live_span_test PRIVATE
        "${_zr_live_span_root}/tests"
        "${_zr_live_span_root}/tests/core/support"
        "${_zr_live_span_root}/tests/third_party/zr_unity/Unity/src")
target_compile_definitions(zr_vm_ssa_exec_ir_owner_live_span_test PRIVATE
        _CRT_SECURE_NO_WARNINGS UNITY_INCLUDE_CONFIG_H UNITY_USE_FLUSH_STDOUT)
set_target_properties(zr_vm_ssa_exec_ir_owner_live_span_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
        MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(zr_vm_ssa_exec_ir_owner_live_span_test PRIVATE /W4 /UNDEBUG)
else ()
    target_compile_options(zr_vm_ssa_exec_ir_owner_live_span_test PRIVATE -Wall -Wextra -UNDEBUG)
endif ()
if (ZR_SSA_OWNER_LIVE_SPAN_UBSAN)
    if (NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "Finite live-span UBSan requires Clang")
    endif ()
    if (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(zr_vm_ssa_exec_ir_owner_live_span_test PRIVATE
                /clang:-fsanitize=undefined /clang:-fno-sanitize-recover=all)
    else ()
        target_compile_options(zr_vm_ssa_exec_ir_owner_live_span_test PRIVATE
                -fsanitize=undefined -fno-sanitize-recover=all)
        target_link_options(zr_vm_ssa_exec_ir_owner_live_span_test PRIVATE
                -fsanitize=undefined -fno-sanitize-recover=all)
    endif ()
endif ()
add_test(NAME ssa_exec_ir_owner_live_span COMMAND zr_vm_ssa_exec_ir_owner_live_span_test)
set_tests_properties(ssa_exec_ir_owner_live_span PROPERTIES
        LABELS "ssa;owner;live-span;test-ledger" TIMEOUT 180
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
if (DEFINED ZR_SSA_OWNER_LIVE_SPAN_SOURCE_ROOT)
    if (DEFINED ZR_SSA_OWNER_LIVE_SPAN_MAP_NAME)
        if (NOT ZR_SSA_OWNER_LIVE_SPAN_MAP_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9_.-]*\\.map$")
            message(FATAL_ERROR "Live-span map must be a basename inside the build directory")
        endif ()
        if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            target_link_options(zr_vm_ssa_exec_ir_owner_live_span_test PRIVATE
                    /INCREMENTAL:NO "/MAP:${CMAKE_BINARY_DIR}/${ZR_SSA_OWNER_LIVE_SPAN_MAP_NAME}")
        else ()
            target_link_options(zr_vm_ssa_exec_ir_owner_live_span_test PRIVATE
                    "-Wl,-Map,${CMAKE_BINARY_DIR}/${ZR_SSA_OWNER_LIVE_SPAN_MAP_NAME}")
        endif ()
    endif ()
    set(_zr_live_span_graph "target\tabsolute_source\tsha256\n")
    get_target_property(_zr_live_span_sources zr_vm_ssa_exec_ir_owner_live_span_test SOURCES)
    foreach (_source IN LISTS _zr_live_span_sources)
        file(SHA256 "${_source}" _hash)
        string(APPEND _zr_live_span_graph "zr_vm_ssa_exec_ir_owner_live_span_test\t${_source}\t${_hash}\n")
    endforeach ()
    set(_zr_live_span_settings "compiler\t${CMAKE_C_COMPILER}\ncompiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\ngenerator\t${CMAKE_GENERATOR}\nbuild_type\t${CMAKE_BUILD_TYPE}\nlinker\t${CMAKE_LINKER}\ntoolchain_file\t${CMAKE_TOOLCHAIN_FILE}\nmake_rules_override_c\t${CMAKE_USER_MAKE_RULES_OVERRIDE_C}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\nubsan\t${ZR_SSA_OWNER_LIVE_SPAN_UBSAN}\n")
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
            LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS
            MSVC_RUNTIME_LIBRARY)
        get_target_property(_value zr_vm_ssa_exec_ir_owner_live_span_test ${_property})
        if (_value STREQUAL "_value-NOTFOUND")
            set(_value "")
        endif ()
        string(APPEND _zr_live_span_settings "zr_vm_ssa_exec_ir_owner_live_span_test.${_property}\t${_value}\n")
    endforeach ()
    file(WRITE "${CMAKE_BINARY_DIR}/owner-live-span-target-sources.tsv" "${_zr_live_span_graph}")
    file(WRITE "${CMAKE_BINARY_DIR}/owner-live-span-settings.tsv" "${_zr_live_span_settings}")
endif ()
