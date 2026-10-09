# Actual descriptor translation unit, with only its calloc/free fault boundary.
if (TARGET zr_vm_ssa_source_descriptor_oom_test)
    return()
endif ()
if (DEFINED ZR_SSA_SOURCE_ROOT)
    add_library(ssa_source_descriptor_oom_object OBJECT
            "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c")
    target_include_directories(ssa_source_descriptor_oom_object PRIVATE
            ${_zr_public_includes} ${_zr_vendor_includes}
            "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser")
    target_compile_definitions(ssa_source_descriptor_oom_object PRIVATE
            ZR_CURRENT_MODULE="zr_vm_parser" ZR_LIBRARY_TYPE_STATIC=1
            ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1 ZR_DEBUG=1 _CRT_SECURE_NO_WARNINGS
            ZR_VM_COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")
    zr_ssa_source_apply_c_settings(ssa_source_descriptor_oom_object)
    target_compile_options(ssa_source_descriptor_oom_object PRIVATE
            "/FI${ZR_SSA_SOURCE_ROOT}/tests/parser/support/ssa_source_descriptor_oom_shim.h")
    add_executable(zr_vm_ssa_source_descriptor_oom_test
            "${ZR_SSA_SOURCE_ROOT}/tests/parser/test_ssa_source_descriptor_oom.c"
            "${ZR_SSA_SOURCE_ROOT}/tests/parser/support/ssa_literal_script_fixture.c")
    target_include_directories(zr_vm_ssa_source_descriptor_oom_test PRIVATE
            ${_zr_public_includes}
            "${ZR_SSA_SOURCE_ROOT}/tests"
            "${ZR_SSA_SOURCE_ROOT}/tests/third_party/zr_unity/Unity/src"
            "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser"
            "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/compiler"
            "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir")
    target_compile_definitions(zr_vm_ssa_source_descriptor_oom_test PRIVATE
            ${_zr_unity_definitions} ZR_LIBRARY_TYPE_STATIC=1
            ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1 ZR_DEBUG=1 _CRT_SECURE_NO_WARNINGS
            ZR_CURRENT_MODULE="zr_vm_ssa_source_descriptor_oom_test"
            ZR_VM_COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")
    zr_ssa_source_apply_c_settings(zr_vm_ssa_source_descriptor_oom_test)
    target_link_libraries(zr_vm_ssa_source_descriptor_oom_test PRIVATE
            ssa_source_descriptor_oom_object
            ssa_source_zr_vm_parser ssa_source_zr_vm_core ssa_source_zr_vm_library
            ssa_source_zr_vm_common ${_zr_vendor_targets} ssa_source_unity kernel32)
    set(ZR_SSA_SOURCE_DESCRIPTOR_OOM_MAP_NAME "ssa-source-descriptor-oom-link.map"
            CACHE STRING "Descriptor OOM link map basename")
    if (NOT ZR_SSA_SOURCE_DESCRIPTOR_OOM_MAP_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9_.-]*\\.map$")
        message(FATAL_ERROR "Descriptor map must be a basename inside the build directory")
    endif ()
    target_link_options(zr_vm_ssa_source_descriptor_oom_test PRIVATE
            /INCREMENTAL:NO /OPT:REF /VERBOSE:LIB /STACK:8388608
            "/MAP:${CMAKE_BINARY_DIR}/${ZR_SSA_SOURCE_DESCRIPTOR_OOM_MAP_NAME}")
else ()
    if (NOT TARGET zr_vm_parser_static OR NOT TARGET zr_vm_core_static OR NOT TARGET zr_vm_library_static)
        return()
    endif ()
    add_library(zr_vm_ssa_source_descriptor_oom_object OBJECT
            "${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c")
    target_include_directories(zr_vm_ssa_source_descriptor_oom_object PRIVATE
            "${CMAKE_SOURCE_DIR}/zr_vm_common/include" "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
            "${CMAKE_SOURCE_DIR}/zr_vm_parser/include" "${CMAKE_SOURCE_DIR}/zr_vm_library/include"
            "${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser")
    target_compile_definitions(zr_vm_ssa_source_descriptor_oom_object PRIVATE
            ZR_CURRENT_MODULE="zr_vm_parser" ZR_LIBRARY_TYPE_STATIC=1)
    if (MSVC)
        target_compile_options(zr_vm_ssa_source_descriptor_oom_object PRIVATE
                "/FI${CMAKE_SOURCE_DIR}/tests/parser/support/ssa_source_descriptor_oom_shim.h")
    else ()
        target_compile_options(zr_vm_ssa_source_descriptor_oom_object PRIVATE
                -include "${CMAKE_SOURCE_DIR}/tests/parser/support/ssa_source_descriptor_oom_shim.h")
    endif ()
    add_executable(zr_vm_ssa_source_descriptor_oom_test
            "${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_source_descriptor_oom.c"
            "${CMAKE_SOURCE_DIR}/tests/parser/support/ssa_literal_script_fixture.c"
            "${CMAKE_SOURCE_DIR}/tests/third_party/zr_unity/Unity/src/unity.c")
    target_include_directories(zr_vm_ssa_source_descriptor_oom_test PRIVATE
            "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/tests"
            "${CMAKE_SOURCE_DIR}/tests/third_party/zr_unity/Unity/src"
            "${CMAKE_SOURCE_DIR}/zr_vm_common/include" "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
            "${CMAKE_SOURCE_DIR}/zr_vm_parser/include" "${CMAKE_SOURCE_DIR}/zr_vm_library/include"
            "${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser"
            "${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler"
            "${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir")
    target_compile_definitions(zr_vm_ssa_source_descriptor_oom_test PRIVATE
            UNITY_INCLUDE_CONFIG_H UNITY_INCLUDE_DOUBLE UNITY_DOUBLE_PRECISION=1e-6
            UNITY_USE_FLUSH_STDOUT ZR_LIBRARY_TYPE_STATIC=1
            ZR_CURRENT_MODULE="zr_vm_ssa_source_descriptor_oom_test")
    target_link_libraries(zr_vm_ssa_source_descriptor_oom_test PRIVATE
            zr_vm_ssa_source_descriptor_oom_object
            zr_vm_parser_static zr_vm_core_static zr_vm_library_static)
    if (MSVC)
        target_link_options(zr_vm_ssa_source_descriptor_oom_test PRIVATE /STACK:8388608)
    endif ()
endif ()
add_test(NAME ssa_source_descriptor_oom COMMAND ${CMAKE_COMMAND} -E env
        --unset=ZR_VM_PROFILE_OUT --unset=ZR_VM_TRACE_CORE_BOOTSTRAP
        ZR_VM_PROFILE_INSTRUCTIONS=0 ZR_VM_PROFILE_SLOWPATHS=0
        ZR_VM_PROFILE_HELPERS=0 ZR_VM_PROFILE_MEMORY=0
        $<TARGET_FILE:zr_vm_ssa_source_descriptor_oom_test>)
set_tests_properties(ssa_source_descriptor_oom PROPERTIES
        LABELS "ssa;source-metadata;allocation-failure" TIMEOUT 120)
if (DEFINED ZR_SSA_SOURCE_ROOT)
    set_property(TEST ssa_source_descriptor_oom APPEND PROPERTY
            ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
endif ()
