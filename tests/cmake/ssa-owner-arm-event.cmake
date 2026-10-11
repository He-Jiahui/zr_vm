# Actual owner allocator and Unity only; phase selects the expected raw protocol.
if (TARGET zr_vm_ssa_owner_arm_event_test)
    return()
endif ()
if (DEFINED ZR_SSA_OWNER_ARM_EVENT_SOURCE_ROOT)
    set(_zr_arm_event_root "${ZR_SSA_OWNER_ARM_EVENT_SOURCE_ROOT}")
else ()
    set(_zr_arm_event_root "${CMAKE_SOURCE_DIR}")
endif ()
set(ZR_SSA_OWNER_ARM_EVENT_EXPECTED_PHASE "GREEN" CACHE STRING "Reviewed owner observer protocol: RED or GREEN")
set_property(CACHE ZR_SSA_OWNER_ARM_EVENT_EXPECTED_PHASE PROPERTY STRINGS RED GREEN)
if (NOT ZR_SSA_OWNER_ARM_EVENT_EXPECTED_PHASE MATCHES "^(RED|GREEN)$")
    message(FATAL_ERROR "Select the exact reviewed RED or GREEN owner observer protocol")
endif ()
add_executable(zr_vm_ssa_owner_arm_event_test
        "${_zr_arm_event_root}/tests/core/test_ssa_owner_arm_event.c"
        "${_zr_arm_event_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_arm_event_root}/tests/third_party/zr_unity/Unity/src/unity.c")
target_include_directories(zr_vm_ssa_owner_arm_event_test PRIVATE
        "${_zr_arm_event_root}/tests"
        "${_zr_arm_event_root}/tests/core/support"
        "${_zr_arm_event_root}/tests/third_party/zr_unity/Unity/src")
target_compile_definitions(zr_vm_ssa_owner_arm_event_test PRIVATE
        _CRT_SECURE_NO_WARNINGS UNITY_INCLUDE_CONFIG_H UNITY_USE_FLUSH_STDOUT)
set_target_properties(zr_vm_ssa_owner_arm_event_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
        MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(zr_vm_ssa_owner_arm_event_test PRIVATE /W4 /UNDEBUG)
else ()
    target_compile_options(zr_vm_ssa_owner_arm_event_test PRIVATE -Wall -Wextra -UNDEBUG)
endif ()
if (ZR_SSA_OWNER_ARM_EVENT_UBSAN)
    if (NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "Finite owner observer UBSan requires Clang")
    endif ()
    if (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(zr_vm_ssa_owner_arm_event_test PRIVATE
                /clang:-fsanitize=undefined /clang:-fno-sanitize-recover=all)
    else ()
        target_compile_options(zr_vm_ssa_owner_arm_event_test PRIVATE
                -fsanitize=undefined -fno-sanitize-recover=all)
        target_link_options(zr_vm_ssa_owner_arm_event_test PRIVATE
                -fsanitize=undefined -fno-sanitize-recover=all)
    endif ()
endif ()
add_test(NAME ssa_owner_arm_event
        COMMAND "${CMAKE_COMMAND}"
        "-DZR_ARM_EVENT_EXECUTABLE=$<TARGET_FILE:zr_vm_ssa_owner_arm_event_test>"
        "-DZR_ARM_EVENT_EXPECTED_PHASE=${ZR_SSA_OWNER_ARM_EVENT_EXPECTED_PHASE}"
        "-DZR_ARM_EVENT_OUTPUT_DIR=${CMAKE_BINARY_DIR}"
        -P "${_zr_arm_event_root}/tests/cmake/ssa-owner-arm-event-check.cmake")
set_tests_properties(ssa_owner_arm_event PROPERTIES
        LABELS "ssa;owner;arm-event;expected-negative" TIMEOUT 180
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
if (DEFINED ZR_SSA_OWNER_ARM_EVENT_SOURCE_ROOT)
    if (DEFINED ZR_SSA_OWNER_ARM_EVENT_MAP_NAME)
        if (NOT ZR_SSA_OWNER_ARM_EVENT_MAP_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9_.-]*\\.map$")
            message(FATAL_ERROR "Owner observer map must be a basename in the build directory")
        endif ()
        if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            target_link_options(zr_vm_ssa_owner_arm_event_test PRIVATE
                    /INCREMENTAL:NO "/MAP:${CMAKE_BINARY_DIR}/${ZR_SSA_OWNER_ARM_EVENT_MAP_NAME}")
        else ()
            target_link_options(zr_vm_ssa_owner_arm_event_test PRIVATE
                    "-Wl,-Map,${CMAKE_BINARY_DIR}/${ZR_SSA_OWNER_ARM_EVENT_MAP_NAME}")
        endif ()
    endif ()
    set(_zr_arm_event_graph "target\tabsolute_source\tsha256\n")
    get_target_property(_zr_arm_event_sources zr_vm_ssa_owner_arm_event_test SOURCES)
    list(LENGTH _zr_arm_event_sources _zr_arm_event_source_count)
    if (NOT _zr_arm_event_source_count EQUAL 3)
        message(FATAL_ERROR "Owner observer gate requires exactly three C translation units")
    endif ()
    foreach (_source IN LISTS _zr_arm_event_sources)
        file(SHA256 "${_source}" _hash)
        string(APPEND _zr_arm_event_graph "zr_vm_ssa_owner_arm_event_test\t${_source}\t${_hash}\n")
    endforeach ()
    set(_zr_arm_event_settings "compiler\t${CMAKE_C_COMPILER}\ncompiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\ngenerator\t${CMAKE_GENERATOR}\nbuild_type\t${CMAKE_BUILD_TYPE}\nlinker\t${CMAKE_LINKER}\ntoolchain_file\t${CMAKE_TOOLCHAIN_FILE}\nmake_rules_override_c\t${CMAKE_USER_MAKE_RULES_OVERRIDE_C}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\nubsan\t${ZR_SSA_OWNER_ARM_EVENT_UBSAN}\nexpected_phase\t${ZR_SSA_OWNER_ARM_EVENT_EXPECTED_PHASE}\n")
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
            LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
        get_target_property(_value zr_vm_ssa_owner_arm_event_test ${_property})
        if (_value STREQUAL "_value-NOTFOUND")
            set(_value "")
        endif ()
        string(APPEND _zr_arm_event_settings "zr_vm_ssa_owner_arm_event_test.${_property}\t${_value}\n")
    endforeach ()
    file(WRITE "${CMAKE_BINARY_DIR}/owner-arm-event-target-sources.tsv" "${_zr_arm_event_graph}")
    file(WRITE "${CMAKE_BINARY_DIR}/owner-arm-event-settings.tsv" "${_zr_arm_event_settings}")
endif ()
