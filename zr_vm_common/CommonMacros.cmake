function(zr_declare_module module_name use_common_lib src_files)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
    set(zr_module_name ${module_name})
    set(zr_module_src ${src_files})
    if(${use_common_lib})
        include_directories(${CMAKE_SOURCE_DIR}/zr_vm_common/include)
        set(zr_module_src ${zr_vm_common_src} ${zr_module_src})
    endif()

    set(zr_module_static ${zr_module_name}_static)
    set(zr_module_shared ${zr_module_name}_shared)

    add_library(${zr_module_static} STATIC ${zr_module_src})

    add_library(${zr_module_shared} SHARED ${zr_module_src})

    set_target_properties(${zr_module_static} PROPERTIES OUTPUT_NAME ${zr_module_name})
    set_target_properties(${zr_module_shared} PROPERTIES OUTPUT_NAME ${zr_module_name})

#    install(TARGETS ${zr_module_static} ${zr_module_shared}
#            ARCHIVE DESTINATION lib
#            LIBRARY DESTINATION lib)

endfunction()

function(zr_declare_executable module_name use_common_lib src_files)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
    set(zr_module_name ${module_name})
    set(zr_module_src ${src_files})
    if(${use_common_lib})
        include_directories(${CMAKE_SOURCE_DIR}/zr_vm_common/include)
        set(zr_module_src ${zr_vm_common_src} ${zr_module_src})
    endif()

    set(zr_module_executable ${zr_module_name}_executable)

    add_executable(${zr_module_executable} ${zr_module_src})

    set_target_properties(${zr_module_executable} PROPERTIES OUTPUT_NAME ${zr_module_name})

    #    install(TARGETS ${zr_module_static} ${zr_module_shared}
    #            ARCHIVE DESTINATION lib
    #            LIBRARY DESTINATION lib)

endfunction()


function(zr_link_library_for_module module_name library_name)
    set(zr_module_name ${module_name})
    set(zr_module_static ${zr_module_name}_static)
    set(zr_module_shared ${zr_module_name}_shared)
    target_link_libraries(${zr_module_static} ${library_name}_static)
    target_link_libraries(${zr_module_shared} ${library_name}_shared)
endfunction()

function(zr_install_module module_name)
    set(zr_module_name ${module_name})
    set(zr_module_static ${zr_module_name}_static)
    set(zr_module_shared ${zr_module_name}_shared)
    install(TARGETS ${zr_module_static} ${zr_module_shared}
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib)
endfunction()