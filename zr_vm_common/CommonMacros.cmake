function(zr_declare_module module_name use_common_lib)
    set(zr_module_name ${module_name})
    file(GLOB_RECURSE zr_module_src
            RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
            src/*.c
    )
    if(${use_common_lib})
        set(zr_module_src ${zr_vm_common_src} ${zr_module_src})
    endif ()

    string(TOUPPER "${zr_module_name}_api" zr_api_name)

    if(BUILD_STATIC_LIB)
        set(zr_module_static ${zr_module_name}_static)
        add_library(${zr_module_static} STATIC ${zr_module_src})
        # DEFINE MODULE NAME
        target_compile_options(${zr_module_static} PRIVATE -DZR_CURRENT_MODULE="${zr_module_name}")
        # DEFINE LIBRARY TYPE
        target_compile_definitions(${zr_module_static} PRIVATE -DZR_LIBRARY_TYPE_STATIC)

        target_include_directories(${zr_module_static} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
        if(${use_common_lib})
            target_include_directories(${zr_module_static} PRIVATE ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
        endif()
        set_target_properties(${zr_module_static} PROPERTIES OUTPUT_NAME ${zr_module_name})
    endif ()

    if(BUILD_SHARED_LIB)
        set(zr_module_shared ${zr_module_name}_shared)
        add_library(${zr_module_shared} SHARED ${zr_module_src})
        # DEFINE MODULE NAME
        target_compile_options(${zr_module_shared} PRIVATE -DZR_CURRENT_MODULE="${zr_module_name}")
        # DEFINE LIBRARY TYPE
        target_compile_definitions(${zr_module_shared} PRIVATE -DZR_LIBRARY_TYPE_SHARED)

        target_include_directories(${zr_module_shared} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
        if(${use_common_lib})
            target_include_directories(${zr_module_shared} PRIVATE ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
        endif()
        set_target_properties(${zr_module_shared} PROPERTIES OUTPUT_NAME ${zr_module_name})

    endif ()

endfunction()

function(zr_declare_executable module_name use_common_lib)
    set(zr_module_name ${module_name})
    file(GLOB_RECURSE zr_module_src
            RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
            src/*.c
    )

    set(zr_module_executable ${zr_module_name}_executable)
    if(${use_common_lib})
        set(zr_module_src ${zr_vm_common_src} ${zr_module_src})
    endif ()

    add_executable(${zr_module_executable} ${zr_module_src})


    target_include_directories(${zr_module_executable} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    if(${use_common_lib})
        target_include_directories(${zr_module_executable} PRIVATE ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    endif()


    set_target_properties(${zr_module_executable} PROPERTIES OUTPUT_NAME ${zr_module_name})

    #    install(TARGETS ${zr_module_static} ${zr_module_shared}
    #            ARCHIVE DESTINATION lib
    #            LIBRARY DESTINATION lib)

endfunction()

function(zr_link_library_for_module module_name library_name)
    set(zr_module_name ${module_name})
    set(zr_module_shared ${zr_module_name}_shared)

    if(BUILD_STATIC_LIB)
        set(zr_module_static ${zr_module_name}_static)
        target_include_directories(${zr_module_static} PUBLIC ${CMAKE_SOURCE_DIR}/${library_name}/include)
        target_link_libraries(${zr_module_static} ${library_name}_static)
    endif ()

    if(BUILD_SHARED_LIB)
        set(zr_module_shared ${zr_module_name}_shared)
        target_include_directories(${zr_module_shared} PUBLIC ${CMAKE_SOURCE_DIR}/${library_name}/include)
        target_link_libraries(${zr_module_shared} ${library_name}_shared)
    endif ()
endfunction()

function(zr_link_library_for_executable module_name library_name)
    set(zr_module_name ${module_name})
    set(zr_module_executable ${zr_module_name}_executable)

    if(BUILD_STATIC_LIB)
        target_include_directories(${zr_module_executable} PUBLIC ${CMAKE_SOURCE_DIR}/${library_name}/include)
        target_link_libraries(${zr_module_executable} ${library_name}_static)
    elseif (BUILD_SHARED_LIB)
        target_include_directories(${zr_module_executable} PUBLIC ${CMAKE_SOURCE_DIR}/${library_name}/include)
        target_link_libraries(${zr_module_executable} ${library_name}_shared)
    endif()
endfunction()

function(zr_install_module module_name)
    set(zr_module_name ${module_name})
    set(zr_module_shared ${zr_module_name}_shared)

    if(BUILD_STATIC_LIB)
        set(zr_module_static ${zr_module_name}_static)
        target_compile_definitions(${zr_module_static} PUBLIC ${zr_module_name})
        install(TARGETS ${zr_module_static}
                ARCHIVE DESTINATION lib
                LIBRARY DESTINATION lib)
    endif()

    if(BUILD_SHARED_LIB)
        set(zr_module_shared ${zr_module_name}_shared)
        target_compile_definitions(${zr_module_shared} PUBLIC ${zr_module_name})
        install(TARGETS ${zr_module_shared}
                ARCHIVE DESTINATION lib
                LIBRARY DESTINATION lib)
    endif()
endfunction()

