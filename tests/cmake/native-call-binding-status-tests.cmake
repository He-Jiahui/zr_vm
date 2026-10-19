foreach(required IN ITEMS zr_vm_core_static zr_vm_library_static)
    if(NOT TARGET ${required})
        message(STATUS "Native call binding status tests require ${required}")
        return()
    endif()
endforeach()
add_executable(zr_vm_native_call_binding_status_test
        "${CMAKE_SOURCE_DIR}/tests/library/test_native_call_binding_status.c")
target_include_directories(zr_vm_native_call_binding_status_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/include")
target_link_libraries(zr_vm_native_call_binding_status_test PRIVATE
        zr_vm_library_static zr_vm_core_static)
set_target_properties(zr_vm_native_call_binding_status_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_native_call_binding_status_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_native_call_binding_status_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS invalid_token invalid_slot reserved operation missing_contract no_registry_control)
    add_test(NAME native_call_binding_status_${case}
            COMMAND zr_vm_native_call_binding_status_test ${case})
    set_tests_properties(native_call_binding_status_${case} PROPERTIES TIMEOUT 20)
endforeach()
