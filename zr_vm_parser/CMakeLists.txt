include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_parser")

zr_declare_module(${zr_curr_module_name} ON)

set(_zr_vm_parser_private_include_dirs
        ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_parser/compiler
        ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_parser/parser
        ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_parser/type_inference
        ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_parser/writer
)
if (TARGET zr_vm_parser_static)
    target_include_directories(zr_vm_parser_static PRIVATE ${_zr_vm_parser_private_include_dirs})
endif ()
if (TARGET zr_vm_parser_shared)
    target_include_directories(zr_vm_parser_shared PRIVATE ${_zr_vm_parser_private_include_dirs})
endif ()

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")

zr_install_module(${zr_curr_module_name})
