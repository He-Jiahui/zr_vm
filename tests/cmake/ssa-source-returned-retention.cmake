# Real source compilation returns metadata owned independently of compiler scratch.
if (TARGET zr_vm_ssa_source_returned_retention_test)
    return()
endif ()
if (DEFINED ZR_SSA_SOURCE_ROOT)
    set(_zr_returned_root "${ZR_SSA_SOURCE_ROOT}")
    set(_zr_returned_descriptor ssa_source_returned_descriptor_object)
    set(_zr_returned_owner ssa_source_returned_owner_object)
else ()
    if (NOT TARGET zr_vm_parser_static OR NOT TARGET zr_vm_core_static OR NOT TARGET zr_vm_library_static)
        return()
    endif ()
    set(_zr_returned_root "${CMAKE_SOURCE_DIR}")
    set(_zr_returned_descriptor zr_vm_ssa_source_returned_descriptor_object)
    set(_zr_returned_owner zr_vm_ssa_source_returned_owner_object)
endif ()
add_library(${_zr_returned_descriptor} OBJECT
        "${_zr_returned_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c")
add_library(${_zr_returned_owner} OBJECT
        "${_zr_returned_root}/zr_vm_core/src/zr_vm_core/aot/aot_ir_owned_module.c")
add_executable(zr_vm_ssa_source_returned_retention_test
        "${_zr_returned_root}/tests/parser/test_ssa_source_returned_retention.c"
        "${_zr_returned_root}/tests/parser/support/ssa_source_returned_crt_ledger.c"
        "${_zr_returned_root}/tests/parser/support/ssa_source_returned_retention_fixture.c")
if (NOT DEFINED ZR_SSA_SOURCE_ROOT)
    target_sources(zr_vm_ssa_source_returned_retention_test PRIVATE
            "${_zr_returned_root}/tests/third_party/zr_unity/Unity/src/unity.c")
endif ()
foreach (_zr_returned_target IN ITEMS ${_zr_returned_descriptor} ${_zr_returned_owner}
        zr_vm_ssa_source_returned_retention_test)
    target_include_directories(${_zr_returned_target} PRIVATE
            "${_zr_returned_root}" "${_zr_returned_root}/tests"
            "${_zr_returned_root}/tests/third_party/zr_unity/Unity/src"
            "${_zr_returned_root}/zr_vm_common/include"
            "${_zr_returned_root}/zr_vm_core/include"
            "${_zr_returned_root}/zr_vm_core/src/zr_vm_core"
            "${_zr_returned_root}/zr_vm_parser/include"
            "${_zr_returned_root}/zr_vm_parser/src/zr_vm_parser"
            "${_zr_returned_root}/zr_vm_library/include")
    target_compile_definitions(${_zr_returned_target} PRIVATE ZR_LIBRARY_TYPE_STATIC=1)
    if (DEFINED ZR_SSA_SOURCE_ROOT)
        target_include_directories(${_zr_returned_target} PRIVATE ${_zr_vendor_includes})
        target_compile_definitions(${_zr_returned_target} PRIVATE
                ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1 ZR_DEBUG=1 _CRT_SECURE_NO_WARNINGS
                ZR_VM_COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")
        zr_ssa_source_apply_c_settings(${_zr_returned_target})
    endif ()
endforeach ()
target_compile_definitions(${_zr_returned_descriptor} PRIVATE ZR_CURRENT_MODULE="zr_vm_parser")
target_compile_definitions(${_zr_returned_owner} PRIVATE ZR_CURRENT_MODULE="zr_vm_core")
target_compile_definitions(zr_vm_ssa_source_returned_retention_test PRIVATE
        ZR_CURRENT_MODULE="zr_vm_ssa_source_returned_retention_test"
        UNITY_INCLUDE_CONFIG_H UNITY_INCLUDE_DOUBLE UNITY_DOUBLE_PRECISION=1e-6 UNITY_USE_FLUSH_STDOUT)
if (MSVC)
    target_compile_options(${_zr_returned_descriptor} PRIVATE
            "/FI${_zr_returned_root}/tests/parser/support/ssa_source_returned_descriptor_shim.h")
    target_compile_options(${_zr_returned_owner} PRIVATE
            "/FI${_zr_returned_root}/tests/parser/support/ssa_source_returned_owner_shim.h")
    target_link_options(zr_vm_ssa_source_returned_retention_test PRIVATE /STACK:8388608)
else ()
    target_compile_options(${_zr_returned_descriptor} PRIVATE
            -include "${_zr_returned_root}/tests/parser/support/ssa_source_returned_descriptor_shim.h")
    target_compile_options(${_zr_returned_owner} PRIVATE
            -include "${_zr_returned_root}/tests/parser/support/ssa_source_returned_owner_shim.h")
endif ()
if (DEFINED ZR_SSA_SOURCE_ROOT)
    target_link_libraries(zr_vm_ssa_source_returned_retention_test PRIVATE
            ${_zr_returned_descriptor} ${_zr_returned_owner}
            ssa_source_zr_vm_parser ssa_source_zr_vm_core ssa_source_zr_vm_library
            ssa_source_zr_vm_common ${_zr_vendor_targets} ssa_source_unity kernel32)
    set(ZR_SSA_SOURCE_RETURNED_RETENTION_MAP_NAME "ssa-source-returned-retention-link.map"
            CACHE STRING "Returned source retention link map basename")
    if (NOT ZR_SSA_SOURCE_RETURNED_RETENTION_MAP_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9_.-]*\\.map$")
        message(FATAL_ERROR "Returned retention map must be a basename in the build directory")
    endif ()
    target_link_options(zr_vm_ssa_source_returned_retention_test PRIVATE
            /INCREMENTAL:NO /OPT:REF /VERBOSE:LIB
            "/MAP:${CMAKE_BINARY_DIR}/${ZR_SSA_SOURCE_RETURNED_RETENTION_MAP_NAME}")
else ()
    target_link_libraries(zr_vm_ssa_source_returned_retention_test PRIVATE
            ${_zr_returned_descriptor} ${_zr_returned_owner}
            zr_vm_parser_static zr_vm_core_static zr_vm_library_static)
endif ()
add_test(NAME ssa_source_returned_retention COMMAND ${CMAKE_COMMAND} -E env
        --unset=ZR_VM_PROFILE_OUT --unset=ZR_VM_TRACE_CORE_BOOTSTRAP
        --unset=ZR_VM_TRACE_PROJECT_STARTUP
        ZR_VM_PROFILE_INSTRUCTIONS=0 ZR_VM_PROFILE_SLOWPATHS=0
        ZR_VM_PROFILE_HELPERS=0 ZR_VM_PROFILE_MEMORY=0
        $<TARGET_FILE:zr_vm_ssa_source_returned_retention_test>)
set_tests_properties(ssa_source_returned_retention PROPERTIES
        LABELS "ssa;source-metadata;returned-ownership" TIMEOUT 120)
if (DEFINED ZR_SSA_SOURCE_ROOT)
    set_property(TEST ssa_source_returned_retention APPEND PROPERTY
            ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
endif ()
