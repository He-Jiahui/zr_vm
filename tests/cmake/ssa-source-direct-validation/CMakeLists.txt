cmake_minimum_required(VERSION 3.31)

# This is an independent project. Source paths always identify the current
# checkout; only build products and provenance metadata belong in the build tree.
get_filename_component(ZR_SSA_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." REALPATH)
set(_zr_output_root "E:/cargo-targets/zr_vm/build")
cmake_path(ABSOLUTE_PATH CMAKE_BINARY_DIR NORMALIZE OUTPUT_VARIABLE _zr_binary_dir)
cmake_path(IS_PREFIX _zr_output_root "${_zr_binary_dir}" NORMALIZE _zr_output_allowed)
if (NOT _zr_output_allowed OR CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "Use a separate build directory under E:/cargo-targets/zr_vm/build.")
endif ()
if (DEFINED CMAKE_TRY_COMPILE_TARGET_TYPE AND
        NOT CMAKE_TRY_COMPILE_TARGET_TYPE STREQUAL "EXECUTABLE")
    message(FATAL_ERROR "Compiler detection must include the real executable ABI/link probe.")
endif ()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
project(zr_ssa_source_direct_validation LANGUAGES C)
if (NOT WIN32 OR NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
        NOT CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR "This focused native route requires Windows clang-cl and the supplied LLVM toolchain.")
endif ()
if (DEFINED CMAKE_TRY_COMPILE_TARGET_TYPE AND
        NOT CMAKE_TRY_COMPILE_TARGET_TYPE STREQUAL "EXECUTABLE")
    message(FATAL_ERROR "Do not replace executable compiler detection with a static-library probe.")
endif ()
if (NOT CMAKE_GENERATOR STREQUAL "Ninja")
    message(FATAL_ERROR "Use the bounded single-configuration Ninja route.")
endif ()
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif ()
if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "This assertion-enabled source validation uses Debug configuration.")
endif ()

# CMAKE_AR is supplied by the real toolchain. llvm-lib handles normal COFF
# archives; compiler flags, especially sanitizer flags, must not reach it.
get_filename_component(_zr_ar_name "${CMAKE_AR}" NAME)
string(TOLOWER "${_zr_ar_name}" _zr_ar_name)
if (NOT _zr_ar_name STREQUAL "llvm-lib.exe" OR NOT EXISTS "${CMAKE_AR}")
    message(FATAL_ERROR "Set CMAKE_AR to the actual native llvm-lib.exe in the toolchain.")
endif ()
# CMake quotes the expanded tool path; manually quoting this placeholder would
# double-quote llvm-lib's path when the installation directory contains spaces.
set(CMAKE_C_CREATE_STATIC_LIBRARY "<CMAKE_AR> /nologo /out:<TARGET> <OBJECTS>")

option(ZR_SSA_SOURCE_DIRECT_UBSAN "Instrument all selected C inputs with UBSan" ON)
if (ZR_SSA_SOURCE_DIRECT_UBSAN)
    # These driver flags must precede /link, including during toolchain probes.
    # target_link_options would put them after /link in this native driver rule.
    string(FIND "${CMAKE_C_LINK_EXECUTABLE}" "/clang:-fsanitize=undefined" _zr_san_position)
    string(FIND "${CMAKE_C_LINK_EXECUTABLE}" "/clang:-fno-sanitize-recover=all" _zr_recover_position)
    string(FIND "${CMAKE_C_LINK_EXECUTABLE}" "/link" _zr_link_position)
    if (_zr_san_position LESS 0 OR _zr_recover_position LESS 0 OR _zr_link_position LESS 0 OR
            NOT _zr_san_position LESS _zr_link_position OR
            NOT _zr_recover_position LESS _zr_link_position)
        message(FATAL_ERROR "The LLVM toolchain must place UBSan driver flags before /link.")
    endif ()
endif ()
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
enable_testing()

set(_zr_public_includes
        "${ZR_SSA_SOURCE_ROOT}"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_common/include"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_core/include"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/include"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_library/include")
