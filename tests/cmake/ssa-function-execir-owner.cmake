# Real Core lifecycle with separate ExecIR storage and AOT arena observations.
if (TARGET zr_vm_ssa_function_exec_ir_owner_test)
    return()
endif ()
if (DEFINED ZR_SSA_SOURCE_ROOT)
    set(_zr_function_execir_root "${ZR_SSA_SOURCE_ROOT}")
    set(ZR_SSA_EXECIR_OWNER_SOURCE_ROOT "${ZR_SSA_SOURCE_ROOT}")
    function(zr_ssa_execir_owner_apply_settings target_name)
        target_compile_definitions(${target_name} PRIVATE
                ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1 ZR_DEBUG=1
                ZR_VM_COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")
        zr_ssa_source_apply_c_settings(${target_name})
    endfunction()
else ()
    if (NOT TARGET zr_vm_core_static)
        return()
    endif ()
    set(_zr_function_execir_root "${CMAKE_SOURCE_DIR}")
endif ()
include("${CMAKE_CURRENT_LIST_DIR}/ssa-execir-owned-module.cmake")

# The private allocator observers must be the only providers of these TUs.
get_target_property(_zr_function_execir_excluded_sources
        zr_vm_ssa_execir_owner_private_core SOURCES)
list(APPEND _zr_function_execir_excluded_sources
        "${_zr_function_execir_root}/zr_vm_core/src/zr_vm_core/aot/aot_ir_owned_module.c")
if (DEFINED ZR_SSA_SOURCE_ROOT)
    # Reuse the current ordinary objects without recompiling the whole Core.
    # Only this test links the derived binary archive; other tests keep Core.
    get_filename_component(_zr_function_execir_compiler_dir "${CMAKE_C_COMPILER}" DIRECTORY)
    find_program(ZR_SSA_FUNCTION_EXECIR_ARCHIVER NAMES llvm-ar
            HINTS "${_zr_function_execir_compiler_dir}" REQUIRED)
    set(_zr_function_execir_excluded_members)
    foreach (_source IN LISTS _zr_function_execir_excluded_sources)
        get_filename_component(_name "${_source}" NAME)
        list(APPEND _zr_function_execir_excluded_members "${_name}.obj")
    endforeach ()
    set(_zr_function_execir_archive
            "${CMAKE_BINARY_DIR}/lib/ssa_function_execir_residual_core${CMAKE_STATIC_LIBRARY_SUFFIX}")
    add_custom_command(OUTPUT "${_zr_function_execir_archive}"
            "${_zr_function_execir_archive}.members.tsv"
            COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=$<TARGET_FILE:ssa_source_zr_vm_core>"
            "-DOUTPUT=${_zr_function_execir_archive}"
            "-DARCHIVE_ROOT=${CMAKE_BINARY_DIR}"
            "-DARCHIVER=${ZR_SSA_FUNCTION_EXECIR_ARCHIVER}"
            "-DEXCLUDED_BASENAMES=${_zr_function_execir_excluded_members}"
            -P "${CMAKE_CURRENT_LIST_DIR}/ssa-function-execir-residual-archive.cmake"
            DEPENDS ssa_source_zr_vm_core "${ZR_SSA_FUNCTION_EXECIR_ARCHIVER}"
            "${CMAKE_CURRENT_LIST_DIR}/ssa-function-execir-residual-archive.cmake"
            VERBATIM)
    add_custom_target(zr_vm_ssa_function_execir_residual_archive
            DEPENDS "${_zr_function_execir_archive}"
            "${_zr_function_execir_archive}.members.tsv")
    add_library(zr_vm_ssa_function_execir_residual_core STATIC IMPORTED)
    set_target_properties(zr_vm_ssa_function_execir_residual_core PROPERTIES
            IMPORTED_LOCATION "${_zr_function_execir_archive}")
    add_dependencies(zr_vm_ssa_function_execir_residual_core
            zr_vm_ssa_function_execir_residual_archive)
else ()
    # Native builds need no LLVM archive editor. Compile the actual Core
    # sources except the private providers, retaining embedded common sources.
    get_target_property(_zr_function_execir_sources zr_vm_core_static SOURCES)
    get_target_property(_zr_function_execir_source_dir zr_vm_core_static SOURCE_DIR)
    set(_zr_function_execir_residual_sources)
    set(_zr_function_execir_excluded_count 0)
    foreach (_source IN LISTS _zr_function_execir_sources)
        file(REAL_PATH "${_source}" _absolute BASE_DIRECTORY "${_zr_function_execir_source_dir}")
        if (_absolute IN_LIST _zr_function_execir_excluded_sources)
            math(EXPR _zr_function_execir_excluded_count "${_zr_function_execir_excluded_count} + 1")
        else ()
            list(APPEND _zr_function_execir_residual_sources "${_absolute}")
        endif ()
    endforeach ()
    if (NOT _zr_function_execir_excluded_count EQUAL 16)
        message(FATAL_ERROR "Function ExecIR residual Core must exclude exactly 16 providers")
    endif ()
    add_library(zr_vm_ssa_function_execir_residual_core STATIC
            ${_zr_function_execir_residual_sources})
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
            COMPILE_FEATURES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS
            MSVC_RUNTIME_LIBRARY POSITION_INDEPENDENT_CODE LINK_OPTIONS
            LINK_LIBRARIES INTERFACE_LINK_LIBRARIES INTERFACE_INCLUDE_DIRECTORIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES INTERFACE_COMPILE_DEFINITIONS
            INTERFACE_COMPILE_OPTIONS INTERFACE_COMPILE_FEATURES)
        get_target_property(_value zr_vm_core_static ${_property})
        if (NOT _value MATCHES "-NOTFOUND$")
            set_property(TARGET zr_vm_ssa_function_execir_residual_core
                    PROPERTY ${_property} "${_value}")
        endif ()
    endforeach ()
