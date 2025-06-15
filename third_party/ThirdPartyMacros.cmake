function(zr_declare_third_party module_name zr_module_src)
    set(zr_module_name ${module_name})

    if(BUILD_STATIC_LIB)
        set(zr_module_static ${zr_module_name}_static)
        add_library(${zr_module_static} STATIC ${zr_module_src})
        # DEFINE MODULE NAME
        target_compile_options(${zr_module_static} PRIVATE -DZR_CURRENT_MODULE="${zr_module_name}")
        # DEFINE LIBRARY TYPE
        target_compile_definitions(${zr_module_static} PRIVATE -DZR_LIBRARY_TYPE_STATIC)

        target_include_directories(${zr_module_static} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

        set_target_properties(${zr_module_static} PROPERTIES OUTPUT_NAME ${zr_module_name})
    endif ()

    if(BUILD_SHARED_LIB)
        set(zr_module_shared ${zr_module_name}_shared)
        add_library(${zr_module_shared} SHARED ${zr_module_src})
        # DEFINE MODULE NAME
        target_compile_options(${zr_module_shared} PRIVATE -DZR_CURRENT_MODULE="${zr_module_name}")
        # DEFINE LIBRARY TYPE
        target_compile_definitions(${zr_module_shared} PRIVATE -DZR_LIBRARY_TYPE_SHARED)

        target_include_directories(${zr_module_shared} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

        set_target_properties(${zr_module_shared} PROPERTIES OUTPUT_NAME ${zr_module_name})

    endif ()

endfunction()