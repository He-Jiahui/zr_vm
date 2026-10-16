if (ZR_VM_BENCHMARK_PYTHON_EXECUTABLE)
    add_test(NAME syntax_migration_embedded_positions_python
            COMMAND "${ZR_VM_BENCHMARK_PYTHON_EXECUTABLE}" -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_syntax_migration_embedded_positions.py")
    set_tests_properties(syntax_migration_embedded_positions_python PROPERTIES TIMEOUT 30)
endif ()
