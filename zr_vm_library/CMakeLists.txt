include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_library")

add_subdirectory(third_party)

zr_declare_module(${zr_curr_module_name} ON)

zr_link_third_party_for_module(${zr_curr_module_name} "zr_c_json")
zr_link_third_party_for_module(${zr_curr_module_name} "zr_tiny_dir")

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)


zr_install_module(${zr_curr_module_name})
