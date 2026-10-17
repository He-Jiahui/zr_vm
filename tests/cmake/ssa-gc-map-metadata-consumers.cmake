# Actual metadata consumers, with no interpreter entry or runtime archive.
if (TARGET zr_vm_ssa_gc_map_metadata_consumers_test)
    return()
endif ()
if (NOT TARGET zr_vm_ssa_execir_owner_private_core)
    message(FATAL_ERROR "GC-map consumers require the existing private Core objects")
endif ()
set(_zr_gc_root "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}")
target_include_directories(zr_vm_ssa_execir_owner_private_core PRIVATE
        "${_zr_gc_root}/zr_vm_core/src/zr_vm_core")
add_library(zr_vm_ssa_gc_map_metadata_consumers_private OBJECT
        "${_zr_gc_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c"
        "${_zr_gc_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c"
        "${_zr_gc_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c"
        "${_zr_gc_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_phi.c")
add_executable(zr_vm_ssa_gc_map_metadata_consumers_test
        "${_zr_gc_root}/tests/parser/test_ssa_gc_map_metadata_consumers.c"
        "${_zr_gc_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_gc_root}/tests/third_party/zr_unity/Unity/src/unity.c")
foreach (_target IN ITEMS zr_vm_ssa_gc_map_metadata_consumers_private
        zr_vm_ssa_gc_map_metadata_consumers_test)
    target_include_directories(${_target} PRIVATE
            "${_zr_gc_root}"
            "${_zr_gc_root}/zr_vm_common/include"
            "${_zr_gc_root}/zr_vm_core/include"
            "${_zr_gc_root}/zr_vm_core/src/zr_vm_core"
            "${_zr_gc_root}/zr_vm_parser/include"
            "${_zr_gc_root}/zr_vm_parser/src/zr_vm_parser"
            "${_zr_gc_root}/tests"
            "${_zr_gc_root}/tests/core/support"
            "${_zr_gc_root}/tests/third_party/zr_unity/Unity/src")
    target_compile_definitions(${_target} PRIVATE
            ZR_LIBRARY_TYPE_STATIC=1 _CRT_SECURE_NO_WARNINGS)
    set_target_properties(${_target} PROPERTIES C_STANDARD 11
            C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
            MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    zr_ssa_execir_owner_apply_settings(${_target})
endforeach ()
target_compile_definitions(zr_vm_ssa_gc_map_metadata_consumers_private PRIVATE
        ZR_CURRENT_MODULE="zr_vm_gc_map_metadata_providers")
target_compile_definitions(zr_vm_ssa_gc_map_metadata_consumers_test PRIVATE
        ZR_CURRENT_MODULE="zr_vm_ssa_gc_map_metadata_consumers_test"
        UNITY_INCLUDE_CONFIG_H UNITY_USE_FLUSH_STDOUT)
if (MSVC)
    target_compile_options(zr_vm_ssa_gc_map_metadata_consumers_private PRIVATE
            "/FI${_zr_gc_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
    target_link_options(zr_vm_ssa_gc_map_metadata_consumers_test PRIVATE
            /INCREMENTAL:NO /OPT:NOREF /OPT:NOICF /VERBOSE:LIB
            "/MAP:${CMAKE_BINARY_DIR}/ssa-gc-map-metadata-consumers.map")
else ()
    target_compile_options(zr_vm_ssa_gc_map_metadata_consumers_private PRIVATE
            -include "${_zr_gc_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
    target_link_options(zr_vm_ssa_gc_map_metadata_consumers_test PRIVATE
            "-Wl,-Map,${CMAKE_BINARY_DIR}/ssa-gc-map-metadata-consumers.map")
endif ()
target_link_libraries(zr_vm_ssa_gc_map_metadata_consumers_test PRIVATE
        zr_vm_ssa_gc_map_metadata_consumers_private zr_vm_ssa_execir_owner_private_core)
add_test(NAME ssa_gc_map_metadata_consumers COMMAND zr_vm_ssa_gc_map_metadata_consumers_test)
set_tests_properties(ssa_gc_map_metadata_consumers PROPERTIES
        LABELS "ssa;core-storage;metadata-validation" TIMEOUT 120
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")

# Configure evidence records actual providers and effective target properties.
set(_zr_gc_graph "target\tabsolute_source\tsha256\n")
set(_zr_gc_settings "compiler\t${CMAKE_C_COMPILER}\ncompiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\ngenerator\t${CMAKE_GENERATOR}\nbuild_type\t${CMAKE_BUILD_TYPE}\nlinker\t${CMAKE_LINKER}\ntoolchain_file\t${CMAKE_TOOLCHAIN_FILE}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\n")
foreach (_target IN ITEMS zr_vm_ssa_gc_map_metadata_consumers_private
        zr_vm_ssa_gc_map_metadata_consumers_test zr_vm_ssa_execir_owner_private_core)
    get_target_property(_sources ${_target} SOURCES)
    foreach (_source IN LISTS _sources)
        get_filename_component(_absolute "${_source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(SHA256 "${_absolute}" _hash)
        string(APPEND _zr_gc_graph "${_target}\t${_absolute}\t${_hash}\n")
    endforeach ()
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
            LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
        get_target_property(_value ${_target} ${_property})
        if (_value STREQUAL "_value-NOTFOUND")
            set(_value "")
        endif ()
        string(APPEND _zr_gc_settings "${_target}.${_property}\t${_value}\n")
    endforeach ()
endforeach ()
file(WRITE "${CMAKE_BINARY_DIR}/gc-map-metadata-consumers-target-sources.tsv" "${_zr_gc_graph}")
file(WRITE "${CMAKE_BINARY_DIR}/gc-map-metadata-consumers-settings.tsv" "${_zr_gc_settings}")
