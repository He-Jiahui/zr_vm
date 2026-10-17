if (ZR_VM_BENCHMARK_PYTHON_EXECUTABLE)
    add_test(NAME benchmark_consumer_scope_types_python
            COMMAND "${ZR_VM_BENCHMARK_PYTHON_EXECUTABLE}" -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/test_benchmark_consumer_scope_types.py")
    set_tests_properties(benchmark_consumer_scope_types_python PROPERTIES TIMEOUT 30)
endif ()

# @unit benchmark_consumer_scope_types_python registration
# Summary: 在已找到Python时注册scope类型unittest的CTest入口与30秒timeout。
#
# Constraints and lifecycle: 依赖父tests目录CMAKE_CURRENT_SOURCE_DIR与ZR_VM_BENCHMARK_PYTHON_EXECUTABLE；条件只判断truthy，不执行版本探测。命令带-B禁pyc缓存，不代表禁止全部I/O；本测试的报告边界由mock替代。false时不注册、无编译目标、无默认报告输出；脚本须兼容当前类型注解语法。set_tests_properties对刚注册的唯一名称生效。
# References: tests/cmake/benchmark-scope-types-tests.cmake:1;tests/cmake/benchmark-scope-types-tests.cmake:2;tests/cmake/benchmark-scope-types-tests.cmake:3;tests/cmake/benchmark-scope-types-tests.cmake:4;tests/cmake/benchmark-scope-types-tests.cmake:5;tests/cmake/benchmark-scope-types-tests.cmake:6;tests/CMakeLists.txt:58;tests/CMakeLists.txt:778
