include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_library")

set(zr_curr_module_src
        src/zr_vm_library.c
)
zr_declare_module(${zr_curr_module_name} ON ${zr_curr_module_src})

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")

zr_install_module(${zr_curr_module_name})