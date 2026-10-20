if (TARGET zr_vm_cli_executable)
    add_test(
            NAME cli_integration
            COMMAND ${CMAKE_COMMAND}
            "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
            "-DPROJECT_FIXTURES_DIR=${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}/fixtures/projects"
            "-DGENERATED_DIR=${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_cli_suite.cmake
    )
endif ()

if (TARGET zr_vm_cli_executable AND TARGET zr_vm_perf_runner AND
    TARGET zr_vm_native_benchmark_runner AND TARGET zr_vm_zr_benchmark_server)
    if (ZR_VM_REGISTER_PERFORMANCE_CTEST)
        add_test(
                NAME performance_report
                COMMAND ${CMAKE_COMMAND}
                "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
                "-DPERF_RUNNER_EXE=$<TARGET_FILE:zr_vm_perf_runner>"
                "-DNATIVE_BENCHMARK_EXE=$<TARGET_FILE:zr_vm_native_benchmark_runner>"
                "-DZR_BENCHMARK_SERVER_EXE=$<TARGET_FILE:zr_vm_zr_benchmark_server>"
                "-DBENCHMARKS_DIR=${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}/benchmarks"
                "-DGENERATED_DIR=${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}"
                "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
                -P ${ZR_VM_PERFORMANCE_RUNNER_SCRIPT}
        )
        set_tests_properties(performance_report PROPERTIES
                LABELS "benchmark;long_running"
                RUN_SERIAL TRUE
                TIMEOUT 1800
        )
    endif ()

    add_custom_target(run_performance_suite
            COMMAND ${CMAKE_COMMAND}
                    "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
                    "-DPERF_RUNNER_EXE=$<TARGET_FILE:zr_vm_perf_runner>"
                    "-DNATIVE_BENCHMARK_EXE=$<TARGET_FILE:zr_vm_native_benchmark_runner>"
                    "-DZR_BENCHMARK_SERVER_EXE=$<TARGET_FILE:zr_vm_zr_benchmark_server>"
                    "-DBENCHMARKS_DIR=${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}/benchmarks"
                    "-DGENERATED_DIR=${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}"
                    "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
                    -P ${ZR_VM_PERFORMANCE_RUNNER_SCRIPT}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run cross-language performance suite (same script as CTest performance_report)"
    )
    add_dependencies(run_performance_suite
            zr_vm_cli_executable
            zr_vm_perf_runner
            zr_vm_native_benchmark_runner
            zr_vm_zr_benchmark_server
    )
endif ()

if (TARGET zr_vm_benchmark_registry_test)
    add_test(
            NAME benchmark_registry
            COMMAND ${CMAKE_COMMAND}
                    "-DEXE=$<TARGET_FILE:zr_vm_benchmark_registry_test>"
                    "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_benchmark_registry_test.cmake
    )
endif ()

add_test(
        NAME benchmark_registry_contract
        COMMAND ${CMAKE_COMMAND}
                "-DREGISTRY_FILE=${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/registry.cmake"
                "-DFIXTURE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/benchmark_registry_contract_fixture.cmake"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_benchmark_registry_contract_test.cmake
)

if (ZR_VM_BENCHMARK_PYTHON_EXECUTABLE)
    add_test(
            NAME benchmark_statistics_python
            COMMAND "${ZR_VM_BENCHMARK_PYTHON_EXECUTABLE}"
                    ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/test_benchmark_statistics.py
    )
    add_test(
            NAME benchmark_execution_plan_python
            COMMAND "${ZR_VM_BENCHMARK_PYTHON_EXECUTABLE}"
                    ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/test_benchmark_execution_plan.py
    )
    add_test(
            NAME benchmark_task3_report_consumers_python
            COMMAND "${ZR_VM_BENCHMARK_PYTHON_EXECUTABLE}"
                    ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/test_benchmark_task3_report_consumers.py
    )
    add_test(
            NAME benchmark_task4_integration_python
            COMMAND "${ZR_VM_BENCHMARK_PYTHON_EXECUTABLE}"
                    ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/test_benchmark_task4_integration.py
    )
    add_test(
            NAME benchmark_task3_suite_contract
            COMMAND ${CMAKE_COMMAND}
                    "-DTASK3_MODULE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/benchmark_task3_suite.cmake"
                    "-DEXECUTION_PLAN_SCRIPT=${CMAKE_SOURCE_DIR}/scripts/benchmark/benchmark_execution_plan.py"
                    "-DPERFORMANCE_SUITE_SCRIPT=${ZR_VM_PERFORMANCE_RUNNER_SCRIPT}"
                    "-DASSEMBLY_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/benchmark_task3_case_assembly.cmake"
                    "-DTESTS_CMAKE=${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
                    "-DPYTHON_EXE=${ZR_VM_BENCHMARK_PYTHON_EXECUTABLE}"
                    "-DTEST_OUTPUT_DIR=${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}/benchmark_task3_suite_contract"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_benchmark_task3_suite_contract_test.cmake
    )
