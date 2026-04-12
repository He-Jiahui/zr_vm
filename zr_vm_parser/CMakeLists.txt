include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_parser")

zr_declare_module(${zr_curr_module_name} ON)

set(_zr_vm_parser_backend_aot_stub "${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_parser/backend_aot/backend_aot_disabled_stub.c")
if (NOT ZR_VM_BUILD_AOT)
    foreach (_zr_vm_parser_backend_aot_target IN ITEMS zr_vm_parser_static zr_vm_parser_shared)
        if (TARGET ${_zr_vm_parser_backend_aot_target})
            get_target_property(_zr_vm_parser_sources ${_zr_vm_parser_backend_aot_target} SOURCES)
            list(FILTER _zr_vm_parser_sources EXCLUDE REGEX "[/\\\\]backend_aot[/\\\\].*\\.c$")
            set_property(TARGET ${_zr_vm_parser_backend_aot_target} PROPERTY SOURCES "${_zr_vm_parser_sources}")
            target_sources(${_zr_vm_parser_backend_aot_target} PRIVATE ${_zr_vm_parser_backend_aot_stub})
        endif ()
    endforeach ()
endif ()

set(_zr_vm_parser_private_include_dirs
        ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_parser/compiler
        ${CMAKE_CURRENT_SOURCE_DIR}/src/zr_vm_parser/backend_aot
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
