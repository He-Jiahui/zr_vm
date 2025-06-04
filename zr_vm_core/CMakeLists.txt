include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_core")
set(zr_curr_module_src
        src/zr_vm_core.c
)

zr_declare_module(${zr_curr_module_name} ON ${zr_curr_module_src})


zr_install_module(${zr_curr_module_name})