endif ()

add_test(
        NAME benchmark_task4_environment_contract
        COMMAND ${CMAKE_COMMAND}
                "-DTASK4_MODULE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/benchmark_task4_environment.cmake"
                "-DPERFORMANCE_SUITE_SCRIPT=${ZR_VM_PERFORMANCE_RUNNER_SCRIPT}"
                "-DASSEMBLY_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/benchmark_task3_case_assembly.cmake"
                "-DTEST_OUTPUT_DIR=${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}/benchmark_task4_environment_contract"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_benchmark_task4_environment_contract_test.cmake
)

if (TARGET zr_vm_benchmark_support_test AND TARGET zr_vm_perf_runner AND ZR_VM_BENCHMARK_PYTHON_EXECUTABLE)
    add_test(
            NAME benchmark_support
            COMMAND ${CMAKE_COMMAND}
                    "-DEXE=$<TARGET_FILE:zr_vm_benchmark_support_test>"
                    "-DPERF_RUNNER_EXE=$<TARGET_FILE:zr_vm_perf_runner>"
                    "-DPYTHON_EXE=${ZR_VM_BENCHMARK_PYTHON_EXECUTABLE}"
                    "-DMEASUREMENT_CONTRACT_MODULE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/benchmark_measurement_contract.cmake"
                    "-DPERFORMANCE_SUITE_SCRIPT=${ZR_VM_PERFORMANCE_RUNNER_SCRIPT}"
                    "-DCSV_SCRIPT=${CMAKE_SOURCE_DIR}/scripts/benchmark/benchmark_reports_to_csv.py"
                    "-DAGGREGATE_SCRIPT=${CMAKE_SOURCE_DIR}/scripts/benchmark/aggregate_benchmark_summary.py"
                    "-DTEST_OUTPUT_DIR=${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}/benchmark_support"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_benchmark_support_test.cmake
    )
endif ()

if (TARGET zr_vm_perf_runner_persistent_protocol_test AND TARGET zr_vm_perf_runner)
    add_test(
            NAME benchmark_persistent_protocol
            COMMAND ${CMAKE_COMMAND}
                    "-DPERF_RUNNER_EXE=$<TARGET_FILE:zr_vm_perf_runner>"
                    "-DFIXTURE_EXE=$<TARGET_FILE:zr_vm_perf_runner_persistent_protocol_test>"
                    "-DTEST_OUTPUT_DIR=${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}/benchmark_persistent_protocol"
                    "-DBENCHMARKS_DIR=${CMAKE_SOURCE_DIR}/tests/benchmarks"
                    "-DPERSISTENT_COMMAND_MODULE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/benchmark_persistent_commands.cmake"
                    $<$<TARGET_EXISTS:zr_vm_cli_executable>:-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>>
                    $<$<TARGET_EXISTS:zr_vm_zr_benchmark_server>:-DZR_BENCHMARK_SERVER_EXE=$<TARGET_FILE:zr_vm_zr_benchmark_server>>
                    "-DDOTNET_PROJECT=${CMAKE_SOURCE_DIR}/tests/benchmarks/dotnet_runner/BenchmarkRunner.csproj"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_perf_runner_persistent_protocol_test.cmake
    )
    set_tests_properties(benchmark_persistent_protocol PROPERTIES TIMEOUT 120)
endif ()

if (TARGET zr_vm_perf_statistics_test)
    add_test(NAME benchmark_perf_statistics COMMAND zr_vm_perf_statistics_test)
endif ()

if (TARGET zr_vm_perf_report_test)
    add_test(NAME benchmark_perf_report COMMAND zr_vm_perf_report_test)
endif ()
