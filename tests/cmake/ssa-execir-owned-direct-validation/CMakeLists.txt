cmake_minimum_required(VERSION 3.31)
# Compile the actual checkout; this project stores no source copies.
get_filename_component(ZR_SSA_EXECIR_OWNER_SOURCE_ROOT
        "${CMAKE_CURRENT_LIST_DIR}/../../.." REALPATH)
set(_zr_execir_owner_output_root "E:/cargo-targets/zr_vm/build")
cmake_path(ABSOLUTE_PATH CMAKE_BINARY_DIR NORMALIZE OUTPUT_VARIABLE _zr_execir_owner_binary)
cmake_path(IS_PREFIX _zr_execir_owner_output_root "${_zr_execir_owner_binary}"
        NORMALIZE _zr_execir_owner_output_allowed)
if (NOT _zr_execir_owner_output_allowed OR CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "Use a separate E:/cargo-targets/zr_vm/build directory")
endif ()
if (DEFINED CMAKE_TRY_COMPILE_TARGET_TYPE AND
        NOT CMAKE_TRY_COMPILE_TARGET_TYPE STREQUAL "EXECUTABLE")
    message(FATAL_ERROR "Use the actual executable compiler ABI probe")
endif ()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
project(zr_ssa_execir_owned_validation LANGUAGES C)
if (NOT WIN32 OR NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
        NOT CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" OR
        NOT CMAKE_GENERATOR STREQUAL "Ninja" OR
        NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "This finite route requires Windows clang-cl/Ninja/Debug")
endif ()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
foreach (_zr_execir_owner_driver_flag IN ITEMS
        "/clang:-fsanitize=undefined" "/clang:-fno-sanitize-recover=all")
    string(FIND "${CMAKE_C_LINK_EXECUTABLE}" "${_zr_execir_owner_driver_flag}" _flag_position)
    string(FIND "${CMAKE_C_LINK_EXECUTABLE}" "/link" _link_position)
    if (_flag_position LESS 0 OR _link_position LESS 0 OR
            NOT _flag_position LESS _link_position)
        message(FATAL_ERROR "Toolchain must pass UBSan driver flags before /link")
    endif ()
endforeach ()
enable_testing()
function(zr_ssa_execir_owner_apply_settings target)
    target_compile_definitions(${target} PRIVATE
            ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1 ZR_DEBUG=1)
    target_compile_options(${target} PRIVATE /UNDEBUG
            /clang:-fsanitize=undefined /clang:-fno-sanitize-recover=all)
endfunction()
include("${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}/tests/cmake/ssa-execir-owned-module.cmake")

# Keep the existing constructed clone tests intact as an independent prerequisite.
add_executable(zr_vm_ssa_core_model_test
        "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}/tests/parser/test_ssa_core_model.c"
        "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}/tests/parser/exec_ir_clone_test_allocator.c"
        ${_zr_execir_owner_core_sources})
target_include_directories(zr_vm_ssa_core_model_test PRIVATE
        "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}/zr_vm_common/include"
        "${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}/zr_vm_core/include")
target_compile_definitions(zr_vm_ssa_core_model_test PRIVATE
        ZR_LIBRARY_TYPE_STATIC=1 _CRT_SECURE_NO_WARNINGS
        ZR_CURRENT_MODULE="zr_vm_core" ZR_EXEC_IR_TEST_ALLOCATOR)
target_compile_options(zr_vm_ssa_core_model_test PRIVATE
        "/FI${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}/tests/parser/exec_ir_clone_test_allocator.h")
zr_ssa_execir_owner_apply_settings(zr_vm_ssa_core_model_test)
add_test(NAME ssa_core_model COMMAND zr_vm_ssa_core_model_test)
set_tests_properties(ssa_core_model PROPERTIES LABELS "ssa;clone-support" TIMEOUT 120)
include("${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}/tests/cmake/ssa-execir-module-copy.cmake")

set(_zr_module_copy_graph "target\tabsolute_source\tsha256\n")
set(_zr_module_copy_settings "compiler\t${CMAKE_C_COMPILER}\ncompiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\ngenerator\t${CMAKE_GENERATOR}\nbuild_type\t${CMAKE_BUILD_TYPE}\nlinker\t${CMAKE_LINKER}\ntoolchain_file\t${CMAKE_TOOLCHAIN_FILE}\nmake_rules_override_c\t${CMAKE_USER_MAKE_RULES_OVERRIDE_C}\narchive_tool\t${CMAKE_AR}\narchive_rule\t${CMAKE_C_CREATE_STATIC_LIBRARY}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\n")
foreach (_target IN ITEMS zr_vm_ssa_core_model_test zr_vm_ssa_execir_owner_private_core
        zr_vm_ssa_exec_ir_owned_module_test zr_vm_ssa_execir_module_copy_private
        zr_vm_ssa_exec_ir_module_copy_test)
    get_target_property(_sources ${_target} SOURCES)
    foreach (_source IN LISTS _sources)
        get_filename_component(_absolute "${_source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(SHA256 "${_absolute}" _hash)
        string(APPEND _zr_module_copy_graph "${_target}\t${_absolute}\t${_hash}\n")
    endforeach ()
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS
            LINK_OPTIONS LINK_LIBRARIES C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS
            MSVC_RUNTIME_LIBRARY)
        get_target_property(_value ${_target} ${_property})
        if (_value STREQUAL "_value-NOTFOUND")
            set(_value "")
        endif ()
        string(APPEND _zr_module_copy_settings "${_target}.${_property}\t${_value}\n")
    endforeach ()
endforeach ()
file(WRITE "${CMAKE_BINARY_DIR}/module-copy-target-sources.tsv" "${_zr_module_copy_graph}")
file(WRITE "${CMAKE_BINARY_DIR}/module-copy-settings.tsv" "${_zr_module_copy_settings}")
include("${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}/tests/cmake/ssa-canonical-type-lookup.cmake")
include("${ZR_SSA_EXECIR_OWNER_SOURCE_ROOT}/tests/cmake/ssa-primitive-frame-validation.cmake")
