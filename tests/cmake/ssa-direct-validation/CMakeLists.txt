cmake_minimum_required(VERSION 3.31)

# Real source root remains this bounded subproject. Production files are read
# directly from the checkout; no source tree is staged or copied.
get_filename_component(ZR_SSA_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(_zr_finite_fragments
        ssa-analysis-fact-capacity-tests.cmake
        ssa-branch-range-null-tests.cmake
        ssa-branch-arithmetic-constraints-tests.cmake
        ssa-branch-loop-ranges-tests.cmake
        ssa-aot-scalar-arithmetic-tests.cmake
        ssa-aot-scalar-conditional-tests.cmake
        ssa-licm-scalar-legality.cmake)
project(zr_ssa_direct_validation LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
enable_testing()
option(ZR_SSA_DIRECT_UBSAN "Instrument these local C fixtures with UBSan" ON)
if (ZR_SSA_DIRECT_UBSAN AND CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    # LINK_FLAGS belongs after /link in the verified driver rule. Sanitizer
    # driver options must instead be supplied explicitly before that separator.
    string(FIND "${CMAKE_C_LINK_EXECUTABLE}" "/clang:-fsanitize=undefined" _zr_san_position)
    string(FIND "${CMAKE_C_LINK_EXECUTABLE}" "/clang:-fno-sanitize-recover=all" _zr_recover_position)
    string(FIND "${CMAKE_C_LINK_EXECUTABLE}" "/link" _zr_link_position)
    if (_zr_san_position LESS 0 OR _zr_recover_position LESS 0 OR _zr_link_position LESS 0 OR
            NOT _zr_san_position LESS _zr_link_position OR
            NOT _zr_recover_position LESS _zr_link_position)
        message(FATAL_ERROR "The native LLVM toolchain must place UBSan driver options before /link.")
    endif ()
endif ()

# A bounded test configuration does not register modules or third-party targets.
# CommonMacros currently assumes the full repository CMAKE_SOURCE_DIR.
function(zr_vm_apply_common_test_settings target_name)
    target_include_directories(${target_name} PRIVATE
            "${ZR_SSA_SOURCE_ROOT}/tests"
            "${ZR_SSA_SOURCE_ROOT}/tests/harness"
            "${ZR_SSA_SOURCE_ROOT}/zr_vm_common/include")
    target_compile_definitions(${target_name} PRIVATE
            _CRT_SECURE_NO_WARNINGS
            ZR_VM_TESTS_SOURCE_DIR="${ZR_SSA_SOURCE_ROOT}/tests"
            ZR_VM_TESTS_BINARY_DIR="${CMAKE_BINARY_DIR}")
endfunction()

# Explicit lower support closure for the existing loop assertions and LICM.
set(_zr_core "${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core")
set(_zr_parser "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/src/zr_vm_parser/exec_ir")
set(_zr_loop_support)
foreach (_source IN ITEMS
        exec_ir exec_ir_binding_rows exec_ir_verify exec_ir_verify_constants
        exec_ir_verify_ssa exec_ir_verify_effects exec_ir_verify_effect_backedges
        exec_ir_materialize exec_ir_materialize_owners exec_ir_state_map_liveness
        exec_ir_deopt_aggregate exec_ir_state_map_storage exec_ir_owner_state)
    list(APPEND _zr_loop_support "${_zr_core}/exec_ir/${_source}.c")
endforeach ()
list(APPEND _zr_loop_support
        "${_zr_core}/execution_contract.c"
        "${_zr_parser}/analysis/exec_ir_loops.c"
        "${_zr_parser}/exec_ir_effects.c"
        "${_zr_parser}/exec_ir_effects_linear.c"
        "${_zr_parser}/exec_ir_effect_loops.c"
        "${_zr_parser}/passes/exec_ir_licm.c"
        "${_zr_parser}/exec_ir_profile.c")
add_executable(zr_vm_ssa_loops_specialization_test
        "${ZR_SSA_SOURCE_ROOT}/tests/parser/test_ssa_loops_specialization.c"
        ${_zr_loop_support})
target_include_directories(zr_vm_ssa_loops_specialization_test PRIVATE
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/include"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_core/include"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_common/include")
zr_vm_apply_common_test_settings(zr_vm_ssa_loops_specialization_test)
add_test(NAME ssa_loops_specialization COMMAND zr_vm_ssa_loops_specialization_test)

# Register the original scalar-text consumer before the conditional fragment;
# that owned fragment adds the arithmetic and conditional helper TUs exactly once.
set(_zr_backend "${ZR_SSA_SOURCE_ROOT}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot")
add_executable(zr_vm_ssa_aot_scalar_text_test
        "${ZR_SSA_SOURCE_ROOT}/tests/parser/test_ssa_aot_scalar_text.c"
        "${_zr_core}/aot_ir.c"
        "${_zr_core}/exec_ir/exec_ir_state_map_storage.c"
        "${_zr_backend}/backend_aot_ir_scalar_text.c"
        "${_zr_backend}/backend_aot_ir_scalar_c.c"
        "${_zr_backend}/backend_aot_ir_scalar_llvm.c")
target_include_directories(zr_vm_ssa_aot_scalar_text_test PRIVATE
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/include"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_core/include"
        "${ZR_SSA_SOURCE_ROOT}/zr_vm_common/include"
        "${_zr_backend}")
zr_vm_apply_common_test_settings(zr_vm_ssa_aot_scalar_text_test)
add_test(NAME ssa_aot_scalar_text COMMAND zr_vm_ssa_aot_scalar_text_test)

foreach (_fragment IN LISTS _zr_finite_fragments)
    include("${ZR_SSA_SOURCE_ROOT}/tests/cmake/${_fragment}")
endforeach ()
set(_zr_targets
        zr_vm_ssa_loops_specialization_test
        zr_vm_ssa_aot_scalar_text_test
        zr_vm_ssa_analysis_fact_capacity_test
        zr_vm_ssa_branch_range_null_test
        zr_vm_ssa_branch_arithmetic_constraints_test
        zr_vm_ssa_branch_loop_ranges_test
        zr_vm_ssa_aot_scalar_arithmetic_test
        zr_vm_ssa_aot_scalar_conditional_test
        zr_vm_ssa_licm_scalar_legality_test)
foreach (_target IN LISTS _zr_targets)
    if (MSVC)
        target_compile_options(${_target} PRIVATE /UNDEBUG)
    else ()
        target_compile_options(${_target} PRIVATE -UNDEBUG)
    endif ()
    if (ZR_SSA_DIRECT_UBSAN)
        if (NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
            message(FATAL_ERROR "This bounded UBSan configuration requires Clang.")
        endif ()
        if (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            target_compile_options(${_target} PRIVATE
                    /clang:-fsanitize=undefined /clang:-fno-sanitize-recover=all
                    /clang:-fno-omit-frame-pointer)
        else ()
            target_compile_options(${_target} PRIVATE
                    -fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
            target_link_options(${_target} PRIVATE -fsanitize=undefined -fno-sanitize-recover=all)
        endif ()
    endif ()
endforeach ()
set_tests_properties(
        ssa_loops_specialization ssa_aot_scalar_text ssa_analysis_fact_capacity
        ssa_branch_range_null ssa_branch_arithmetic_constraints ssa_branch_loop_ranges
        ssa_aot_scalar_arithmetic ssa_aot_scalar_conditional ssa_licm_scalar_legality
        PROPERTIES LABELS "ssa;direct-validation" TIMEOUT 30
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")

# Publish the actual target source graph for independent validation.
set(_zr_graph "")
foreach (_target IN LISTS _zr_targets)
    get_target_property(_sources ${_target} SOURCES)
    set(_zr_seen_sources)
    foreach (_source IN LISTS _sources)
        get_filename_component(_absolute "${_source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        cmake_path(IS_PREFIX ZR_SSA_SOURCE_ROOT "${_absolute}" NORMALIZE _inside_checkout)
        if (NOT _inside_checkout OR NOT EXISTS "${_absolute}")
            message(FATAL_ERROR "Unexpected validation source: ${_absolute}")
        endif ()
        if (_absolute IN_LIST _zr_seen_sources)
            message(FATAL_ERROR "Duplicate validation source in ${_target}: ${_absolute}")
        endif ()
        list(APPEND _zr_seen_sources "${_absolute}")
        string(APPEND _zr_graph "${_target}\t${_absolute}\n")
    endforeach ()
endforeach ()
file(WRITE "${CMAKE_BINARY_DIR}/direct-target-sources.tsv" "${_zr_graph}")