endif ()

add_library(zr_vm_ssa_function_execir_aot_object OBJECT
        "${_zr_function_execir_root}/zr_vm_core/src/zr_vm_core/aot/aot_ir_owned_module.c")
add_executable(zr_vm_ssa_function_exec_ir_owner_test
        "${_zr_function_execir_root}/tests/core/test_ssa_function_exec_ir_owner.c"
        "${_zr_function_execir_root}/tests/core/support/ssa_function_exec_ir_owner_fixture.c"
        "${_zr_function_execir_root}/tests/core/support/ssa_function_exec_ir_owner_ledger.c"
        "${_zr_function_execir_root}/tests/core/support/ssa_exec_ir_owned_module_fixture.c"
        "${_zr_function_execir_root}/tests/core/support/ssa_exec_ir_owner_capture.c"
        "${_zr_function_execir_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_function_execir_root}/tests/core/support/ssa_function_aot_owner_fixture.c")
foreach (_zr_function_execir_target IN ITEMS zr_vm_ssa_function_execir_aot_object
        zr_vm_ssa_function_exec_ir_owner_test)
    target_include_directories(${_zr_function_execir_target} PRIVATE
            "${_zr_function_execir_root}/zr_vm_core/src/zr_vm_core"
            "${_zr_function_execir_root}"
            "${_zr_function_execir_root}/zr_vm_common/include"
            "${_zr_function_execir_root}/zr_vm_core/include"
            "${_zr_function_execir_root}/tests"
            "${_zr_function_execir_root}/tests/third_party/zr_unity/Unity/src")
    target_compile_definitions(${_zr_function_execir_target} PRIVATE
            ZR_LIBRARY_TYPE_STATIC=1 _CRT_SECURE_NO_WARNINGS)
    if (DEFINED ZR_SSA_SOURCE_ROOT)
        target_compile_definitions(${_zr_function_execir_target} PRIVATE
                ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1 ZR_DEBUG=1
                ZR_VM_COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")
        zr_ssa_source_apply_c_settings(${_zr_function_execir_target})
    endif ()
endforeach ()
target_compile_definitions(zr_vm_ssa_function_execir_aot_object PRIVATE ZR_CURRENT_MODULE="zr_vm_core")
target_compile_definitions(zr_vm_ssa_function_exec_ir_owner_test PRIVATE
        ZR_CURRENT_MODULE="zr_vm_ssa_function_exec_ir_owner_test"
        UNITY_INCLUDE_CONFIG_H UNITY_INCLUDE_DOUBLE UNITY_DOUBLE_PRECISION=1e-6
        UNITY_USE_FLUSH_STDOUT)
if (MSVC)
    target_compile_options(zr_vm_ssa_function_execir_aot_object PRIVATE
            "/FI${_zr_function_execir_root}/tests/core/support/ssa_function_exec_ir_owner_aot_shim.h")
    target_link_options(zr_vm_ssa_function_exec_ir_owner_test PRIVATE /STACK:8388608)
else ()
    target_compile_options(zr_vm_ssa_function_execir_aot_object PRIVATE
            -include "${_zr_function_execir_root}/tests/core/support/ssa_function_exec_ir_owner_aot_shim.h")
endif ()
target_link_libraries(zr_vm_ssa_function_exec_ir_owner_test PRIVATE
        zr_vm_ssa_execir_owner_private_core zr_vm_ssa_function_execir_aot_object)
if (DEFINED ZR_SSA_SOURCE_ROOT)
    target_link_libraries(zr_vm_ssa_function_exec_ir_owner_test PRIVATE
            zr_vm_ssa_function_execir_residual_core ssa_source_zr_vm_common ssa_source_xxhash
            ssa_source_utf8proc ssa_source_unity kernel32)
    set(ZR_SSA_FUNCTION_EXECIR_OWNER_MAP_NAME "ssa-function-execir-owner-link.map"
            CACHE STRING "Finite Function ExecIR owner link map basename")
    if (NOT ZR_SSA_FUNCTION_EXECIR_OWNER_MAP_NAME MATCHES "^[A-Za-z0-9][A-Za-z0-9_.-]*\\.map$")
        message(FATAL_ERROR "Function ExecIR owner map must be a basename in the build directory")
    endif ()
    target_link_options(zr_vm_ssa_function_exec_ir_owner_test PRIVATE
            /INCREMENTAL:NO /OPT:REF /VERBOSE:LIB
            "/MAP:${CMAKE_BINARY_DIR}/${ZR_SSA_FUNCTION_EXECIR_OWNER_MAP_NAME}")
else ()
    target_sources(zr_vm_ssa_function_exec_ir_owner_test PRIVATE
            "${_zr_function_execir_root}/tests/third_party/zr_unity/Unity/src/unity.c")
    target_link_libraries(zr_vm_ssa_function_exec_ir_owner_test PRIVATE
            zr_vm_ssa_function_execir_residual_core)
endif ()
add_test(NAME ssa_function_exec_ir_owner COMMAND ${CMAKE_COMMAND} -E env
        --unset=ZR_VM_PROFILE_OUT --unset=ZR_VM_TRACE_CORE_BOOTSTRAP
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
        ZR_VM_PROFILE_INSTRUCTIONS=0 ZR_VM_PROFILE_SLOWPATHS=0
        ZR_VM_PROFILE_HELPERS=0 ZR_VM_PROFILE_MEMORY=0
        $<TARGET_FILE:zr_vm_ssa_function_exec_ir_owner_test>)
set_tests_properties(ssa_function_exec_ir_owner PROPERTIES
        LABELS "ssa;core-lifecycle;unique-ownership;allocator-fault" TIMEOUT 120)
