# Compile the actual borrowed lookup without mutable index or VM providers.
if (TARGET zr_vm_ssa_canonical_type_lookup_test)
    return()
endif ()
if (DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT)
    set(_zr_lookup_root "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}")
else ()
    set(_zr_lookup_root "${CMAKE_SOURCE_DIR}")
endif ()
add_library(zr_vm_ssa_canonical_lookup_private OBJECT
        "${_zr_lookup_root}/zr_vm_parser/src/zr_vm_parser/canonical_type_lookup.c")
add_executable(zr_vm_ssa_canonical_type_lookup_test
        "${_zr_lookup_root}/tests/parser/test_ssa_canonical_type_lookup.c"
        "${_zr_lookup_root}/tests/third_party/zr_unity/Unity/src/unity.c")
foreach (_target IN ITEMS zr_vm_ssa_canonical_lookup_private
        zr_vm_ssa_canonical_type_lookup_test)
    target_include_directories(${_target} PRIVATE
            "${_zr_lookup_root}/zr_vm_common/include"
            "${_zr_lookup_root}/zr_vm_core/include"
            "${_zr_lookup_root}/zr_vm_parser/include"
            "${_zr_lookup_root}/tests"
            "${_zr_lookup_root}/tests/third_party/zr_unity/Unity/src")
    target_compile_definitions(${_target} PRIVATE
            ZR_LIBRARY_TYPE_STATIC=1 _CRT_SECURE_NO_WARNINGS)
    set_target_properties(${_target} PROPERTIES C_STANDARD 11
            C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
            MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    if (DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT)
        zr_ssa_execir_owner_apply_settings(${_target})
    endif ()
endforeach ()
target_compile_definitions(zr_vm_ssa_canonical_lookup_private PRIVATE
        ZR_CURRENT_MODULE="zr_vm_parser")
target_compile_definitions(zr_vm_ssa_canonical_type_lookup_test PRIVATE
        ZR_CURRENT_MODULE="zr_vm_ssa_canonical_type_lookup_test"
        UNITY_INCLUDE_CONFIG_H UNITY_USE_FLUSH_STDOUT)
target_link_libraries(zr_vm_ssa_canonical_type_lookup_test PRIVATE
        zr_vm_ssa_canonical_lookup_private)
add_test(NAME ssa_canonical_type_lookup COMMAND zr_vm_ssa_canonical_type_lookup_test)
set_tests_properties(ssa_canonical_type_lookup PROPERTIES
        LABELS "ssa;canonical;borrowed-lookup" TIMEOUT 120)
if (DEFINED ZR_SSA_EXECIR_OWNER_SOURCE_ROOT)
    set(ZR_SSA_CANONICAL_LOOKUP_MAP_NAME "ssa-canonical-type-lookup-link.map"
            CACHE STRING "Finite lookup link map basename")
    if (NOT ZR_SSA_CANONICAL_LOOKUP_MAP_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9_.-]*\\.map$")
        message(FATAL_ERROR "Lookup map must be a basename in the build directory")
    endif ()
    target_link_options(zr_vm_ssa_canonical_type_lookup_test PRIVATE
            /INCREMENTAL:NO /OPT:REF /VERBOSE:LIB
            "/MAP:${CMAKE_BINARY_DIR}/${ZR_SSA_CANONICAL_LOOKUP_MAP_NAME}")
    set(_lookup_graph "target\tabsolute_source\tsha256\n")
    set(_lookup_settings "compiler\t${CMAKE_C_COMPILER}\ncompiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\ngenerator\t${CMAKE_GENERATOR}\nbuild_type\t${CMAKE_BUILD_TYPE}\nlinker\t${CMAKE_LINKER}\ntoolchain_file\t${CMAKE_TOOLCHAIN_FILE}\nmake_rules_override_c\t${CMAKE_USER_MAKE_RULES_OVERRIDE_C}\narchive_tool\t${CMAKE_AR}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\n")
    foreach (_target IN ITEMS zr_vm_ssa_canonical_lookup_private
            zr_vm_ssa_canonical_type_lookup_test)
        get_target_property(_sources ${_target} SOURCES)
        foreach (_source IN LISTS _sources)
            file(SHA256 "${_source}" _hash)
            string(APPEND _lookup_graph "${_target}\t${_source}\t${_hash}\n")
        endforeach ()
        foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS
                COMPILE_OPTIONS LINK_OPTIONS LINK_LIBRARIES C_STANDARD
                C_STANDARD_REQUIRED C_EXTENSIONS MSVC_RUNTIME_LIBRARY)
            get_target_property(_value ${_target} ${_property})
            if (_value STREQUAL "_value-NOTFOUND")
                set(_value "")
            endif ()
            string(APPEND _lookup_settings "${_target}.${_property}\t${_value}\n")
        endforeach ()
    endforeach ()
    file(WRITE "${CMAKE_BINARY_DIR}/canonical-lookup-target-sources.tsv" "${_lookup_graph}")
    file(WRITE "${CMAKE_BINARY_DIR}/canonical-lookup-settings.tsv" "${_lookup_settings}")
endif ()
