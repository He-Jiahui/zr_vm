# Genuine returned-source metadata with separately observed producer and owner windows.
if (TARGET zr_vm_ssa_source_returned_execir_retention_test)
    return()
endif ()
if (NOT DEFINED ZR_SSA_SOURCE_ROOT)
    if (NOT TARGET zr_vm_parser_static OR NOT TARGET zr_vm_core_static OR NOT TARGET zr_vm_library_static)
        return()
    endif ()
    set(_zr_returned_execir_root "${CMAKE_SOURCE_DIR}")
    set(_zr_returned_execir_descriptor zr_vm_ssa_source_returned_descriptor_object)
    set(_zr_returned_execir_owner zr_vm_ssa_source_returned_owner_object)
else ()
    set(_zr_returned_execir_root "${ZR_SSA_SOURCE_ROOT}")
    set(_zr_returned_execir_descriptor ssa_source_returned_descriptor_object)
    set(_zr_returned_execir_owner ssa_source_returned_owner_object)
endif ()
include("${CMAKE_CURRENT_LIST_DIR}/ssa-source-returned-retention.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ssa-function-execir-owner.cmake")

set(_zr_returned_execir_parser_sources)
foreach (_name IN ITEMS exec_ir_build exec_ir_state_maps exec_ir_dead_source_places
        exec_ir_source_frame exec_ir_source_frame_validation exec_ir_frame_layout
        exec_ir_source_finalized_module exec_ir_source_finalized_module_validation exec_ir_source_callable_query
        exec_ir_source_integer_expression exec_ir_source_integer_constants)
    if (_name MATCHES "^exec_ir_source_(frame|finalized_module|callable_query|integer_)")
        set(_source_dir exec_ir/source)
    else ()
        set(_source_dir exec_ir)
    endif ()
    list(APPEND _zr_returned_execir_parser_sources
            "${_zr_returned_execir_root}/zr_vm_parser/src/zr_vm_parser/${_source_dir}/${_name}.c")
endforeach ()
list(APPEND _zr_returned_execir_parser_sources
        "${_zr_returned_execir_root}/zr_vm_parser/src/zr_vm_parser/types/canonical_type_lookup.c")
set(_zr_returned_execir_producer
        "${_zr_returned_execir_root}/zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c")
# Preserve the full ten-source residual exclusion list; replace the two Frame
# providers with one private role rather than compiling duplicate definitions.
set(_zr_returned_execir_frame_sources
        "${_zr_returned_execir_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame.c"
        "${_zr_returned_execir_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c")
set(_zr_returned_execir_private_parser_sources ${_zr_returned_execir_parser_sources})
list(REMOVE_ITEM _zr_returned_execir_private_parser_sources ${_zr_returned_execir_frame_sources})
add_library(zr_vm_ssa_returned_execir_private_parser OBJECT
        ${_zr_returned_execir_private_parser_sources})
add_library(zr_vm_ssa_returned_execir_private_frame OBJECT ${_zr_returned_execir_frame_sources})
add_library(zr_vm_ssa_returned_execir_observed_producer OBJECT
        "${_zr_returned_execir_producer}")
set(_zr_returned_execir_core_copy
        "${_zr_returned_execir_root}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c")
add_library(zr_vm_ssa_returned_execir_private_core_copy OBJECT "${_zr_returned_execir_core_copy}")
add_executable(zr_vm_ssa_source_returned_execir_retention_test
        "${_zr_returned_execir_root}/tests/parser/test_ssa_source_returned_execir_retention.c"
        "${_zr_returned_execir_root}/tests/parser/support/ssa_source_returned_execir_fixture.c"
        "${_zr_returned_execir_root}/tests/parser/support/ssa_source_returned_crt_ledger.c"
        "${_zr_returned_execir_root}/tests/parser/support/ssa_source_returned_execir_cleanup.c"
        "${_zr_returned_execir_root}/tests/parser/support/ssa_source_returned_retention_fixture.c"
        "${_zr_returned_execir_root}/tests/core/support/ssa_exec_ir_owned_module_fixture.c"
        "${_zr_returned_execir_root}/tests/core/support/ssa_exec_ir_owner_capture.c"
        "${_zr_returned_execir_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c")
