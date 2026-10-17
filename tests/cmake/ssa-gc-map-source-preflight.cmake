# Finite Source storage admission witnesses; real providers only, no runtime archive.
if (TARGET zr_vm_ssa_gc_map_source_preflight_test)
    return()
endif ()
if (NOT DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT OR
        NOT COMMAND zr_ssa_execir_owner_apply_settings)
    message(FATAL_ERROR "Source GC-map preflight requires the finite owner configuration")
endif ()
set(_zr_sp_objects zr_vm_ssa_execir_owner_private_core
        zr_vm_ssa_execir_module_copy_private zr_vm_ssa_primitive_frame_validation_private
        zr_vm_ssa_source_finalized_module_private)
foreach (_target IN LISTS _zr_sp_objects)
    if (NOT TARGET ${_target})
        message(FATAL_ERROR "Source GC-map preflight missing real provider: ${_target}")
    endif ()
endforeach ()
set(_zr_sp_root "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}")
add_executable(zr_vm_ssa_gc_map_source_preflight_test
        "${_zr_sp_root}/tests/parser/test_ssa_gc_map_source_preflight.c"
        "${_zr_sp_root}/tests/parser/support/ssa_gc_map_source_preflight_fixture.c"
        "${_zr_sp_root}/tests/parser/support/ssa_primitive_frame_validation_fixture.c"
        "${_zr_sp_root}/tests/parser/support/ssa_source_finalized_module_fixture.c"
        "${_zr_sp_root}/tests/parser/support/ssa_source_finalized_module_witness.c"
        "${_zr_sp_root}/tests/parser/support/ssa_source_finalized_module_faults.c"
        "${_zr_sp_root}/tests/core/support/ssa_exec_ir_owned_module_fixture.c"
        "${_zr_sp_root}/tests/core/support/ssa_exec_ir_owner_capture.c"
        "${_zr_sp_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_sp_root}/tests/third_party/zr_unity/Unity/src/unity.c")
target_include_directories(zr_vm_ssa_gc_map_source_preflight_test PRIVATE
        "${_zr_sp_root}"
        "${_zr_sp_root}/zr_vm_common/include"
        "${_zr_sp_root}/zr_vm_core/include"
        "${_zr_sp_root}/zr_vm_core/src/zr_vm_core"
        "${_zr_sp_root}/zr_vm_parser/include"
        "${_zr_sp_root}/zr_vm_parser/src/zr_vm_parser"
        "${_zr_sp_root}/zr_vm_parser/src"
        "${_zr_sp_root}/tests"
        "${_zr_sp_root}/tests/core/support"
        "${_zr_sp_root}/tests/third_party/zr_unity/Unity/src")
target_compile_definitions(zr_vm_ssa_gc_map_source_preflight_test PRIVATE
        ZR_LIBRARY_TYPE_STATIC=1 _CRT_SECURE_NO_WARNINGS
        ZR_CURRENT_MODULE="zr_vm_ssa_gc_map_source_preflight_test"
        UNITY_INCLUDE_CONFIG_H UNITY_INCLUDE_DOUBLE UNITY_DOUBLE_PRECISION=1e-6
        UNITY_USE_FLUSH_STDOUT)
set_target_properties(zr_vm_ssa_gc_map_source_preflight_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
        MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
zr_ssa_execir_owner_apply_settings(zr_vm_ssa_gc_map_source_preflight_test)
# Existing provider OBJECT targets retain their actual allocator FI and settings.
# Fixture calls the real ledger explicitly; allocator NO_REDIRECT and Unity use CRT.
if (MSVC)
    target_compile_options(zr_vm_ssa_gc_map_source_preflight_test PRIVATE /UNDEBUG)
    target_link_options(zr_vm_ssa_gc_map_source_preflight_test PRIVATE
            /INCREMENTAL:NO /OPT:NOREF /OPT:NOICF /VERBOSE:LIB /STACK:8388608
            "/MAP:${CMAKE_BINARY_DIR}/ssa-gc-map-source-preflight.map")
else ()
    target_compile_options(zr_vm_ssa_gc_map_source_preflight_test PRIVATE -UNDEBUG)
    target_link_options(zr_vm_ssa_gc_map_source_preflight_test PRIVATE
            "-Wl,-Map,${CMAKE_BINARY_DIR}/ssa-gc-map-source-preflight.map")
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(zr_vm_ssa_gc_map_source_preflight_test PRIVATE
                -fsanitize=undefined -fno-sanitize-recover=all)
        target_link_options(zr_vm_ssa_gc_map_source_preflight_test PRIVATE
                -fsanitize=undefined -fno-sanitize-recover=all)
    endif ()
endif ()
foreach (_target IN LISTS _zr_sp_objects)
    target_link_libraries(zr_vm_ssa_gc_map_source_preflight_test PRIVATE $<TARGET_OBJECTS:${_target}>)
endforeach ()
if (WIN32)
    target_link_libraries(zr_vm_ssa_gc_map_source_preflight_test PRIVATE kernel32)
endif ()
add_test(NAME ssa_gc_map_source_preflight COMMAND zr_vm_ssa_gc_map_source_preflight_test)
set_tests_properties(ssa_gc_map_source_preflight PROPERTIES
        LABELS "ssa;source;metadata-validation" TIMEOUT 180
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")

# Actual configured graph and effective settings are reviewed before native build.
set(_zr_sp_graph "target\tabsolute_source\tsha256\n")
set(_zr_sp_settings "compiler\t${CMAKE_C_COMPILER}\ncompiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\ngenerator\t${CMAKE_GENERATOR}\nbuild_type\t${CMAKE_BUILD_TYPE}\nlinker\t${CMAKE_LINKER}\ntoolchain_file\t${CMAKE_TOOLCHAIN_FILE}\nmake_rules_override_c\t${CMAKE_USER_MAKE_RULES_OVERRIDE_C}\narchive_tool\t${CMAKE_AR}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\n")
foreach (_target IN ITEMS ${_zr_sp_objects} zr_vm_ssa_gc_map_source_preflight_test)
    get_target_property(_sources ${_target} SOURCES)
    foreach (_source IN LISTS _sources)
        get_filename_component(_absolute "${_source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(SHA256 "${_absolute}" _hash)
        string(APPEND _zr_sp_graph "${_target}\t${_absolute}\t${_hash}\n")
    endforeach ()
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
            LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
        get_target_property(_value ${_target} ${_property})
        if (_value STREQUAL "_value-NOTFOUND")
            set(_value "")
        endif ()
        string(APPEND _zr_sp_settings "${_target}.${_property}\t${_value}\n")
    endforeach ()
endforeach ()
file(WRITE "${CMAKE_BINARY_DIR}/gc-map-source-preflight-target-sources.tsv" "${_zr_sp_graph}")
file(WRITE "${CMAKE_BINARY_DIR}/gc-map-source-preflight-settings.tsv" "${_zr_sp_settings}")
