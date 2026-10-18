# Inactive finite metadata target. Root includes this only after independent review.
if (TARGET zr_vm_ssa_gc_map_parser_consumers_test)
    return()
endif ()
if (NOT TARGET zr_vm_ssa_execir_owner_private_core OR
        NOT DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT OR
        NOT COMMAND zr_ssa_execir_owner_apply_settings)
    message(FATAL_ERROR "Parser GC-map consumers require the finite owner Core15 route")
endif ()
set(_zr_parser_gc_root "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}")
# Actual Core private headers, including mutation declarations, are explicit.
target_include_directories(zr_vm_ssa_execir_owner_private_core PRIVATE
        "${_zr_parser_gc_root}/zr_vm_core/src/zr_vm_core")
set(_zr_parser_gc_providers
        exec_ir/exec_ir_pass_manager.c
        exec_ir/passes/exec_ir_dce.c
        exec_ir/passes/exec_ir_sccp.c
        exec_ir/passes/exec_ir_sccp_owner.c
        exec_ir/passes/exec_ir_constant_materialize.c
        exec_ir/passes/exec_ir_copy_aliases.c
        exec_ir/exec_ir_cfg.c
        exec_ir/exec_ir_effects.c
        exec_ir/exec_ir_effects_linear.c
        exec_ir/exec_ir_effect_loops.c
        exec_ir/exec_ir_state_maps.c
        exec_ir/execbc/exec_ir_fusion_contract.c
        exec_ir/passes/exec_ir_container_specialize.c)
list(TRANSFORM _zr_parser_gc_providers PREPEND
        "${_zr_parser_gc_root}/zr_vm_parser/src/zr_vm_parser/")
add_library(zr_vm_ssa_gc_map_parser_consumers_private OBJECT
        ${_zr_parser_gc_providers}
        "${_zr_parser_gc_root}/zr_vm_core/src/zr_vm_core/object/container_storage_contract.c")
add_executable(zr_vm_ssa_gc_map_parser_consumers_test
        "${_zr_parser_gc_root}/tests/parser/test_ssa_gc_map_parser_consumers.c"
        "${_zr_parser_gc_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_parser_gc_root}/tests/third_party/zr_unity/Unity/src/unity.c")
