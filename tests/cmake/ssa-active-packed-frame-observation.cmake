# Actual packed-frame builder, explicit Core providers and Unity; phase selects only raw expectations.
if (TARGET zr_vm_ssa_active_packed_frame_observation_test)
    return()
endif ()
if (DEFINED ZR_SSA_ACTIVE_PACKED_FRAME_SOURCE_ROOT)
    set(_zr_active_frame_root "${ZR_SSA_ACTIVE_PACKED_FRAME_SOURCE_ROOT}")
else ()
    set(_zr_active_frame_root "${CMAKE_SOURCE_DIR}")
endif ()
set(ZR_SSA_ACTIVE_PACKED_FRAME_EXPECTED_PHASE "GREEN" CACHE STRING "Reviewed active packed-frame protocol: RED or GREEN")
set_property(CACHE ZR_SSA_ACTIVE_PACKED_FRAME_EXPECTED_PHASE PROPERTY STRINGS RED GREEN)
if (NOT ZR_SSA_ACTIVE_PACKED_FRAME_EXPECTED_PHASE MATCHES "^(RED|GREEN)$")
    message(FATAL_ERROR "Select the exact reviewed RED or GREEN active packed-frame protocol")
endif ()
add_executable(zr_vm_ssa_active_packed_frame_observation_test
        "${_zr_active_frame_root}/tests/parser/test_ssa_active_packed_frame_observation.c"
        "${_zr_active_frame_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c"
        "${_zr_active_frame_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c"
        "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c"
        "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c"
        "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c"
        "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c"
        "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c"
        "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c"
        "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c"
        "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c"
        "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c"
        "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c"
        "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c"
        "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c"
        "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c"
        "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/runtime/execution_contract.c"
        "${_zr_active_frame_root}/tests/third_party/zr_unity/Unity/src/unity.c")
target_include_directories(zr_vm_ssa_active_packed_frame_observation_test PRIVATE
        "${_zr_active_frame_root}/zr_vm_parser/src/zr_vm_parser"
        "${_zr_active_frame_root}/tests"
        "${_zr_active_frame_root}/tests/third_party/zr_unity/Unity/src"
        "${_zr_active_frame_root}/zr_vm_common/include"
        "${_zr_active_frame_root}/zr_vm_core/include"
        "${_zr_active_frame_root}/zr_vm_parser/include")
target_compile_definitions(zr_vm_ssa_active_packed_frame_observation_test PRIVATE
        ZR_LIBRARY_TYPE_STATIC=1 ZR_DEBUG=1 _CRT_SECURE_NO_WARNINGS
        UNITY_INCLUDE_CONFIG_H UNITY_USE_FLUSH_STDOUT)
if (WIN32)
    target_compile_definitions(zr_vm_ssa_active_packed_frame_observation_test PRIVATE ZR_PLATFORM_WIN=1)
    if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_definitions(zr_vm_ssa_active_packed_frame_observation_test PRIVATE ZR_PLATFORM_WIN_USE_MSVC=1)
    endif ()
