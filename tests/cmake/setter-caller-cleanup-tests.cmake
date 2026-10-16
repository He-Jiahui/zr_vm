# Real FFI aggregate conversion, with the same private layout as its archive.
foreach(required IN ITEMS zr_vm_core_static zr_vm_library_static zr_vm_parser_static
        zr_vm_lib_ffi_static zr_vm_lib_math_static zr_vm_lib_system_static zr_vm_lib_container_static)
    if(NOT TARGET ${required})
        message(STATUS "Setter caller cleanup tests require ${required}")
        return()
    endif()
endforeach()
add_executable(zr_vm_setter_caller_cleanup_test
        "${CMAKE_SOURCE_DIR}/tests/ffi/test_setter_caller_cleanup.c")
target_include_directories(zr_vm_setter_caller_cleanup_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/src/zr_vm_lib_ffi"
        "$<TARGET_PROPERTY:zr_vm_lib_ffi_static,INCLUDE_DIRECTORIES>")
# ZR_VM_HAS_LIBFFI is PRIVATE on the implementation target, but changes
# ZrFfiTypeLayout. Never compile this fixture against the header's default 0.
get_target_property(zr_cleanup_ffi_definitions zr_vm_lib_ffi_static COMPILE_DEFINITIONS)
set(zr_cleanup_ffi_abi_found FALSE)
foreach(definition IN LISTS zr_cleanup_ffi_definitions)
    if(definition MATCHES "^ZR_VM_HAS_LIBFFI=[01]$")
        target_compile_definitions(zr_vm_setter_caller_cleanup_test PRIVATE "${definition}")
        set(zr_cleanup_ffi_abi_found TRUE)
    endif()
endforeach()
if(NOT zr_cleanup_ffi_abi_found)
    message(FATAL_ERROR "Cannot establish the actual FFI private layout for cleanup regression")
endif()
# The bundled backend exports generated ffi.h and FFI_STATIC_BUILD. A system
# backend's include directory is already copied from the FFI target above.
if(TARGET zr_vm_libffi_vendor)
    target_link_libraries(zr_vm_setter_caller_cleanup_test PRIVATE zr_vm_libffi_vendor)
endif()
target_link_libraries(zr_vm_setter_caller_cleanup_test PRIVATE
        zr_vm_parser_static zr_vm_core_static zr_vm_library_static
        zr_vm_lib_math_static zr_vm_lib_system_static zr_vm_lib_container_static zr_vm_lib_ffi_static)
set_target_properties(zr_vm_setter_caller_cleanup_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_setter_caller_cleanup_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_setter_caller_cleanup_test PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME ffi_setter_caller_cleanup_field_pair_oom
        COMMAND zr_vm_setter_caller_cleanup_test ffi_field_pair_oom)
set_tests_properties(ffi_setter_caller_cleanup_field_pair_oom PROPERTIES TIMEOUT 30)
unset(zr_cleanup_ffi_definitions)
unset(zr_cleanup_ffi_abi_found)
