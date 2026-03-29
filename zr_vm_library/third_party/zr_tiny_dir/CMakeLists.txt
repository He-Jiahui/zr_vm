include(${CMAKE_SOURCE_DIR}/zr_vm_common/ThirdPartyMacros.cmake)

# no src files
set(zr_tiny_dir_src "tinydir.c")


zr_register_owned_third_party(zr_vm_library zr_tiny_dir ${zr_tiny_dir_src})

if (WIN32)
    target_compile_definitions(zr_third_party_zr_tiny_dir_static PRIVATE UNICODE _UNICODE)
endif ()
