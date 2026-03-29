function(zr_get_third_party_target_name out_var library_name)
    set(${out_var} "zr_third_party_${library_name}_static" PARENT_SCOPE)
endfunction()

function(zr_register_owned_third_party owner_module library_name)
    zr_get_third_party_target_name(zr_third_party_target ${library_name})

    if (TARGET ${zr_third_party_target})
        return()
    endif ()

    add_library(${zr_third_party_target} STATIC ${ARGN})
    target_include_directories(${zr_third_party_target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

    set_target_properties(${zr_third_party_target} PROPERTIES
            OUTPUT_NAME ${library_name}
            POSITION_INDEPENDENT_CODE ON
            C_VISIBILITY_PRESET "hidden"
            VISIBILITY_INLINES_HIDDEN ON
    )

    set_property(TARGET ${zr_third_party_target} PROPERTY ZR_THIRD_PARTY_OWNER ${owner_module})
endfunction()

function(zr_link_third_party_for_module module_name library_name)
    zr_get_third_party_target_name(zr_third_party_target ${library_name})

    if (NOT TARGET ${zr_third_party_target})
        message(FATAL_ERROR "Third-party target ${zr_third_party_target} is not registered before linking ${module_name}.")
    endif ()

    if (BUILD_STATIC_LIB)
        target_link_libraries(${module_name}_static PRIVATE ${zr_third_party_target})
    endif ()

    if (BUILD_SHARED_LIB)
        target_link_libraries(${module_name}_shared PRIVATE ${zr_third_party_target})
    endif ()
endfunction()

function(zr_link_third_party_for_target target_name library_name)
    zr_get_third_party_target_name(zr_third_party_target ${library_name})

    if (NOT TARGET ${zr_third_party_target})
        message(FATAL_ERROR "Third-party target ${zr_third_party_target} is not registered before linking ${target_name}.")
    endif ()

    target_link_libraries(${target_name} PRIVATE ${zr_third_party_target})
endfunction()
