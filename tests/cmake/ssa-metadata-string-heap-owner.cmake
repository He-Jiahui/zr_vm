# Exact metadata string-heap ownership gate; direct validation and normal tests
# use the actual same checkout sources without the general runtime test harness.
if (TARGET zr_vm_ssa_metadata_string_heap_owner_test)
    return()
endif ()

if (DEFINED ZR_SSA_SOURCE_ROOT)
    add_executable(zr_vm_ssa_metadata_string_heap_owner_test
            "${ZR_SSA_SOURCE_ROOT}/tests/parser/test_ssa_metadata_string_heap_owner.c"
            "${ZR_SSA_SOURCE_ROOT}/tests/parser/support/ssa_literal_script_fixture.c")
    target_include_directories(zr_vm_ssa_metadata_string_heap_owner_test PRIVATE
            ${_zr_public_includes}
            "${ZR_SSA_SOURCE_ROOT}/tests"
            "${ZR_SSA_SOURCE_ROOT}/tests/third_party/zr_unity/Unity/src"
            "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser"
            "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/compiler"
            "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir")
    target_compile_definitions(zr_vm_ssa_metadata_string_heap_owner_test PRIVATE
            ${_zr_unity_definitions} ZR_LIBRARY_TYPE_STATIC=1
            ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1 ZR_DEBUG=1 _CRT_SECURE_NO_WARNINGS
            ZR_CURRENT_MODULE="zr_vm_ssa_metadata_string_heap_owner_test"
            ZR_VM_COMPILER_VERSION="Clang-19.1.5")
    zr_ssa_source_apply_c_settings(zr_vm_ssa_metadata_string_heap_owner_test)
    target_link_libraries(zr_vm_ssa_metadata_string_heap_owner_test PRIVATE
            ssa_source_zr_vm_parser ssa_source_zr_vm_core ssa_source_zr_vm_library
            ssa_source_zr_vm_common ssa_source_xxhash ssa_source_utf8proc
            ssa_source_cjson ssa_source_miniz ssa_source_unity kernel32)
    set(ZR_SSA_METADATA_STRING_HEAP_MAP_NAME "ssa-metadata-string-heap-owner-link.map"
            CACHE STRING "Exact metadata ownership map filename")
    if (NOT ZR_SSA_METADATA_STRING_HEAP_MAP_NAME MATCHES "^[A-Za-z0-9_.-]+\\.map$")
        message(FATAL_ERROR "Metadata ownership map must be a basename")
    endif ()
    target_link_options(zr_vm_ssa_metadata_string_heap_owner_test PRIVATE
            "/MAP:${CMAKE_BINARY_DIR}/${ZR_SSA_METADATA_STRING_HEAP_MAP_NAME}"
            /INCREMENTAL:NO /OPT:REF /STACK:8388608 /VERBOSE:LIB)
else ()
    if (NOT TARGET zr_vm_parser_static OR NOT TARGET zr_vm_core_static OR NOT TARGET zr_vm_library_static)
        return()
    endif ()
    add_executable(zr_vm_ssa_metadata_string_heap_owner_test
            "${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_metadata_string_heap_owner.c"
            "${CMAKE_SOURCE_DIR}/tests/parser/support/ssa_literal_script_fixture.c"
            "${CMAKE_SOURCE_DIR}/tests/third_party/zr_unity/Unity/src/unity.c")
    target_include_directories(zr_vm_ssa_metadata_string_heap_owner_test PRIVATE
            "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/tests"
            "${CMAKE_SOURCE_DIR}/tests/third_party/zr_unity/Unity/src"
            "${CMAKE_SOURCE_DIR}/zr_vm_common/include" "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
            "${CMAKE_SOURCE_DIR}/zr_vm_parser/include" "${CMAKE_SOURCE_DIR}/zr_vm_library/include"
            "${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser"
            "${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler"
            "${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir")
    target_compile_definitions(zr_vm_ssa_metadata_string_heap_owner_test PRIVATE
            UNITY_INCLUDE_CONFIG_H UNITY_INCLUDE_DOUBLE UNITY_DOUBLE_PRECISION=1e-6
            UNITY_USE_FLUSH_STDOUT ZR_LIBRARY_TYPE_STATIC=1
            ZR_CURRENT_MODULE="zr_vm_ssa_metadata_string_heap_owner_test")
    target_link_libraries(zr_vm_ssa_metadata_string_heap_owner_test PRIVATE
            zr_vm_parser_static zr_vm_core_static zr_vm_library_static)
    if (MSVC)
        target_link_options(zr_vm_ssa_metadata_string_heap_owner_test PRIVATE /STACK:8388608)
    endif ()
endif ()

add_test(NAME ssa_metadata_string_heap_owner COMMAND ${CMAKE_COMMAND} -E env
        --unset=ZR_VM_PROFILE_OUT --unset=ZR_VM_TRACE_CORE_BOOTSTRAP
        ZR_VM_PROFILE_INSTRUCTIONS=0 ZR_VM_PROFILE_SLOWPATHS=0
        ZR_VM_PROFILE_HELPERS=0 ZR_VM_PROFILE_MEMORY=0
        $<TARGET_FILE:zr_vm_ssa_metadata_string_heap_owner_test>)
set_tests_properties(ssa_metadata_string_heap_owner PROPERTIES
        LABELS "ssa;source-metadata;ownership;allocation-failure" TIMEOUT 120)
if (DEFINED ZR_SSA_SOURCE_ROOT)
    set_property(TEST ssa_metadata_string_heap_owner APPEND PROPERTY
            ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
endif ()
