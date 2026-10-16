# Keep these independent tests on the settings of the real Source providers.
include_guard(GLOBAL)

function(zr_ssa_integer_copy_settings target source module)
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
            COMPILE_FEATURES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS
            MSVC_RUNTIME_LIBRARY POSITION_INDEPENDENT_CODE)
        get_target_property(_value ${source} ${_property})
        if (NOT _value MATCHES "-NOTFOUND$")
            if (_property STREQUAL "COMPILE_DEFINITIONS")
                list(FILTER _value EXCLUDE REGEX "^ZR_CURRENT_MODULE=")
            endif ()
            set_property(TARGET ${target} PROPERTY ${_property} "${_value}")
        endif ()
    endforeach ()
    target_compile_definitions(${target} PRIVATE ZR_CURRENT_MODULE="${module}")
endfunction()

function(zr_ssa_integer_link_observed_source target parser producer)
    target_link_libraries(${target} PRIVATE
            zr_vm_ssa_execir_owner_private_core ${parser}
            zr_vm_ssa_returned_execir_private_frame ${producer}
            zr_vm_ssa_returned_execir_private_core_copy
            ${_zr_returned_execir_descriptor} ${_zr_returned_execir_owner}
            zr_vm_ssa_returned_execir_residual_parser zr_vm_ssa_returned_execir_residual_core)
    if (DEFINED ZR_SSA_SOURCE_ROOT)
        target_link_libraries(${target} PRIVATE ssa_source_zr_vm_library ssa_source_zr_vm_common
                ${_zr_vendor_targets} ssa_source_unity kernel32)
    else ()
        target_link_libraries(${target} PRIVATE zr_vm_ssa_returned_execir_residual_library)
        target_sources(${target} PRIVATE
                "${_zr_returned_execir_root}/tests/third_party/zr_unity/Unity/src/unity.c")
    endif ()
endfunction()

function(zr_ssa_integer_register_observed target test_name map_name)
    if (MSVC)
        target_link_options(${target} PRIVATE /STACK:8388608)
        if (DEFINED ZR_SSA_SOURCE_ROOT)
            target_link_options(${target} PRIVATE /INCREMENTAL:NO /OPT:REF /VERBOSE:LIB
                    "/MAP:${CMAKE_BINARY_DIR}/${map_name}")
        endif ()
    endif ()
    add_test(NAME ${test_name} COMMAND ${CMAKE_COMMAND} -E env
            --unset=ZR_VM_PROFILE_OUT --unset=ZR_VM_TRACE_CORE_BOOTSTRAP
            --unset=ZR_VM_TRACE_PROJECT_STARTUP
            UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
            ZR_VM_PROFILE_INSTRUCTIONS=0 ZR_VM_PROFILE_SLOWPATHS=0
            ZR_VM_PROFILE_HELPERS=0 ZR_VM_PROFILE_MEMORY=0
            $<TARGET_FILE:${target}>)
    set_tests_properties(${test_name} PROPERTIES
            LABELS "ssa;source-metadata;returned-ownership;allocator-fault" TIMEOUT 180)
endfunction()
