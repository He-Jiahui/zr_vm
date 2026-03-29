if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src")
    file(GLOB_RECURSE zr_module_src_files
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    )
else ()
    set(zr_module_src_files)
endif ()

set(zr_vm_common_src
    ${zr_module_src_files}
    CACHE
    STRING
    "common source library"
    FORCE
)

