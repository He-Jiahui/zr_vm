include(${CMAKE_SOURCE_DIR}/zr_vm_common/ThirdPartyMacros.cmake)
include(GenerateExportHeader)

set(zr_miniz_src
        ${CMAKE_CURRENT_SOURCE_DIR}/miniz/miniz.c
        ${CMAKE_CURRENT_SOURCE_DIR}/miniz/miniz_tdef.c
        ${CMAKE_CURRENT_SOURCE_DIR}/miniz/miniz_tinfl.c
        ${CMAKE_CURRENT_SOURCE_DIR}/miniz/miniz_zip.c
)

zr_register_owned_third_party(zr_vm_library zr_miniz ${zr_miniz_src})

generate_export_header(zr_third_party_zr_miniz_static
        BASE_NAME miniz
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/miniz_export.h
)

target_include_directories(zr_third_party_zr_miniz_static PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/miniz
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(zr_third_party_zr_miniz_static PRIVATE
        MINIZ_NO_ZLIB_COMPATIBLE_NAMES
)
