include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_core")

add_subdirectory(third_party)

zr_declare_module(${zr_curr_module_name} ON)

if (BUILD_STATIC_LIB)
    target_include_directories(zr_vm_core_static PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
endif ()

if (BUILD_SHARED_LIB)
    target_include_directories(zr_vm_core_shared PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
endif ()

zr_link_third_party_for_module(${zr_curr_module_name} "zr_xx_hash")
zr_link_third_party_for_module(${zr_curr_module_name} "zr_utf8proc")

if (NOT WIN32)
    zr_link_internal_for_module(${zr_curr_module_name} m)
endif ()
zr_install_module(${zr_curr_module_name})
