include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_cli")

zr_declare_executable(${zr_curr_module_name} ON)

set(zr_curr_module_exec ${zr_curr_module_name}_executable)

zr_link_library_for_executable(${zr_curr_module_name} "zr_vm_library")

install(TARGETS ${zr_curr_module_exec}
        ARCHIVE DESTINATION exec
        LIBRARY DESTINATION exec)