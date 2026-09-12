# Central registration point for the SSA plan's focused tests.
# Keep these targets independent from the large legacy test list so an SSA
# milestone can be built and validated without changing existing suites.

if (NOT TARGET zr_vm_ssa_baseline_metrics_test)
    add_executable(
            zr_vm_ssa_baseline_metrics_test
            ${CMAKE_SOURCE_DIR}/tests/benchmarks/test_ssa_baseline_metrics.c
            ${CMAKE_SOURCE_DIR}/tests/performance/perf_ssa_metrics.c
            ${CMAKE_SOURCE_DIR}/tests/performance/perf_statistics.c
    )
    target_include_directories(zr_vm_ssa_baseline_metrics_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/performance
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
    target_compile_definitions(zr_vm_ssa_baseline_metrics_test PRIVATE
            _CRT_SECURE_NO_WARNINGS
    )
    if (NOT WIN32)
        target_link_libraries(zr_vm_ssa_baseline_metrics_test PRIVATE m)
    endif ()
    add_test(NAME ssa_baseline_metrics COMMAND zr_vm_ssa_baseline_metrics_test)
    set_tests_properties(ssa_baseline_metrics PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_contract_freeze_test)
    add_executable(
            zr_vm_ssa_contract_freeze_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_contract_freeze.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
    )
    target_include_directories(zr_vm_ssa_contract_freeze_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
    target_compile_definitions(zr_vm_ssa_contract_freeze_test PRIVATE
            _CRT_SECURE_NO_WARNINGS
    )
    add_test(NAME ssa_contract_freeze COMMAND zr_vm_ssa_contract_freeze_test)
    set_tests_properties(ssa_contract_freeze PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_differential_harness_test)
    add_executable(
            zr_vm_ssa_differential_harness_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_ssa_differential_harness.c
            ${CMAKE_SOURCE_DIR}/tests/harness/ssa_differential_support.c
    )
    target_include_directories(zr_vm_ssa_differential_harness_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/harness
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
    target_compile_definitions(zr_vm_ssa_differential_harness_test PRIVATE
            _CRT_SECURE_NO_WARNINGS
    )
    add_test(NAME ssa_differential_harness COMMAND zr_vm_ssa_differential_harness_test)
    set_tests_properties(ssa_differential_harness PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_core_model_test)
    add_executable(
            zr_vm_ssa_core_model_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_core_model.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
    )
    target_include_directories(zr_vm_ssa_core_model_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
    target_compile_definitions(zr_vm_ssa_core_model_test PRIVATE
            _CRT_SECURE_NO_WARNINGS
    )
    add_test(NAME ssa_core_model COMMAND zr_vm_ssa_core_model_test)
    set_tests_properties(ssa_core_model PROPERTIES LABELS "ssa")
endif ()

if (NOT TARGET zr_vm_ssa_construction_test)
    add_executable(zr_vm_ssa_construction_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_construction.c)
    zr_vm_apply_common_test_settings(zr_vm_ssa_construction_test)
    target_include_directories(zr_vm_ssa_construction_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    target_compile_definitions(zr_vm_ssa_construction_test PRIVATE UNITY_INCLUDE_CONFIG_H)
    zr_link_third_party_for_target(zr_vm_ssa_construction_test "zr_unity")
    zr_vm_link_parser_core(zr_vm_ssa_construction_test)
    add_test(NAME ssa_construction COMMAND zr_vm_ssa_construction_test)
    set_tests_properties(ssa_construction PROPERTIES LABELS "ssa")
endif ()
