# Shared path contract for bounded SSA CMake validations.
get_filename_component(_ZR_SSA_VALIDATION_DEFAULT_SOURCE_ROOT
        "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH)

function(zr_ssa_validation_source_root output_variable)
    if (DEFINED ZR_SSA_SOURCE_ROOT AND NOT ZR_SSA_SOURCE_ROOT STREQUAL "")
        get_filename_component(_source_root "${ZR_SSA_SOURCE_ROOT}" REALPATH)
    else ()
        set(_source_root "${_ZR_SSA_VALIDATION_DEFAULT_SOURCE_ROOT}")
    endif ()
    if (NOT EXISTS "${_source_root}/CMakeLists.txt" OR
            NOT IS_DIRECTORY "${_source_root}/tests/cmake")
        message(FATAL_ERROR "ZR_SSA_SOURCE_ROOT must name the zr_vm checkout containing tests/cmake")
    endif ()
    set(${output_variable} "${_source_root}" PARENT_SCOPE)
endfunction()

# An explicit root is useful for managed builds (for example E:/cargo-targets).
# Without one, the caller's actual binary directory is the bounded output root.
function(zr_ssa_validation_binary_dir output_variable)
    cmake_path(ABSOLUTE_PATH CMAKE_BINARY_DIR NORMALIZE OUTPUT_VARIABLE _binary_dir)
    if (DEFINED ZR_SSA_VALIDATION_OUTPUT_ROOT AND
            NOT ZR_SSA_VALIDATION_OUTPUT_ROOT STREQUAL "")
        cmake_path(ABSOLUTE_PATH ZR_SSA_VALIDATION_OUTPUT_ROOT NORMALIZE
                OUTPUT_VARIABLE _output_root)
        cmake_path(IS_PREFIX _output_root "${_binary_dir}" NORMALIZE _inside_output_root)
        if (NOT _inside_output_root OR _binary_dir STREQUAL _output_root)
            message(FATAL_ERROR
                    "CMAKE_BINARY_DIR must be a child of ZR_SSA_VALIDATION_OUTPUT_ROOT")
        endif ()
    endif ()
    if (_binary_dir STREQUAL CMAKE_SOURCE_DIR)
        message(FATAL_ERROR "Use a separate binary directory for SSA validation")
    endif ()
    set(${output_variable} "${_binary_dir}" PARENT_SCOPE)
endfunction()
