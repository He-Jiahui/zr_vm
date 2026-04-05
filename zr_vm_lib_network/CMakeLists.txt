include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_lib_network")

zr_declare_module(${zr_curr_module_name} ON)

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")

if (WIN32)
    if (BUILD_STATIC_LIB)
        target_link_libraries(${zr_curr_module_name}_static PRIVATE ws2_32)
    endif ()

    if (BUILD_SHARED_LIB)
        target_link_libraries(${zr_curr_module_name}_shared PRIVATE ws2_32)
    endif ()
endif ()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

zr_install_module(${zr_curr_module_name})