foreach (_target IN ITEMS zr_vm_ssa_gc_map_parser_consumers_private
        zr_vm_ssa_gc_map_parser_consumers_test)
    target_include_directories(${_target} PRIVATE
            "${_zr_parser_gc_root}"
            "${_zr_parser_gc_root}/zr_vm_common/include"
            "${_zr_parser_gc_root}/zr_vm_core/include"
            "${_zr_parser_gc_root}/zr_vm_core/src/zr_vm_core"
            "${_zr_parser_gc_root}/zr_vm_parser/include"
            "${_zr_parser_gc_root}/zr_vm_parser/src/zr_vm_parser"
            "${_zr_parser_gc_root}/tests"
            "${_zr_parser_gc_root}/tests/core/support"
            "${_zr_parser_gc_root}/tests/third_party/zr_unity/Unity/src")
    target_compile_definitions(${_target} PRIVATE
            ZR_LIBRARY_TYPE_STATIC=1 _CRT_SECURE_NO_WARNINGS)
    set_target_properties(${_target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
            C_EXTENSIONS OFF MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    zr_ssa_execir_owner_apply_settings(${_target})
    if (MSVC)
        target_compile_options(${_target} PRIVATE /UNDEBUG)
    else ()
        target_compile_options(${_target} PRIVATE -UNDEBUG)
    endif ()
endforeach ()
target_compile_definitions(zr_vm_ssa_gc_map_parser_consumers_private PRIVATE
        ZR_CURRENT_MODULE="zr_vm_gc_map_parser_providers")
target_compile_definitions(zr_vm_ssa_gc_map_parser_consumers_test PRIVATE
        ZR_CURRENT_MODULE="zr_vm_ssa_gc_map_parser_consumers_test"
        UNITY_INCLUDE_CONFIG_H UNITY_USE_FLUSH_STDOUT)
# Forced inclusion redirects real providers only. The allocator TU defines
# NO_REDIRECT itself, and Unity is compiled independently without the shim.
if (MSVC)
    target_compile_options(zr_vm_ssa_gc_map_parser_consumers_private PRIVATE
            "/FI${_zr_parser_gc_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
    target_link_options(zr_vm_ssa_gc_map_parser_consumers_test PRIVATE
            /INCREMENTAL:NO /OPT:NOREF /OPT:NOICF /VERBOSE:LIB
            "/MAP:${CMAKE_BINARY_DIR}/ssa-gc-map-parser-consumers.map")
else ()
    target_compile_options(zr_vm_ssa_gc_map_parser_consumers_private PRIVATE
            -include "${_zr_parser_gc_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
    target_link_options(zr_vm_ssa_gc_map_parser_consumers_test PRIVATE
            "-Wl,-Map,${CMAKE_BINARY_DIR}/ssa-gc-map-parser-consumers.map")
endif ()
# Owner settings carry the registered UBSan driver flags before /link on
# clang-cl; MSVC cl.exe is the separate compatibility smoke configuration.
if (NOT MSVC AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    foreach (_target IN ITEMS zr_vm_ssa_gc_map_parser_consumers_private
            zr_vm_ssa_gc_map_parser_consumers_test)
        target_compile_options(${_target} PRIVATE
                -fsanitize=undefined -fno-sanitize-recover=all)
    endforeach ()
    target_link_options(zr_vm_ssa_gc_map_parser_consumers_test PRIVATE
            -fsanitize=undefined -fno-sanitize-recover=all)
endif ()
target_link_libraries(zr_vm_ssa_gc_map_parser_consumers_test PRIVATE
        zr_vm_ssa_gc_map_parser_consumers_private zr_vm_ssa_execir_owner_private_core)
if (WIN32)
    target_link_libraries(zr_vm_ssa_gc_map_parser_consumers_test PRIVATE kernel32)
endif ()
add_test(NAME ssa_gc_map_parser_consumers COMMAND zr_vm_ssa_gc_map_parser_consumers_test)
set_tests_properties(ssa_gc_map_parser_consumers PROPERTIES
        LABELS "ssa;parser;metadata-validation" TIMEOUT 120
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")

# Finite source and effective settings evidence; no glob or runtime archive.
set(_zr_parser_gc_graph "target\tabsolute_source\tsha256\n")
set(_zr_parser_gc_settings "compiler\t${CMAKE_C_COMPILER}\ncompiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\ngenerator\t${CMAKE_GENERATOR}\nbuild_type\t${CMAKE_BUILD_TYPE}\nlinker\t${CMAKE_LINKER}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\n")
foreach (_target IN ITEMS zr_vm_ssa_gc_map_parser_consumers_private
        zr_vm_ssa_gc_map_parser_consumers_test zr_vm_ssa_execir_owner_private_core)
    get_target_property(_sources ${_target} SOURCES)
    foreach (_source IN LISTS _sources)
        get_filename_component(_absolute "${_source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(SHA256 "${_absolute}" _hash)
        string(APPEND _zr_parser_gc_graph "${_target}\t${_absolute}\t${_hash}\n")
    endforeach ()
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
            LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
        get_target_property(_value ${_target} ${_property})
        if (_value STREQUAL "_value-NOTFOUND")
            set(_value "")
        endif ()
        string(APPEND _zr_parser_gc_settings "${_target}.${_property}\t${_value}\n")
    endforeach ()
endforeach ()
file(WRITE "${CMAKE_BINARY_DIR}/gc-map-parser-consumers-target-sources.tsv" "${_zr_parser_gc_graph}")
file(WRITE "${CMAKE_BINARY_DIR}/gc-map-parser-consumers-settings.tsv" "${_zr_parser_gc_settings}")
