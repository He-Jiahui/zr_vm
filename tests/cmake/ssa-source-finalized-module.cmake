# Snapshot declared finalized primitive witnesses through the real Core owners.
if (TARGET zr_vm_ssa_source_finalized_module_test)
    return()
endif ()
if (DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT)
    set(_zr_finalized_root "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}")
else ()
    set(_zr_finalized_root "${CMAKE_SOURCE_DIR}")
endif ()
include(${CMAKE_CURRENT_LIST_DIR}/ssa-execir-owned-module.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/ssa-execir-module-copy.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/ssa-primitive-frame-validation.cmake)

# Query and lookup use this target's allocator callbacks, supplied by its ledger.
add_library(zr_vm_ssa_source_finalized_module_private OBJECT
        "${_zr_finalized_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module.c"
        "${_zr_finalized_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module_validation.c"
        "${_zr_finalized_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_callable_query.c"
        "${_zr_finalized_root}/zr_vm_parser/src/zr_vm_parser/canonical_type_lookup.c")
add_executable(zr_vm_ssa_source_finalized_module_test
        "${_zr_finalized_root}/tests/parser/test_ssa_source_finalized_module.c"
        "${_zr_finalized_root}/tests/parser/support/ssa_source_finalized_module_fixture.c"
        "${_zr_finalized_root}/tests/parser/support/ssa_source_finalized_module_witness.c"
        "${_zr_finalized_root}/tests/parser/support/ssa_source_finalized_module_faults.c"
        "${_zr_finalized_root}/tests/core/support/ssa_exec_ir_owned_module_fixture.c"
        "${_zr_finalized_root}/tests/core/support/ssa_exec_ir_owner_capture.c"
        "${_zr_finalized_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_finalized_root}/tests/third_party/zr_unity/Unity/src/unity.c")
foreach (_target IN ITEMS zr_vm_ssa_source_finalized_module_private
        zr_vm_ssa_source_finalized_module_test)
    target_include_directories(${_target} PRIVATE
            "${_zr_finalized_root}/zr_vm_common/include"
            "${_zr_finalized_root}/zr_vm_core/include"
            "${_zr_finalized_root}/zr_vm_parser/include"
            "${_zr_finalized_root}/zr_vm_parser/src"
            "${_zr_finalized_root}/tests"
            "${_zr_finalized_root}/tests/core/support"
            "${_zr_finalized_root}/tests/third_party/zr_unity/Unity/src")
    target_compile_definitions(${_target} PRIVATE
            ZR_LIBRARY_TYPE_STATIC=1 _CRT_SECURE_NO_WARNINGS)
    set_target_properties(${_target} PROPERTIES C_STANDARD 11
            C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
            MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    if (COMMAND zr_ssa_execir_owner_apply_settings)
        zr_ssa_execir_owner_apply_settings(${_target})
    endif ()
endforeach ()
target_compile_definitions(zr_vm_ssa_source_finalized_module_private PRIVATE
        ZR_CURRENT_MODULE="zr_vm_parser")
target_compile_definitions(zr_vm_ssa_source_finalized_module_test PRIVATE
        ZR_CURRENT_MODULE="zr_vm_ssa_source_finalized_module_test"
        UNITY_INCLUDE_CONFIG_H UNITY_INCLUDE_DOUBLE UNITY_DOUBLE_PRECISION=1e-6
        UNITY_USE_FLUSH_STDOUT)
if (MSVC)
    target_compile_options(zr_vm_ssa_source_finalized_module_private PRIVATE
            "/FI${_zr_finalized_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
    target_link_options(zr_vm_ssa_source_finalized_module_test PRIVATE /STACK:8388608)
else ()
    target_compile_options(zr_vm_ssa_source_finalized_module_private PRIVATE
            -include "${_zr_finalized_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
endif ()
target_link_libraries(zr_vm_ssa_source_finalized_module_test PRIVATE
        $<TARGET_OBJECTS:zr_vm_ssa_execir_owner_private_core>
        $<TARGET_OBJECTS:zr_vm_ssa_execir_module_copy_private>
        $<TARGET_OBJECTS:zr_vm_ssa_primitive_frame_validation_private>
        $<TARGET_OBJECTS:zr_vm_ssa_source_finalized_module_private>)
add_test(NAME ssa_source_finalized_module COMMAND zr_vm_ssa_source_finalized_module_test)
set_tests_properties(ssa_source_finalized_module PROPERTIES
        LABELS "ssa;source-finalized-module;declared-consistency" TIMEOUT 180
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
if (DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT)
    set(ZR_SSA_SOURCE_FINALIZED_MODULE_MAP_NAME "ssa-source-finalized-module-link.map"
            CACHE STRING "Finite finalized module map basename")
    if (NOT ZR_SSA_SOURCE_FINALIZED_MODULE_MAP_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9_.-]*\\.map$")
        message(FATAL_ERROR "Finalized module map must be a basename in the build directory")
    endif ()
    target_link_options(zr_vm_ssa_source_finalized_module_test PRIVATE
            /INCREMENTAL:NO /OPT:REF /VERBOSE:LIB
            "/MAP:${CMAKE_BINARY_DIR}/${ZR_SSA_SOURCE_FINALIZED_MODULE_MAP_NAME}")
    set(_finalized_graph "target\tabsolute_source\tsha256\n")
    set(_finalized_settings "compiler\t${CMAKE_C_COMPILER}\ncompiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\ngenerator\t${CMAKE_GENERATOR}\nbuild_type\t${CMAKE_BUILD_TYPE}\nlinker\t${CMAKE_LINKER}\ntoolchain_file\t${CMAKE_TOOLCHAIN_FILE}\nmake_rules_override_c\t${CMAKE_USER_MAKE_RULES_OVERRIDE_C}\narchive_tool\t${CMAKE_AR}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\n")
    foreach (_target IN ITEMS zr_vm_ssa_execir_owner_private_core
            zr_vm_ssa_execir_module_copy_private zr_vm_ssa_primitive_frame_validation_private
            zr_vm_ssa_source_finalized_module_private zr_vm_ssa_source_finalized_module_test)
        get_target_property(_sources ${_target} SOURCES)
        foreach (_source IN LISTS _sources)
            file(SHA256 "${_source}" _hash)
            string(APPEND _finalized_graph "${_target}\t${_source}\t${_hash}\n")
        endforeach ()
        foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS
                COMPILE_OPTIONS LINK_OPTIONS LINK_LIBRARIES C_STANDARD
                C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
            get_target_property(_value ${_target} ${_property})
            if (_value STREQUAL "_value-NOTFOUND")
                set(_value "")
            endif ()
            string(APPEND _finalized_settings "${_target}.${_property}\t${_value}\n")
        endforeach ()
    endforeach ()
    file(WRITE "${CMAKE_BINARY_DIR}/source-finalized-module-target-sources.tsv" "${_finalized_graph}")
    file(WRITE "${CMAKE_BINARY_DIR}/source-finalized-module-settings.tsv" "${_finalized_settings}")
endif ()
