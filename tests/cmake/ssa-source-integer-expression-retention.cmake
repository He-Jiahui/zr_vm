# Independent SourceCompile expression assertions reuse the generic lifecycle
# fixture and real module providers, without the literal-only observation shim.
if (TARGET zr_vm_ssa_source_integer_expression_retention_test OR
        NOT TARGET zr_vm_ssa_source_returned_retention_test)
    return()
endif ()
if (DEFINED ZR_SSA_SOURCE_ROOT)
    set(_zr_expression_root "${ZR_SSA_SOURCE_ROOT}")
else ()
    set(_zr_expression_root "${CMAKE_SOURCE_DIR}")
endif ()

add_executable(zr_vm_ssa_source_integer_expression_retention_test
        "${_zr_expression_root}/tests/parser/test_ssa_source_integer_expression_retention.c"
        "${_zr_expression_root}/tests/parser/support/ssa_source_returned_crt_ledger.c"
        "${_zr_expression_root}/tests/parser/support/ssa_source_returned_retention_fixture.c")
if (NOT DEFINED ZR_SSA_SOURCE_ROOT)
    target_sources(zr_vm_ssa_source_integer_expression_retention_test PRIVATE
            "${_zr_expression_root}/tests/third_party/zr_unity/Unity/src/unity.c")
endif ()
foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS LINK_LIBRARIES)
    get_target_property(_value zr_vm_ssa_source_returned_retention_test ${_property})
    if (_value AND NOT _value MATCHES "-NOTFOUND$")
        if (_property STREQUAL "COMPILE_DEFINITIONS")
            list(FILTER _value EXCLUDE REGEX "^ZR_CURRENT_MODULE=")
        endif ()
        set_property(TARGET zr_vm_ssa_source_integer_expression_retention_test
                PROPERTY ${_property} "${_value}")
    endif ()
endforeach ()
target_compile_definitions(zr_vm_ssa_source_integer_expression_retention_test PRIVATE
        ZR_CURRENT_MODULE="zr_vm_ssa_source_integer_expression_retention_test")
if (MSVC)
    target_link_options(zr_vm_ssa_source_integer_expression_retention_test PRIVATE /STACK:8388608)
    if (DEFINED ZR_SSA_SOURCE_ROOT)
        target_link_options(zr_vm_ssa_source_integer_expression_retention_test PRIVATE
                /INCREMENTAL:NO /OPT:REF /VERBOSE:LIB
                "/MAP:${CMAKE_BINARY_DIR}/source-integer-expression-link.map")
    endif ()
endif ()

add_test(NAME ssa_source_integer_expression_retention COMMAND ${CMAKE_COMMAND} -E env
        --unset=ZR_VM_PROFILE_OUT --unset=ZR_VM_TRACE_CORE_BOOTSTRAP
        --unset=ZR_VM_TRACE_PROJECT_STARTUP
        ZR_VM_PROFILE_INSTRUCTIONS=0 ZR_VM_PROFILE_SLOWPATHS=0
        ZR_VM_PROFILE_HELPERS=0 ZR_VM_PROFILE_MEMORY=0
        $<TARGET_FILE:zr_vm_ssa_source_integer_expression_retention_test>)
set_tests_properties(ssa_source_integer_expression_retention PROPERTIES
        LABELS "ssa;source-metadata;returned-ownership" TIMEOUT 120)
if (DEFINED ZR_SSA_SOURCE_ROOT)
    set_property(TEST ssa_source_integer_expression_retention APPEND PROPERTY
            ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
endif ()
