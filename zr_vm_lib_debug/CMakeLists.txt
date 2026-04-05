include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_debug")

file(GLOB_RECURSE zr_debug_module_src CONFIGURE_DEPENDS
        RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        src/*.c
)

set(zr_vm_debug_has_network OFF)
if (TARGET zr_vm_lib_network_shared OR TARGET zr_vm_lib_network_static)
    set(zr_vm_debug_has_network ON)
endif ()

if (zr_debug_module_src AND zr_vm_debug_has_network)
    zr_declare_module(${zr_curr_module_name} ON)

    zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
    zr_link_library_for_module(${zr_curr_module_name} "zr_vm_lib_network")
    zr_link_third_party_for_module(${zr_curr_module_name} "zr_c_json")

    zr_install_module(${zr_curr_module_name})
elseif (zr_debug_module_src)
    message(STATUS "Skipping zr_vm_debug: zr_vm_lib_network target is unavailable")
else ()
    message(STATUS "Skipping zr_vm_debug: no implementation sources were found")
endif ()
