include(${CMAKE_SOURCE_DIR}/zr_vm_common/ThirdPartyMacros.cmake)

set(zr_utf8proc_src
        ${CMAKE_CURRENT_SOURCE_DIR}/utf8proc/utf8proc.c
)

zr_register_owned_third_party(zr_vm_core zr_utf8proc ${zr_utf8proc_src})
zr_get_third_party_target_name(zr_utf8proc_target zr_utf8proc)
target_compile_definitions(${zr_utf8proc_target} PUBLIC UTF8PROC_STATIC)
