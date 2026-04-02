include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_cli")

zr_declare_executable(${zr_curr_module_name} ON)

set(zr_curr_module_exec ${zr_curr_module_name}_executable)

target_include_directories(${zr_curr_module_exec} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_library")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_lib_ffi")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_lib_math")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_lib_system")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_lib_container")
zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_parser")

install(TARGETS ${zr_curr_module_exec}
        ARCHIVE DESTINATION exec
        LIBRARY DESTINATION exec)
