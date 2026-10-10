# Finite actual BuildModule/owned-module/verifier gate. No module archives,
# redirected providers, Source compiler, runtime bootstrap or execution oracle.
if (TARGET zr_vm_ssa_retained_builder_cfg_test)
    return()
endif ()
if (DEFINED ZR_SSA_RETAINED_BUILDER_SOURCE_ROOT)
    set(_zr_retained_cfg_root "${ZR_SSA_RETAINED_BUILDER_SOURCE_ROOT}")
else ()
    set(_zr_retained_cfg_root "${CMAKE_SOURCE_DIR}")
endif ()
set(_zr_retained_cfg_production_sources
    "${_zr_retained_cfg_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c"
    "${_zr_retained_cfg_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_compare.c"
    "${_zr_retained_cfg_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c"
    "${_zr_retained_cfg_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects_linear.c"
    "${_zr_retained_cfg_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c"
    "${_zr_retained_cfg_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_control_edges.c"
    "${_zr_retained_cfg_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c"
    "${_zr_retained_cfg_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_normalize_cfg.c"
    "${_zr_retained_cfg_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_place_eligibility.c"
    "${_zr_retained_cfg_root}/zr_vm_parser/src/zr_vm_parser/semantic_ir_scalar_scratch.c"
    "${_zr_retained_cfg_root}/zr_vm_parser/src/zr_vm_parser/place_queries.c"
    "${_zr_retained_cfg_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c"
    "${_zr_retained_cfg_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c"
    "${_zr_retained_cfg_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/execution_contract.c"
    "${_zr_retained_cfg_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owned_module.c")
list(LENGTH _zr_retained_cfg_production_sources _zr_retained_cfg_production_count)
if (NOT _zr_retained_cfg_production_count EQUAL 29)
    message(FATAL_ERROR "Retained builder gate requires exactly 29 explicit production TUs")
endif ()
add_executable(zr_vm_ssa_retained_builder_cfg_test
    "${_zr_retained_cfg_root}/tests/parser/test_ssa_retained_builder_cfg.c"
    "${_zr_retained_cfg_root}/tests/parser/support/ssa_retained_builder_cfg_fixture.c"
    ${_zr_retained_cfg_production_sources})
target_include_directories(zr_vm_ssa_retained_builder_cfg_test PRIVATE
    "${_zr_retained_cfg_root}/zr_vm_parser/include"
    "${_zr_retained_cfg_root}/zr_vm_core/include"
    "${_zr_retained_cfg_root}/zr_vm_common/include")
target_compile_definitions(zr_vm_ssa_retained_builder_cfg_test PRIVATE ZR_LIBRARY_TYPE_STATIC=1)
set_target_properties(zr_vm_ssa_retained_builder_cfg_test PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(zr_vm_ssa_retained_builder_cfg_test PRIVATE /W4 /UNDEBUG)
else ()
    target_compile_options(zr_vm_ssa_retained_builder_cfg_test PRIVATE -Wall -Wextra -UNDEBUG)
endif ()
if (WIN32)
    target_compile_definitions(zr_vm_ssa_retained_builder_cfg_test PRIVATE
        ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1 ZR_DEBUG=1 _CRT_SECURE_NO_WARNINGS)
endif ()
if (ZR_SSA_RETAINED_BUILDER_UBSAN)
    if (NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "Finite retained builder UBSan requires Clang")
    endif ()
    if (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(zr_vm_ssa_retained_builder_cfg_test PRIVATE
            /clang:-fsanitize=undefined /clang:-fno-sanitize-recover=all)
    else ()
        target_compile_options(zr_vm_ssa_retained_builder_cfg_test PRIVATE
            -fsanitize=undefined -fno-sanitize-recover=all)
        target_link_options(zr_vm_ssa_retained_builder_cfg_test PRIVATE
            -fsanitize=undefined -fno-sanitize-recover=all)
    endif ()
endif ()
add_test(NAME ssa_retained_builder_cfg COMMAND zr_vm_ssa_retained_builder_cfg_test)
set_tests_properties(ssa_retained_builder_cfg PROPERTIES
    LABELS "ssa;builder;retained;cfg;effects" TIMEOUT 180
    ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
if (DEFINED ZR_SSA_RETAINED_BUILDER_SOURCE_ROOT)
    if (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_link_options(zr_vm_ssa_retained_builder_cfg_test PRIVATE
            /INCREMENTAL:NO "/MAP:${CMAKE_BINARY_DIR}/retained-builder-cfg.map")
    else ()
        target_link_options(zr_vm_ssa_retained_builder_cfg_test PRIVATE
            "-Wl,-Map,${CMAKE_BINARY_DIR}/retained-builder-cfg.map")
    endif ()
    set(_zr_retained_cfg_graph "target\tabsolute_source\tsha256\n")
    get_target_property(_zr_retained_cfg_sources zr_vm_ssa_retained_builder_cfg_test SOURCES)
    foreach (_source IN LISTS _zr_retained_cfg_sources)
        file(SHA256 "${_source}" _hash)
        string(APPEND _zr_retained_cfg_graph "zr_vm_ssa_retained_builder_cfg_test\t${_source}\t${_hash}\n")
    endforeach ()
    set(_zr_retained_cfg_settings "compiler\t${CMAKE_C_COMPILER}\ncompiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\ngenerator\t${CMAKE_GENERATOR}\nbuild_type\t${CMAKE_BUILD_TYPE}\nlinker\t${CMAKE_LINKER}\ntoolchain_file\t${CMAKE_TOOLCHAIN_FILE}\nmake_rules_override_c\t${CMAKE_USER_MAKE_RULES_OVERRIDE_C}\narchive_tool\t${CMAKE_AR}\narchive_rule\t${CMAKE_C_CREATE_STATIC_LIBRARY}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\nubsan\t${ZR_SSA_RETAINED_BUILDER_UBSAN}\n")
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
             LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
        get_target_property(_value zr_vm_ssa_retained_builder_cfg_test ${_property})
        if (_value STREQUAL "_value-NOTFOUND")
            set(_value "")
        endif ()
        string(APPEND _zr_retained_cfg_settings "zr_vm_ssa_retained_builder_cfg_test.${_property}\t${_value}\n")
    endforeach ()
    file(WRITE "${CMAKE_BINARY_DIR}/retained-builder-target-sources.tsv" "${_zr_retained_cfg_graph}")
    file(WRITE "${CMAKE_BINARY_DIR}/retained-builder-settings.tsv" "${_zr_retained_cfg_settings}")
endif ()
