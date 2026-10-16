if (ZR_VM_BENCHMARK_PYTHON_EXECUTABLE)
    add_test(NAME benchmark_consumer_scope_types_python
            COMMAND "${ZR_VM_BENCHMARK_PYTHON_EXECUTABLE}" -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/test_benchmark_consumer_scope_types.py")
    set_tests_properties(benchmark_consumer_scope_types_python PROPERTIES TIMEOUT 30)
endif ()
