if (ZR_VM_BENCHMARK_PYTHON_EXECUTABLE)
    find_program(ZR_VM_BENCHMARK_VIEWER_NODE NAMES node nodejs)
    if (ZR_VM_BENCHMARK_VIEWER_NODE)
        add_test(NAME benchmark_compare_viewer_python
                COMMAND "${ZR_VM_BENCHMARK_PYTHON_EXECUTABLE}" -B
                        "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/test_benchmark_compare_viewer.py")
        set_tests_properties(benchmark_compare_viewer_python PROPERTIES
                ENVIRONMENT "BENCHMARK_VIEWER_NODE=${ZR_VM_BENCHMARK_VIEWER_NODE};PYTHONIOENCODING=utf-8"
                TIMEOUT 120)
    endif ()
endif ()
