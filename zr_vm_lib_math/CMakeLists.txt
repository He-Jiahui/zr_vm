include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_lib_math")

zr_declare_module(${zr_curr_module_name} ON)

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")

if (NOT WIN32)
    find_library(ZR_VM_SYSTEM_MATH_LIBRARY m)
    if (ZR_VM_SYSTEM_MATH_LIBRARY)
        zr_link_internal_for_module(${zr_curr_module_name} ${ZR_VM_SYSTEM_MATH_LIBRARY})
    endif ()
endif ()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

zr_install_module(${zr_curr_module_name})
