include(${CMAKE_SOURCE_DIR}/third_party/ThirdPartyMacros.cmake)

# no src files
set(zr_tiny_dir_src "tinydir.c")


zr_declare_third_party(zr_tiny_dir ${zr_tiny_dir_src})

if (WIN32)
    target_compile_definitions(zr_tiny_dir_static PRIVATE UNICODE _UNICODE)
endif ()