set(_zr_vendor_includes
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_core/third_party/zr_xx_hash"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_core/third_party/zr_xx_hash/xxHash"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_core/third_party/zr_utf8proc"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_core/third_party/zr_utf8proc/utf8proc"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_library/third_party/zr_c_json"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_library/third_party/zr_c_json/cJSON"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_library/third_party/zr_miniz"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_library/third_party/zr_miniz/miniz"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_library/third_party/zr_tiny_dir"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_library/third_party/zr_tiny_dir/tinydir")

function(zr_ssa_source_apply_c_settings target_name)
    target_compile_options(${target_name} PRIVATE /UNDEBUG /Od /MD /W4 /utf-8 /Gy /Gw)
    if (ZR_SSA_SOURCE_DIRECT_UBSAN)
        target_compile_options(${target_name} PRIVATE
                /clang:-fsanitize=undefined /clang:-fno-sanitize-recover=all
                /clang:-fno-omit-frame-pointer)
    endif ()
endfunction()

function(zr_ssa_source_add_module module_name)
    if (module_name STREQUAL "zr_vm_common")
        set(_sources "${ZR_SSA_SOURCE_ROOT}/zr_vm_common/src/zr_vm_common/ssa_platform_contract.c")
    else ()
        file(GLOB_RECURSE _sources CONFIGURE_DEPENDS "${ZR_SSA_SOURCE_ROOT}/${module_name}/src/*.c")
    endif ()
    list(REMOVE_DUPLICATES _sources)
    list(SORT _sources)
    if (NOT _sources)
        message(FATAL_ERROR "Missing actual source pool for ${module_name}.")
    endif ()
    add_library(ssa_source_${module_name} STATIC ${_sources})
    target_include_directories(ssa_source_${module_name} PRIVATE
            ${_zr_public_includes} ${_zr_vendor_includes}
            "${ZR_SSA_SOURCE_ROOT}/${module_name}/src/${module_name}")
    target_compile_definitions(ssa_source_${module_name} PRIVATE
            ZR_CURRENT_MODULE="${module_name}"
            ZR_LIBRARY_TYPE_STATIC=1 ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1
            ZR_DEBUG=1 _CRT_SECURE_NO_WARNINGS
            ZR_VM_COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}"
            UTF8PROC_STATIC MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
    zr_ssa_source_apply_c_settings(ssa_source_${module_name})
endfunction()

# Common is compiled once, rather than embedded in each module archive.
foreach (_module IN ITEMS zr_vm_common zr_vm_core zr_vm_library zr_vm_parser)
    zr_ssa_source_add_module(${_module})
endforeach ()
target_include_directories(ssa_source_zr_vm_parser PRIVATE
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/compiler"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/parser"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/type_inference"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/writer")

