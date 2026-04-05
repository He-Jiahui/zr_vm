include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_parser")

zr_declare_module(${zr_curr_module_name} ON)

if (TARGET zr_vm_parser_static)
    target_sources(zr_vm_parser_static PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_parser/backend_aot_exec_ir.c
    )
endif ()

if (TARGET zr_vm_parser_shared)
    target_sources(zr_vm_parser_shared PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_parser/backend_aot_exec_ir.c
    )
endif ()

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")

zr_install_module(${zr_curr_module_name})
