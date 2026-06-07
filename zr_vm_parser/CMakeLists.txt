include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_parser")

zr_declare_module(${zr_curr_module_name} ON)

set(_zr_vm_parser_private_include_dirs
        ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_parser/compiler
        ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_parser/parser
        ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_parser/type_inference
        ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_parser/writer
)

set(_zr_vm_parser_aot_backend_dir
        ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot
)
file(GLOB _zr_vm_parser_aot_backend_sources CONFIGURE_DEPENDS
        "${_zr_vm_parser_aot_backend_dir}/*.c"
)
list(REMOVE_ITEM _zr_vm_parser_aot_backend_sources
        "${_zr_vm_parser_aot_backend_dir}/backend_aot_disabled_stub.c"
)

if (TARGET zr_vm_parser_static)
    target_include_directories(zr_vm_parser_static PRIVATE ${_zr_vm_parser_private_include_dirs})
    target_include_directories(zr_vm_parser_static PRIVATE ${_zr_vm_parser_aot_backend_dir})
    target_sources(zr_vm_parser_static PRIVATE ${_zr_vm_parser_aot_backend_sources})
endif ()
if (TARGET zr_vm_parser_shared)
    target_include_directories(zr_vm_parser_shared PRIVATE ${_zr_vm_parser_private_include_dirs})
    target_include_directories(zr_vm_parser_shared PRIVATE ${_zr_vm_parser_aot_backend_dir})
    target_sources(zr_vm_parser_shared PRIVATE ${_zr_vm_parser_aot_backend_sources})
endif ()

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")

zr_install_module(${zr_curr_module_name})