# Explicit vendor inputs, no vendor CMake bootstrap, assembly, or source copies.
add_library(ssa_source_xxhash STATIC
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_core/third_party/zr_xx_hash/xxHash/xxhash.c")
add_library(ssa_source_utf8proc STATIC
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_core/third_party/zr_utf8proc/utf8proc/utf8proc.c")
add_library(ssa_source_cjson STATIC
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_library/third_party/zr_c_json/cJSON/cJSON.c")
set(_zr_miniz "${ZR_SSA_SOURCE_ROOT}/zr_vm_library/third_party/zr_miniz/miniz")
if (NOT EXISTS "${_zr_miniz}/miniz_export.h")
    message(FATAL_ERROR "Actual quote-included miniz_export.h is missing; review configuration provenance before proceeding.")
endif ()
add_library(ssa_source_miniz STATIC
        "${_zr_miniz}/miniz.c" "${_zr_miniz}/miniz_tdef.c"
        "${_zr_miniz}/miniz_tinfl.c" "${_zr_miniz}/miniz_zip.c")
target_compile_definitions(ssa_source_utf8proc PRIVATE UTF8PROC_STATIC)
target_compile_definitions(ssa_source_miniz PRIVATE MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
set(_zr_vendor_targets ssa_source_xxhash ssa_source_utf8proc ssa_source_cjson ssa_source_miniz)
foreach (_target IN LISTS _zr_vendor_targets)
    target_include_directories(${_target} PRIVATE ${_zr_vendor_includes})
    target_compile_definitions(${_target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    zr_ssa_source_apply_c_settings(${_target})
endforeach ()

set(_zr_test_includes
        "${ZR_SSA_SOURCE_ROOT}/tests" "${ZR_SSA_SOURCE_ROOT}/tests/harness"
        "${ZR_SSA_SOURCE_ROOT}/tests/third_party/zr_unity/Unity/src")
set(_zr_unity_definitions UNITY_INCLUDE_CONFIG_H UNITY_INCLUDE_DOUBLE
        UNITY_DOUBLE_PRECISION=1e-6 UNITY_USE_FLUSH_STDOUT)
add_library(ssa_source_unity STATIC
        "${ZR_SSA_SOURCE_ROOT}/tests/third_party/zr_unity/Unity/src/unity.c")
target_include_directories(ssa_source_unity PRIVATE ${_zr_test_includes})
target_compile_definitions(ssa_source_unity PRIVATE ${_zr_unity_definitions} _CRT_SECURE_NO_WARNINGS)
zr_ssa_source_apply_c_settings(ssa_source_unity)

add_library(ssa_source_runtime_support STATIC
        "${ZR_SSA_SOURCE_ROOT}/tests/harness/runtime_support.c")
target_include_directories(ssa_source_runtime_support PRIVATE
        ${_zr_public_includes} ${_zr_vendor_includes} ${_zr_test_includes})
target_compile_definitions(ssa_source_runtime_support PRIVATE
        ${_zr_unity_definitions} ZR_LIBRARY_TYPE_STATIC=1
        ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1 ZR_DEBUG=1 _CRT_SECURE_NO_WARNINGS
        UTF8PROC_STATIC MINIZ_NO_ZLIB_COMPATIBLE_NAMES
        ZR_CURRENT_MODULE="ssa_source_runtime_support"
        ZR_VM_COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}"
        ZR_VM_TESTS_SOURCE_DIR="${ZR_SSA_SOURCE_ROOT}/tests"
        ZR_VM_TESTS_BINARY_DIR="${CMAKE_BINARY_DIR}")
zr_ssa_source_apply_c_settings(ssa_source_runtime_support)

add_executable(zr_vm_ssa_source_execbc_vm_test
        "${ZR_SSA_SOURCE_ROOT}/tests/parser/test_ssa_source_execbc_vm.c")

add_executable(zr_vm_ssa_compare_metadata_guards_test
        "${ZR_SSA_SOURCE_ROOT}/tests/parser/test_ssa_compare_metadata_guards.c")
target_include_directories(zr_vm_ssa_source_execbc_vm_test PRIVATE
        ${_zr_public_includes} ${_zr_vendor_includes} ${_zr_test_includes})
target_include_directories(zr_vm_ssa_compare_metadata_guards_test PRIVATE
        ${_zr_public_includes} ${_zr_vendor_includes} ${_zr_test_includes}
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/compiler")
target_compile_definitions(zr_vm_ssa_source_execbc_vm_test PRIVATE
        ${_zr_unity_definitions} ZR_LIBRARY_TYPE_STATIC=1
        ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1 ZR_DEBUG=1 _CRT_SECURE_NO_WARNINGS
        UTF8PROC_STATIC MINIZ_NO_ZLIB_COMPATIBLE_NAMES
        ZR_CURRENT_MODULE="zr_vm_ssa_source_execbc_vm_test"
        ZR_VM_COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}"
        ZR_VM_TESTS_SOURCE_DIR="${ZR_SSA_SOURCE_ROOT}/tests"
        ZR_VM_TESTS_BINARY_DIR="${CMAKE_BINARY_DIR}")
zr_ssa_source_apply_c_settings(zr_vm_ssa_source_execbc_vm_test)
target_compile_definitions(zr_vm_ssa_compare_metadata_guards_test PRIVATE
        ${_zr_unity_definitions} ZR_LIBRARY_TYPE_STATIC=1
        ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1 ZR_DEBUG=1 _CRT_SECURE_NO_WARNINGS
        UTF8PROC_STATIC MINIZ_NO_ZLIB_COMPATIBLE_NAMES
        ZR_CURRENT_MODULE="zr_vm_ssa_compare_metadata_guards_test"
        ZR_VM_COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}"
        ZR_VM_TESTS_SOURCE_DIR="${ZR_SSA_SOURCE_ROOT}/tests"
        ZR_VM_TESTS_BINARY_DIR="${CMAKE_BINARY_DIR}")
zr_ssa_source_apply_c_settings(zr_vm_ssa_compare_metadata_guards_test)
target_link_libraries(zr_vm_ssa_source_execbc_vm_test PRIVATE
        ssa_source_zr_vm_parser ssa_source_zr_vm_core ssa_source_zr_vm_library
        ssa_source_zr_vm_common ${_zr_vendor_targets} ssa_source_unity kernel32)
target_link_libraries(zr_vm_ssa_source_execbc_vm_test PRIVATE ssa_source_runtime_support)
target_link_libraries(zr_vm_ssa_compare_metadata_guards_test PRIVATE
        ssa_source_zr_vm_parser ssa_source_zr_vm_core ssa_source_zr_vm_library
        ssa_source_zr_vm_common ${_zr_vendor_targets} ssa_source_unity kernel32
        ssa_source_runtime_support)
target_link_options(zr_vm_ssa_source_execbc_vm_test PRIVATE
        # The Debug/UBSan core dispatcher frame exceeds Windows' default 1 MiB
        # reserve. Keep sufficient stack space in this validation executable.
        /INCREMENTAL:NO /OPT:REF /VERBOSE:LIB /STACK:8388608
        "/MAP:${CMAKE_BINARY_DIR}/ssa-source-link.map")
target_link_options(zr_vm_ssa_compare_metadata_guards_test PRIVATE
        # The canonical VM dispatcher uses a large Debug/UBSan frame.
        /INCREMENTAL:NO /OPT:REF /STACK:8388608
        "/MAP:${CMAKE_BINARY_DIR}/ssa-compare-metadata-guards-link.map")

# These existing fixtures exercise producer fallback and the unchanged scratch
# eligibility guards affected by scalar value binding.
foreach (_zr_guard_target IN ITEMS
        zr_vm_ssa_source_straight_line_cfg_test
        zr_vm_exec_ir_scalar_scratch_eligibility_test
        zr_vm_ssa_source_callable_return_test
        zr_vm_ssa_source_script_entry_tokens_test
        zr_vm_ssa_source_callable_identity_test
        zr_vm_ssa_source_script_entry_identity_test
        zr_vm_ssa_source_range_identity_test
        zr_vm_template_literal_source_range_test
        zr_vm_expression_fragment_parser_test
        zr_vm_ssa_dead_source_places_test
        zr_vm_ssa_host_primitive_layout_test
        zr_vm_ssa_primitive_source_frame_test
        zr_vm_ssa_primitive_source_frame_storage_test
        zr_vm_ssa_typed_binding_contract_test
        zr_vm_ssa_dominator_cfg_test)
    if (_zr_guard_target STREQUAL "zr_vm_ssa_source_straight_line_cfg_test")
        set(_zr_guard_source tests/parser/test_ssa_source_straight_line_cfg.c)
    elseif (_zr_guard_target STREQUAL "zr_vm_exec_ir_scalar_scratch_eligibility_test")
        set(_zr_guard_source tests/parser/test_exec_ir_scalar_scratch_eligibility.c)
    elseif (_zr_guard_target STREQUAL "zr_vm_ssa_source_script_entry_tokens_test")
        set(_zr_guard_source tests/parser/test_ssa_source_script_entry_tokens.c)
    elseif (_zr_guard_target STREQUAL "zr_vm_ssa_source_callable_identity_test")
        set(_zr_guard_source tests/parser/test_ssa_source_callable_identity.c)
    elseif (_zr_guard_target STREQUAL "zr_vm_ssa_source_script_entry_identity_test")
        set(_zr_guard_source tests/parser/test_ssa_source_script_entry_identity.c)
    elseif (_zr_guard_target STREQUAL "zr_vm_ssa_source_range_identity_test")
        set(_zr_guard_source tests/parser/test_ssa_source_range_identity.c)
    elseif (_zr_guard_target STREQUAL "zr_vm_template_literal_source_range_test")
        set(_zr_guard_source tests/parser/test_template_literal_source_range.c)
    elseif (_zr_guard_target STREQUAL "zr_vm_expression_fragment_parser_test")
        set(_zr_guard_source tests/parser/test_expression_fragment_parser.c)
    elseif (_zr_guard_target STREQUAL "zr_vm_ssa_dead_source_places_test")
        set(_zr_guard_source tests/parser/test_ssa_dead_source_places.c)
    elseif (_zr_guard_target STREQUAL "zr_vm_ssa_host_primitive_layout_test")
        set(_zr_guard_source tests/parser/test_ssa_host_primitive_layout.c)
    elseif (_zr_guard_target STREQUAL "zr_vm_ssa_primitive_source_frame_test")
        set(_zr_guard_source tests/parser/test_ssa_primitive_source_frame.c)
    elseif (_zr_guard_target STREQUAL "zr_vm_ssa_primitive_source_frame_storage_test")
        set(_zr_guard_source tests/parser/test_ssa_primitive_source_frame_storage.c)
    elseif (_zr_guard_target STREQUAL "zr_vm_ssa_typed_binding_contract_test")
        set(_zr_guard_source tests/parser/test_ssa_typed_binding_contract_witness.c)
    elseif (_zr_guard_target STREQUAL "zr_vm_ssa_dominator_cfg_test")
        set(_zr_guard_source tests/parser/test_ssa_dominator_cfg.c)
    else ()
        set(_zr_guard_source tests/parser/test_ssa_source_callable_return.c)
    endif ()
    add_executable(${_zr_guard_target} "${ZR_SSA_SOURCE_ROOT}/${_zr_guard_source}")
    if (_zr_guard_target STREQUAL "zr_vm_ssa_dead_source_places_test" OR
            _zr_guard_target STREQUAL "zr_vm_ssa_host_primitive_layout_test" OR
            _zr_guard_target STREQUAL "zr_vm_ssa_primitive_source_frame_test" OR
            _zr_guard_target STREQUAL "zr_vm_ssa_primitive_source_frame_storage_test")
        target_sources(${_zr_guard_target} PRIVATE
                "${ZR_SSA_SOURCE_ROOT}/tests/parser/support/ssa_literal_script_fixture.c")
    endif ()
    target_include_directories(${_zr_guard_target} PRIVATE
            ${_zr_public_includes} ${_zr_vendor_includes} ${_zr_test_includes}
            "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser"
            "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/compiler"
            "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir")
    target_compile_definitions(${_zr_guard_target} PRIVATE
            ${_zr_unity_definitions} ZR_LIBRARY_TYPE_STATIC=1
            ZR_PLATFORM_WIN=1 ZR_PLATFORM_WIN_USE_MSVC=1 ZR_DEBUG=1 _CRT_SECURE_NO_WARNINGS
            UTF8PROC_STATIC MINIZ_NO_ZLIB_COMPATIBLE_NAMES
            ZR_CURRENT_MODULE="${_zr_guard_target}"
            ZR_VM_COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}"
            ZR_VM_TESTS_SOURCE_DIR="${ZR_SSA_SOURCE_ROOT}/tests"
            ZR_VM_TESTS_BINARY_DIR="${CMAKE_BINARY_DIR}")
    zr_ssa_source_apply_c_settings(${_zr_guard_target})
    target_link_libraries(${_zr_guard_target} PRIVATE
            ssa_source_zr_vm_parser ssa_source_zr_vm_core ssa_source_zr_vm_library
            ssa_source_zr_vm_common ${_zr_vendor_targets} ssa_source_unity kernel32
            ssa_source_runtime_support)
    target_link_options(${_zr_guard_target} PRIVATE
            /INCREMENTAL:NO /OPT:REF /STACK:8388608)
endforeach ()

# A failed comparison remains a genuine CTest failure. No WILL_FAIL or wrapper.
add_test(NAME ssa_source_execbc_vm_comparisons
        COMMAND zr_vm_ssa_source_execbc_vm_test --comparisons-only)
add_test(NAME ssa_source_execbc_vm_regressions
        COMMAND zr_vm_ssa_source_execbc_vm_test --regressions-only)
add_test(NAME ssa_source_execbc_vm_loops
        COMMAND zr_vm_ssa_source_execbc_vm_test --loops-only)
add_test(NAME ssa_source_execbc_vm_scalar_guards
        COMMAND zr_vm_ssa_source_execbc_vm_test --scalar-guards-only)
add_test(NAME ssa_compare_metadata_guards
        COMMAND zr_vm_ssa_compare_metadata_guards_test)
add_test(NAME ssa_source_straight_line_cfg
        COMMAND zr_vm_ssa_source_straight_line_cfg_test)
add_test(NAME exec_ir_scalar_scratch_eligibility
        COMMAND zr_vm_exec_ir_scalar_scratch_eligibility_test)
add_test(NAME ssa_source_callable_return
        COMMAND zr_vm_ssa_source_callable_return_test)
add_test(NAME ssa_source_script_entry_tokens
        COMMAND zr_vm_ssa_source_script_entry_tokens_test)
add_test(NAME ssa_source_callable_identity
        COMMAND zr_vm_ssa_source_callable_identity_test)
add_test(NAME ssa_source_script_entry_identity
        COMMAND zr_vm_ssa_source_script_entry_identity_test)
add_test(NAME ssa_source_range_identity
        COMMAND zr_vm_ssa_source_range_identity_test)
add_test(NAME template_literal_source_range
        COMMAND zr_vm_template_literal_source_range_test)
add_test(NAME expression_fragment_parser
        COMMAND zr_vm_expression_fragment_parser_test)
add_test(NAME ssa_dead_source_places
        COMMAND zr_vm_ssa_dead_source_places_test)
add_test(NAME ssa_host_primitive_layout
        COMMAND zr_vm_ssa_host_primitive_layout_test)
add_test(NAME ssa_primitive_source_frame
        COMMAND zr_vm_ssa_primitive_source_frame_test)
add_test(NAME ssa_primitive_source_frame_storage
        COMMAND zr_vm_ssa_primitive_source_frame_storage_test)
add_test(NAME ssa_typed_binding_contract
        COMMAND zr_vm_ssa_typed_binding_contract_test)
add_test(NAME ssa_dominator_cfg
        COMMAND zr_vm_ssa_dominator_cfg_test)
set_tests_properties(ssa_source_execbc_vm_comparisons ssa_source_execbc_vm_regressions
        ssa_source_execbc_vm_loops
        ssa_source_execbc_vm_scalar_guards
        PROPERTIES LABELS "ssa;source-direct-validation" TIMEOUT 120
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
set_tests_properties(ssa_compare_metadata_guards PROPERTIES
        LABELS "ssa;source-direct-validation" TIMEOUT 120
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
set_tests_properties(ssa_source_straight_line_cfg exec_ir_scalar_scratch_eligibility
        ssa_source_callable_return
        ssa_source_script_entry_tokens ssa_source_callable_identity
        ssa_source_script_entry_identity ssa_source_range_identity
        template_literal_source_range expression_fragment_parser
        ssa_dead_source_places
        ssa_host_primitive_layout
        ssa_primitive_source_frame
        ssa_primitive_source_frame_storage
        ssa_typed_binding_contract
        PROPERTIES LABELS "ssa;source-direct-validation" TIMEOUT 120
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")

# Source membership and hashes are metadata only. Sources/headers are never copied.
set(_zr_targets ssa_source_zr_vm_parser ssa_source_zr_vm_core ssa_source_zr_vm_library
        ssa_source_zr_vm_common ${_zr_vendor_targets} ssa_source_unity ssa_source_runtime_support
        zr_vm_ssa_source_execbc_vm_test zr_vm_ssa_compare_metadata_guards_test
        zr_vm_ssa_source_straight_line_cfg_test zr_vm_exec_ir_scalar_scratch_eligibility_test
        zr_vm_ssa_source_callable_return_test zr_vm_ssa_source_script_entry_tokens_test
        zr_vm_ssa_source_callable_identity_test
        zr_vm_ssa_source_script_entry_identity_test zr_vm_ssa_source_range_identity_test
        zr_vm_template_literal_source_range_test zr_vm_expression_fragment_parser_test
        zr_vm_ssa_dead_source_places_test
        zr_vm_ssa_host_primitive_layout_test
        zr_vm_ssa_primitive_source_frame_test
        zr_vm_ssa_primitive_source_frame_storage_test
        zr_vm_ssa_typed_binding_contract_test)
set(_zr_graph "target\tabsolute_source\tsha256\n")
set(_zr_settings "compiler\t${CMAKE_C_COMPILER}\ncompiler_id\t${CMAKE_C_COMPILER_ID}\ncompiler_version\t${CMAKE_C_COMPILER_VERSION}\narchive_tool\t${CMAKE_AR}\narchive_rule\t${CMAKE_C_CREATE_STATIC_LIBRARY}\nlink_rule\t${CMAKE_C_LINK_EXECUTABLE}\n")
foreach (_target IN LISTS _zr_targets)
    # A shared fixture is a valid source of several distinct targets. Reject
    # duplicate membership within a target while recording every target pair.
    set(_zr_target_sources)
    get_target_property(_sources ${_target} SOURCES)
    list(LENGTH _sources _count)
    string(APPEND _zr_settings "source_count\t${_target}\t${_count}\n")
    foreach (_property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS LINK_LIBRARIES LINK_OPTIONS)
        get_target_property(_value ${_target} ${_property})
        if (_value AND NOT _value MATCHES "-NOTFOUND$")
            string(APPEND _zr_settings "${_property}\t${_target}\t${_value}\n")
        endif ()
    endforeach ()
    foreach (_source IN LISTS _sources)
        file(REAL_PATH "${_source}" _absolute)
        cmake_path(IS_PREFIX ZR_SSA_SOURCE_ROOT "${_absolute}" NORMALIZE _inside_checkout)
        if (NOT _inside_checkout OR NOT EXISTS "${_absolute}" OR _absolute IN_LIST _zr_target_sources)
            message(FATAL_ERROR "Missing, duplicate or non-checkout source: ${_absolute}")
        endif ()
        list(APPEND _zr_target_sources "${_absolute}")
        file(SHA256 "${_absolute}" _hash)
        string(APPEND _zr_graph "${_target}\t${_absolute}\t${_hash}\n")
    endforeach ()
endforeach ()
set(_zr_header_hashes "absolute_header\tsha256\n")
foreach (_header IN ITEMS
        tests/unity_config.h
        tests/parser/ssa_source_execbc_vm_diagnostics.inc
        tests/parser/ssa_source_execbc_vm_loop_break.inc
        tests/parser/ssa_source_execbc_vm_loop_phi.inc
        tests/parser/ssa_source_execbc_vm_scalar_guards.inc
        tests/parser/ssa_source_execbc_vm_compare.inc
        tests/parser/support/ssa_literal_script_fixture.h
        zr_vm_parser/include/zr_vm_parser/exec_ir_dead_source_places.h
        zr_vm_parser/include/zr_vm_parser/exec_ir_host_primitive_layout.h
        zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
        zr_vm_library/third_party/zr_miniz/miniz/miniz_export.h)
    file(SHA256 "${ZR_SSA_SOURCE_ROOT}/${_header}" _hash)
    string(APPEND _zr_header_hashes "${ZR_SSA_SOURCE_ROOT}/${_header}\t${_hash}\n")
endforeach ()
file(WRITE "${CMAKE_BINARY_DIR}/source-direct-target-sources.tsv" "${_zr_graph}")
file(WRITE "${CMAKE_BINARY_DIR}/source-direct-target-settings.tsv" "${_zr_settings}")
file(WRITE "${CMAKE_BINARY_DIR}/source-direct-header-hashes.tsv" "${_zr_header_hashes}")
