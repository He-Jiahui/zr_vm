foreach(required IN ITEMS zr_vm_core_static zr_vm_library_static zr_vm_parser_static
        zr_vm_lib_math_static zr_vm_lib_system_static zr_vm_lib_container_static zr_vm_lib_ffi_static)
    if(NOT TARGET ${required})
        message(STATUS "System FS resource cleanup tests require ${required}")
        return()
    endif()
endforeach()
# Compile the actual checkout sources into test-local objects. No production
# source properties, original targets, or global allocator behavior are changed.
add_library(zr_vm_fs_resource_observed OBJECT
        "${CMAKE_SOURCE_DIR}/zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/src/zr_vm_library/file.c")
target_compile_definitions(zr_vm_fs_resource_observed PRIVATE ZR_LIBRARY_TYPE_STATIC ZR_FS_CRT_REDIRECT)
target_include_directories(zr_vm_fs_resource_observed PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/src/zr_vm_library"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/third_party/zr_tiny_dir/tinydir"
        "${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_lib_system/src/zr_vm_lib_system")
set(fs_crt_header "${CMAKE_SOURCE_DIR}/tests/system/support/fs_resource_crt_observer.h")
if(MSVC)
    target_compile_options(zr_vm_fs_resource_observed PRIVATE "/FI${fs_crt_header}" /utf-8)
else()
    # The forced header includes stdlib before file.c's own feature-test macro.
    target_compile_definitions(zr_vm_fs_resource_observed PRIVATE _POSIX_C_SOURCE=200809L)
    target_compile_options(zr_vm_fs_resource_observed PRIVATE -include "${fs_crt_header}")
endif()
add_executable(zr_vm_system_fs_resource_cleanup_test
        "${CMAKE_SOURCE_DIR}/tests/system/test_system_fs_resource_cleanup.c"
        "${CMAKE_SOURCE_DIR}/tests/system/support/fs_resource_crt_observer.c"
        $<TARGET_OBJECTS:zr_vm_fs_resource_observed>)
target_include_directories(zr_vm_system_fs_resource_cleanup_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include")
target_link_libraries(zr_vm_system_fs_resource_cleanup_test PRIVATE
        zr_vm_library_static zr_vm_parser_static zr_vm_core_static zr_vm_lib_math_static
        zr_vm_lib_system_static zr_vm_lib_container_static zr_vm_lib_ffi_static)
foreach(target IN ITEMS zr_vm_fs_resource_observed zr_vm_system_fs_resource_cleanup_test)
    set_target_properties(${target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
endforeach()
if(MSVC)
    target_compile_options(zr_vm_system_fs_resource_cleanup_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_system_fs_resource_cleanup_test PRIVATE -Wall -Wextra -Werror)
endif()
set(ZR_VM_FS_RESOURCE_FIXTURE_ROOT
        "${CMAKE_CURRENT_BINARY_DIR}/fs-resource-cleanup"
        CACHE PATH "Registered unique FS resource fixture parent; root cleans by actual hash")
foreach(case IN ITEMS module_text file_text bytes_new bytes_push raw_append entries glob)
    foreach(mode IN ITEMS oom normal)
        add_test(NAME system_fs_resource_${case}_${mode}
                COMMAND zr_vm_system_fs_resource_cleanup_test ${case} ${mode}
                "${ZR_VM_FS_RESOURCE_FIXTURE_ROOT}/${case}_${mode}")
        set_tests_properties(system_fs_resource_${case}_${mode} PROPERTIES TIMEOUT 30)
    endforeach()
endforeach()