endif ()
set_target_properties(zr_vm_ssa_active_packed_frame_observation_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
        MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(zr_vm_ssa_active_packed_frame_observation_test PRIVATE /W4 /UNDEBUG)
else ()
    target_compile_options(zr_vm_ssa_active_packed_frame_observation_test PRIVATE -Wall -Wextra -UNDEBUG)
endif ()
if (ZR_SSA_ACTIVE_PACKED_FRAME_UBSAN)
    if (NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "Finite active packed-frame UBSan requires Clang")
    endif ()
    if (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(zr_vm_ssa_active_packed_frame_observation_test PRIVATE
                /clang:-fsanitize=undefined /clang:-fno-sanitize-recover=all)
    else ()
        target_compile_options(zr_vm_ssa_active_packed_frame_observation_test PRIVATE
                -fsanitize=undefined -fno-sanitize-recover=all)
        target_link_options(zr_vm_ssa_active_packed_frame_observation_test PRIVATE
                -fsanitize=undefined -fno-sanitize-recover=all)
    endif ()
endif ()
add_test(NAME ssa_active_packed_frame_observation
        COMMAND "${CMAKE_COMMAND}"
        "-DZR_ACTIVE_FRAME_EXECUTABLE=$<TARGET_FILE:zr_vm_ssa_active_packed_frame_observation_test>"
        "-DZR_ACTIVE_FRAME_EXPECTED_PHASE=${ZR_SSA_ACTIVE_PACKED_FRAME_EXPECTED_PHASE}"
        "-DZR_ACTIVE_FRAME_OUTPUT_DIR=${CMAKE_BINARY_DIR}"
        -P "${_zr_active_frame_root}/tests/cmake/ssa-active-packed-frame-observation-check.cmake")
set_tests_properties(ssa_active_packed_frame_observation PROPERTIES
        LABELS "ssa;frame;active-observation;expected-negative" TIMEOUT 180
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1;ZR_VM_PROFILE_INSTRUCTIONS=0;ZR_VM_PROFILE_SLOWPATHS=0;ZR_VM_PROFILE_HELPERS=0;ZR_VM_PROFILE_MEMORY=0")
if (DEFINED ZR_SSA_ACTIVE_PACKED_FRAME_SOURCE_ROOT)
    # Separate original plain assert/CHECK regressions. Root selects these only
    # for GREEN; configuration does not imply either was built or executed.
    set(_zr_active_frame_core_sources)
    foreach (_name IN ITEMS exec_ir exec_ir_binding_rows exec_ir_verify exec_ir_verify_constants
            exec_ir_verify_ssa exec_ir_verify_effects exec_ir_verify_effect_backedges
            exec_ir_materialize exec_ir_materialize_owners exec_ir_state_map_liveness
            exec_ir_deopt_aggregate exec_ir_state_map_storage exec_ir_owner_state)
        list(APPEND _zr_active_frame_core_sources
                "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/exec_ir/${_name}.c")
    endforeach ()
    list(APPEND _zr_active_frame_core_sources
            "${_zr_active_frame_root}/zr_vm_core/src/zr_vm_core/runtime/execution_contract.c")
    add_executable(zr_vm_ssa_active_frame_legacy_layout_test
            "${_zr_active_frame_root}/tests/parser/test_ssa_frame_layout.c"
            "${_zr_active_frame_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c"
            ${_zr_active_frame_core_sources})
    add_executable(zr_vm_ssa_active_frame_legacy_roots_test
            "${_zr_active_frame_root}/tests/parser/test_ssa_roots_observation.c"
            "${_zr_active_frame_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c"
            "${_zr_active_frame_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c"
            ${_zr_active_frame_core_sources})
    foreach (_target IN ITEMS zr_vm_ssa_active_frame_legacy_layout_test zr_vm_ssa_active_frame_legacy_roots_test)
        target_include_directories(${_target} PRIVATE
                "${_zr_active_frame_root}/zr_vm_common/include"
                "${_zr_active_frame_root}/zr_vm_core/include"
                "${_zr_active_frame_root}/zr_vm_parser/include")
        target_compile_definitions(${_target} PRIVATE ZR_LIBRARY_TYPE_STATIC=1 ZR_DEBUG=1
                ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1 _CRT_SECURE_NO_WARNINGS)
        set_target_properties(${_target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
                C_EXTENSIONS OFF MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
        target_compile_options(${_target} PRIVATE /W4 /UNDEBUG)
        if (ZR_SSA_ACTIVE_PACKED_FRAME_UBSAN)
            target_compile_options(${_target} PRIVATE
                    /clang:-fsanitize=undefined /clang:-fno-sanitize-recover=all)
        endif ()
        target_link_options(${_target} PRIVATE /INCREMENTAL:NO "/MAP:${CMAKE_BINARY_DIR}/${_target}-link.map")
    endforeach ()
    add_test(NAME ssa_active_frame_legacy_layout COMMAND "${CMAKE_COMMAND}" -E env
            --unset=ZR_VM_PROFILE_OUT --unset=ZR_VM_TRACE_CORE_BOOTSTRAP --unset=ZR_VM_TRACE_PROJECT_STARTUP
            $<TARGET_FILE:zr_vm_ssa_active_frame_legacy_layout_test>)
    add_test(NAME ssa_active_frame_legacy_roots COMMAND "${CMAKE_COMMAND}" -E env
            --unset=ZR_VM_PROFILE_OUT --unset=ZR_VM_TRACE_CORE_BOOTSTRAP --unset=ZR_VM_TRACE_PROJECT_STARTUP
            $<TARGET_FILE:zr_vm_ssa_active_frame_legacy_roots_test>)
    set_tests_properties(ssa_active_frame_legacy_layout ssa_active_frame_legacy_roots PROPERTIES
            LABELS "ssa;frame;legacy-regression" TIMEOUT 120
            ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1;ZR_VM_PROFILE_INSTRUCTIONS=0;ZR_VM_PROFILE_SLOWPATHS=0;ZR_VM_PROFILE_HELPERS=0;ZR_VM_PROFILE_MEMORY=0")
    set(ZR_SSA_ACTIVE_PACKED_FRAME_MAP_NAME "ssa-active-packed-frame-observation-link.map"
            CACHE STRING "Finite active packed-frame link map basename")
    if (DEFINED ZR_SSA_ACTIVE_PACKED_FRAME_MAP_NAME)
        if (NOT ZR_SSA_ACTIVE_PACKED_FRAME_MAP_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9_.-]*\\.map$")
            message(FATAL_ERROR "Active packed-frame map must be a basename in the build directory")
        endif ()
        if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            target_link_options(zr_vm_ssa_active_packed_frame_observation_test PRIVATE
                    /INCREMENTAL:NO "/MAP:${CMAKE_BINARY_DIR}/${ZR_SSA_ACTIVE_PACKED_FRAME_MAP_NAME}")
        else ()
            target_link_options(zr_vm_ssa_active_packed_frame_observation_test PRIVATE
                    "-Wl,-Map,${CMAKE_BINARY_DIR}/${ZR_SSA_ACTIVE_PACKED_FRAME_MAP_NAME}")
        endif ()
    endif ()
    set(_zr_active_frame_graph "target\tabsolute_source\tsha256\n")
    set(_zr_active_frame_targets zr_vm_ssa_active_packed_frame_observation_test
            zr_vm_ssa_active_frame_legacy_layout_test zr_vm_ssa_active_frame_legacy_roots_test)
    foreach (_target IN LISTS _zr_active_frame_targets)
    get_target_property(_zr_active_frame_sources ${_target} SOURCES)
    list(LENGTH _zr_active_frame_sources _zr_active_frame_source_count)
    if (_target STREQUAL "zr_vm_ssa_active_packed_frame_observation_test")
        set(_expected_count 18)
    elseif (_target STREQUAL "zr_vm_ssa_active_frame_legacy_layout_test")
        set(_expected_count 16)
    else ()
        set(_expected_count 17)
    endif ()
    if (NOT _zr_active_frame_source_count EQUAL _expected_count)
        message(FATAL_ERROR "Active packed-frame manifest count mismatch for ${_target}")
    endif ()
    set(_zr_active_frame_seen)
    foreach (_source IN LISTS _zr_active_frame_sources)
        file(REAL_PATH "${_source}" _absolute)
        cmake_path(IS_PREFIX _zr_active_frame_root "${_absolute}" NORMALIZE _inside_checkout)
        if (NOT _inside_checkout OR NOT EXISTS "${_absolute}" OR _absolute IN_LIST _zr_active_frame_seen)
            message(FATAL_ERROR "Missing, duplicate or non-checkout active packed-frame source")
        endif ()
        list(APPEND _zr_active_frame_seen "${_absolute}")
        file(SHA256 "${_source}" _hash)
        string(APPEND _zr_active_frame_graph "${_target}\t${_source}\t${_hash}\n")
    endforeach ()
    endforeach ()
    set(_zr_active_frame_settings "compiler\t${CMAKE_C_COMPILER}\ncompiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\ngenerator\t${CMAKE_GENERATOR}\nbuild_type\t${CMAKE_BUILD_TYPE}\nlinker\t${CMAKE_LINKER}\ntoolchain_file\t${CMAKE_TOOLCHAIN_FILE}\nmake_rules_override_c\t${CMAKE_USER_MAKE_RULES_OVERRIDE_C}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\nubsan\t${ZR_SSA_ACTIVE_PACKED_FRAME_UBSAN}\nexpected_phase\t${ZR_SSA_ACTIVE_PACKED_FRAME_EXPECTED_PHASE}\n")
    string(APPEND _zr_active_frame_settings "configured_targets\t${_zr_active_frame_targets}\n")
    foreach (_target IN LISTS _zr_active_frame_targets)
    get_target_property(_sources ${_target} SOURCES)
    list(LENGTH _sources _count)
    string(APPEND _zr_active_frame_settings "${_target}.source_count\t${_count}\n")
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
            LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
        get_target_property(_value ${_target} ${_property})
        if (_value STREQUAL "_value-NOTFOUND")
            set(_value "")
        endif ()
        string(APPEND _zr_active_frame_settings "${_target}.${_property}\t${_value}\n")
    endforeach ()
    endforeach ()
    file(WRITE "${CMAKE_BINARY_DIR}/active-packed-frame-observation-target-sources.tsv" "${_zr_active_frame_graph}")
    file(WRITE "${CMAKE_BINARY_DIR}/active-packed-frame-observation-settings.tsv" "${_zr_active_frame_settings}")
    # Interface baseline only; the runtime gate separately admits every actual
    # latest selected header dependency before execution.
    set(_zr_active_frame_headers "absolute_header\tsha256\n")
    foreach (_relative IN ITEMS
            zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.h
            zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.h
            zr_vm_core/include/zr_vm_core/exec_ir.h
            zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
            zr_vm_core/include/zr_vm_core/execution_contract.h
            tests/unity_config.h
            tests/third_party/zr_unity/Unity/src/unity.h
            tests/third_party/zr_unity/Unity/src/unity_internals.h)
        file(SHA256 "${_zr_active_frame_root}/${_relative}" _hash)
        string(APPEND _zr_active_frame_headers "${_zr_active_frame_root}/${_relative}\t${_hash}\n")
    endforeach ()
    file(WRITE "${CMAKE_BINARY_DIR}/active-packed-frame-observation-header-hashes.tsv" "${_zr_active_frame_headers}")
endif ()
