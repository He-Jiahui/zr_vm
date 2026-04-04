include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_cli")

zr_declare_executable(${zr_curr_module_name} ON)

set(zr_curr_module_exec ${zr_curr_module_name}_executable)

target_include_directories(${zr_curr_module_exec} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${zr_curr_module_exec} PRIVATE
        ZR_VM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)

zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_library")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_lib_ffi")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_lib_math")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_lib_system")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_lib_container")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_parser")

if (BUILD_SHARED_LIB)
    target_compile_definitions(${zr_curr_module_exec} PRIVATE
            ZR_VM_HOST_LIB_DIR="$<TARGET_LINKER_FILE_DIR:zr_vm_library_shared>"
    )
elseif (BUILD_STATIC_LIB)
    target_compile_definitions(${zr_curr_module_exec} PRIVATE
            ZR_VM_HOST_LIB_DIR="${CMAKE_BINARY_DIR}/lib"
    )
endif ()

install(TARGETS ${zr_curr_module_exec}
        ARCHIVE DESTINATION exec
        LIBRARY DESTINATION exec)
