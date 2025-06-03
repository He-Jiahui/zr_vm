include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
set(zr_vm_core_name "zr_vm_core")
set(zr_vm_core_src src/zr_vm_core.c)

add_library(zr_vm_core_static STATIC ${zr_vm_core_src})

add_library(zr_vm_core_shared SHARED ${zr_vm_core_src})

set_target_properties(zr_vm_core_static PROPERTIES OUTPUT_NAME ${zr_vm_core_name})
set_target_properties(zr_vm_core_shared PROPERTIES OUTPUT_NAME ${zr_vm_core_name})

install(TARGETS zr_vm_core_static zr_vm_core_shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)