foreach (_target IN ITEMS zr_vm_ssa_returned_execir_private_parser
        zr_vm_ssa_returned_execir_private_frame
        zr_vm_ssa_returned_execir_observed_producer
        zr_vm_ssa_returned_execir_private_core_copy
        zr_vm_ssa_source_returned_execir_retention_test)
    target_include_directories(${_target} PRIVATE
            "${_zr_returned_execir_root}" "${_zr_returned_execir_root}/tests"
            "${_zr_returned_execir_root}/tests/third_party/zr_unity/Unity/src"
            "${_zr_returned_execir_root}/zr_vm_common/include"
            "${_zr_returned_execir_root}/zr_vm_core/include"
            "${_zr_returned_execir_root}/zr_vm_core/src/zr_vm_core"
            "${_zr_returned_execir_root}/zr_vm_parser/include"
            "${_zr_returned_execir_root}/zr_vm_parser/src"
            "${_zr_returned_execir_root}/zr_vm_parser/src/zr_vm_parser"
            "${_zr_returned_execir_root}/zr_vm_parser/src/zr_vm_parser/compiler"
            "${_zr_returned_execir_root}/zr_vm_parser/src/zr_vm_parser/exec_ir"
            "${_zr_returned_execir_root}/zr_vm_library/include"
            ${_zr_vendor_includes})
    target_compile_definitions(${_target} PRIVATE
            ZR_LIBRARY_TYPE_STATIC=1 _CRT_SECURE_NO_WARNINGS)
    if (DEFINED ZR_SSA_SOURCE_ROOT)
        target_compile_definitions(${_target} PRIVATE
                ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1 ZR_DEBUG=1
                ZR_VM_COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")
        zr_ssa_source_apply_c_settings(${_target})
    endif ()
endforeach ()
target_compile_definitions(zr_vm_ssa_returned_execir_private_core_copy PRIVATE ZR_CURRENT_MODULE="zr_vm_core")
foreach (_target IN ITEMS zr_vm_ssa_returned_execir_private_parser
        zr_vm_ssa_returned_execir_private_frame
        zr_vm_ssa_returned_execir_observed_producer)
    target_compile_definitions(${_target} PRIVATE ZR_CURRENT_MODULE="zr_vm_parser")
endforeach ()
target_compile_definitions(zr_vm_ssa_source_returned_execir_retention_test PRIVATE
        ZR_CURRENT_MODULE="zr_vm_ssa_source_returned_execir_retention_test"
        UNITY_INCLUDE_CONFIG_H UNITY_INCLUDE_DOUBLE UNITY_DOUBLE_PRECISION=1e-6
        UNITY_USE_FLUSH_STDOUT)
if (MSVC)
    target_compile_options(zr_vm_ssa_returned_execir_private_core_copy PRIVATE
            "/FI${_zr_returned_execir_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
    target_compile_options(zr_vm_ssa_returned_execir_private_parser PRIVATE
            "/FI${_zr_returned_execir_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
    target_compile_options(zr_vm_ssa_returned_execir_private_frame PRIVATE
            "/FI${_zr_returned_execir_root}/tests/parser/support/ssa_source_returned_execir_frame_shim.h")
    target_compile_options(zr_vm_ssa_returned_execir_observed_producer PRIVATE
            "/FI${_zr_returned_execir_root}/tests/parser/support/ssa_source_returned_execir_owner_shim.h")
    target_link_options(zr_vm_ssa_source_returned_execir_retention_test PRIVATE /STACK:8388608)
else ()
    target_compile_options(zr_vm_ssa_returned_execir_private_core_copy PRIVATE
            -include "${_zr_returned_execir_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
    target_compile_options(zr_vm_ssa_returned_execir_private_parser PRIVATE
            -include "${_zr_returned_execir_root}/tests/core/support/ssa_exec_ir_owned_module_shim.h")
    target_compile_options(zr_vm_ssa_returned_execir_private_frame PRIVATE
            -include "${_zr_returned_execir_root}/tests/parser/support/ssa_source_returned_execir_frame_shim.h")
    target_compile_options(zr_vm_ssa_returned_execir_observed_producer PRIVATE
            -include "${_zr_returned_execir_root}/tests/parser/support/ssa_source_returned_execir_owner_shim.h")
endif ()

set(_zr_returned_execir_excluded_sources ${_zr_returned_execir_parser_sources}
        "${_zr_returned_execir_producer}"
        "${_zr_returned_execir_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c")
get_target_property(_zr_returned_execir_core_excluded_sources zr_vm_ssa_execir_owner_private_core SOURCES)
list(APPEND _zr_returned_execir_core_excluded_sources "${_zr_returned_execir_core_copy}"
        "${_zr_returned_execir_root}/zr_vm_core/src/zr_vm_core/aot/aot_ir_owned_module.c")
if (DEFINED ZR_SSA_SOURCE_ROOT)
set(_zr_returned_execir_core_excluded_members)
foreach (_source IN LISTS _zr_returned_execir_core_excluded_sources)
    get_filename_component(_name "${_source}" NAME)
    list(APPEND _zr_returned_execir_core_excluded_members "${_name}.obj")
endforeach ()
set(_zr_returned_execir_core_archive
        "${CMAKE_BINARY_DIR}/lib/ssa_source_returned_execir_residual_core${CMAKE_STATIC_LIBRARY_SUFFIX}")
add_custom_command(OUTPUT "${_zr_returned_execir_core_archive}"
        "${_zr_returned_execir_core_archive}.members.tsv"
        COMMAND "${CMAKE_COMMAND}"
        "-DINPUT=$<TARGET_FILE:ssa_source_zr_vm_core>"
        "-DOUTPUT=${_zr_returned_execir_core_archive}"
        "-DARCHIVE_ROOT=${CMAKE_BINARY_DIR}"
        "-DARCHIVER=${ZR_SSA_FUNCTION_EXECIR_ARCHIVER}"
        "-DEXCLUDED_BASENAMES=${_zr_returned_execir_core_excluded_members}"
        -DEXPECTED_EXCLUDED_COUNT=17
        -P "${CMAKE_CURRENT_LIST_DIR}/ssa-function-execir-residual-archive.cmake"
        DEPENDS ssa_source_zr_vm_core "${ZR_SSA_FUNCTION_EXECIR_ARCHIVER}"
        "${CMAKE_CURRENT_LIST_DIR}/ssa-function-execir-residual-archive.cmake"
        VERBATIM)
add_custom_target(zr_vm_ssa_returned_execir_residual_core_archive
        DEPENDS "${_zr_returned_execir_core_archive}" "${_zr_returned_execir_core_archive}.members.tsv")
add_library(zr_vm_ssa_returned_execir_residual_core STATIC IMPORTED)
set_target_properties(zr_vm_ssa_returned_execir_residual_core PROPERTIES
        IMPORTED_LOCATION "${_zr_returned_execir_core_archive}")
add_dependencies(zr_vm_ssa_returned_execir_residual_core zr_vm_ssa_returned_execir_residual_core_archive)
set(_zr_returned_execir_excluded_members)
foreach (_source IN LISTS _zr_returned_execir_excluded_sources)
    get_filename_component(_name "${_source}" NAME)
    list(APPEND _zr_returned_execir_excluded_members "${_name}.obj")
endforeach ()
set(_zr_returned_execir_archive
        "${CMAKE_BINARY_DIR}/lib/ssa_source_returned_execir_residual_parser${CMAKE_STATIC_LIBRARY_SUFFIX}")
add_custom_command(OUTPUT "${_zr_returned_execir_archive}"
        "${_zr_returned_execir_archive}.members.tsv"
        COMMAND "${CMAKE_COMMAND}"
        "-DINPUT=$<TARGET_FILE:ssa_source_zr_vm_parser>"
        "-DOUTPUT=${_zr_returned_execir_archive}"
        "-DARCHIVE_ROOT=${CMAKE_BINARY_DIR}"
        "-DARCHIVER=${ZR_SSA_FUNCTION_EXECIR_ARCHIVER}"
        "-DEXCLUDED_BASENAMES=${_zr_returned_execir_excluded_members}"
        -DEXPECTED_EXCLUDED_COUNT=14
        -P "${CMAKE_CURRENT_LIST_DIR}/ssa-function-execir-residual-archive.cmake"
        DEPENDS ssa_source_zr_vm_parser "${ZR_SSA_FUNCTION_EXECIR_ARCHIVER}"
        "${CMAKE_CURRENT_LIST_DIR}/ssa-function-execir-residual-archive.cmake"
        VERBATIM)
add_custom_target(zr_vm_ssa_returned_execir_residual_parser_archive
        DEPENDS "${_zr_returned_execir_archive}" "${_zr_returned_execir_archive}.members.tsv")
add_library(zr_vm_ssa_returned_execir_residual_parser STATIC IMPORTED)
set_target_properties(zr_vm_ssa_returned_execir_residual_parser PROPERTIES
        IMPORTED_LOCATION "${_zr_returned_execir_archive}")
add_dependencies(zr_vm_ssa_returned_execir_residual_parser
        zr_vm_ssa_returned_execir_residual_parser_archive)
else ()
    # Preserve actual compilation properties, including legitimate OFF/0 values.
    # Remap both static dependency properties, including LINK_ONLY expressions.
    function(_zr_returned_execir_residual target original expected)
        get_target_property(_sources ${original} SOURCES)
        get_target_property(_source_dir ${original} SOURCE_DIR)
        set(_retained)
        set(_excluded 0)
        foreach (_source IN LISTS _sources)
            file(REAL_PATH "${_source}" _absolute BASE_DIRECTORY "${_source_dir}")
            if (_absolute IN_LIST ARGN)
                math(EXPR _excluded "${_excluded} + 1")
            else ()
                list(APPEND _retained "${_absolute}")
            endif ()
        endforeach ()
        if (NOT _excluded EQUAL expected)
            message(FATAL_ERROR "Returned ExecIR residual exclusion count mismatch: ${original}")
        endif ()
        add_library(${target} STATIC ${_retained})
        foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
                COMPILE_FEATURES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS
                MSVC_RUNTIME_LIBRARY POSITION_INDEPENDENT_CODE LINK_OPTIONS
                LINK_LIBRARIES INTERFACE_LINK_LIBRARIES INTERFACE_INCLUDE_DIRECTORIES
                INTERFACE_SYSTEM_INCLUDE_DIRECTORIES INTERFACE_COMPILE_DEFINITIONS
                INTERFACE_COMPILE_OPTIONS INTERFACE_COMPILE_FEATURES)
            get_target_property(_value ${original} ${_property})
            if (NOT _value MATCHES "-NOTFOUND$")
                if (_property STREQUAL "LINK_LIBRARIES" OR _property STREQUAL "INTERFACE_LINK_LIBRARIES")
                    string(REPLACE "zr_vm_core_static" "zr_vm_ssa_returned_execir_residual_core" _value "${_value}")
                    string(REPLACE "zr_vm_library_static" "zr_vm_ssa_returned_execir_residual_library" _value "${_value}")
                endif ()
                set_property(TARGET ${target} PROPERTY ${_property} "${_value}")
            endif ()
        endforeach ()
    endfunction()
    _zr_returned_execir_residual(zr_vm_ssa_returned_execir_residual_core zr_vm_core_static 17
            ${_zr_returned_execir_core_excluded_sources})
    _zr_returned_execir_residual(zr_vm_ssa_returned_execir_residual_library zr_vm_library_static 0)
    _zr_returned_execir_residual(zr_vm_ssa_returned_execir_residual_parser zr_vm_parser_static 14
            ${_zr_returned_execir_excluded_sources})
    target_sources(zr_vm_ssa_source_returned_execir_retention_test PRIVATE
            "${_zr_returned_execir_root}/tests/third_party/zr_unity/Unity/src/unity.c")
endif ()
target_link_libraries(zr_vm_ssa_source_returned_execir_retention_test PRIVATE
        zr_vm_ssa_execir_owner_private_core zr_vm_ssa_returned_execir_private_parser
        zr_vm_ssa_returned_execir_private_frame
        zr_vm_ssa_returned_execir_observed_producer
        zr_vm_ssa_returned_execir_private_core_copy
        ${_zr_returned_execir_descriptor} ${_zr_returned_execir_owner}
        zr_vm_ssa_returned_execir_residual_parser zr_vm_ssa_returned_execir_residual_core)
if (DEFINED ZR_SSA_SOURCE_ROOT)
    target_link_libraries(zr_vm_ssa_source_returned_execir_retention_test PRIVATE
        ssa_source_zr_vm_library ssa_source_zr_vm_common
        ${_zr_vendor_targets} ssa_source_unity kernel32)
set(ZR_SSA_SOURCE_RETURNED_EXECIR_MAP_NAME "ssa-source-returned-execir-retention-link.map"
        CACHE STRING "Returned source ExecIR retention link map basename")
if (NOT ZR_SSA_SOURCE_RETURNED_EXECIR_MAP_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9_.-]*\\.map$")
    message(FATAL_ERROR "Returned ExecIR map must be a basename in the build directory")
endif ()
target_link_options(zr_vm_ssa_source_returned_execir_retention_test PRIVATE
        /INCREMENTAL:NO /OPT:REF /VERBOSE:LIB
        "/MAP:${CMAKE_BINARY_DIR}/${ZR_SSA_SOURCE_RETURNED_EXECIR_MAP_NAME}")
else ()
    target_link_libraries(zr_vm_ssa_source_returned_execir_retention_test PRIVATE
            zr_vm_ssa_returned_execir_residual_library)
endif ()
add_test(NAME ssa_source_returned_execir_retention COMMAND ${CMAKE_COMMAND} -E env
        --unset=ZR_VM_PROFILE_OUT --unset=ZR_VM_TRACE_CORE_BOOTSTRAP
        --unset=ZR_VM_TRACE_PROJECT_STARTUP
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
        ZR_VM_PROFILE_INSTRUCTIONS=0 ZR_VM_PROFILE_SLOWPATHS=0
        ZR_VM_PROFILE_HELPERS=0 ZR_VM_PROFILE_MEMORY=0
        $<TARGET_FILE:zr_vm_ssa_source_returned_execir_retention_test>)
set_tests_properties(ssa_source_returned_execir_retention PROPERTIES
        LABELS "ssa;source-metadata;returned-ownership;allocator-fault" TIMEOUT 120)
