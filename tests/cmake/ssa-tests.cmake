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
