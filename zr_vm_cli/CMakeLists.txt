include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_cli")

set(zr_curr_module_src
        src/zr_vm_cli.c
)
zr_declare_executable(${zr_curr_module_name} ON ${zr_curr_module_src})

set(zr_curr_module_exec ${zr_curr_module_name}_executable)

target_link_libraries(${zr_curr_module_exec} "zr_vm_library_static")

install(TARGETS ${zr_curr_module_exec}
        ARCHIVE DESTINATION exec
        LIBRARY DESTINATION exec)