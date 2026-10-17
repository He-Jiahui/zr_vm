if (ZR_VM_BENCHMARK_PYTHON_EXECUTABLE)
    add_test(NAME syntax_migration_embedded_positions_python
            COMMAND "${ZR_VM_BENCHMARK_PYTHON_EXECUTABLE}" -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_syntax_migration_embedded_positions.py")
    set_tests_properties(syntax_migration_embedded_positions_python PROPERTIES TIMEOUT 30)
endif ()

# @unit syntax_migration_embedded_positions_python::registration [block]
# ## Summary
# 在测试目录配置期间，仅当已解析的 Python 可执行程序变量为真时注册现有的宿主位置 unittest 脚本，并为该 CTest 项设置三十秒超时。
# ## Parameters
# ZR_VM_BENCHMARK_PYTHON_EXECUTABLE 由上层 find_program 或 cache 提供；CMAKE_CURRENT_SOURCE_DIR 在此 include 时必须仍是仓库 tests 目录。
# ## Returns
# 条件成立时新增 syntax_migration_embedded_positions_python 测试，命令为所选 Python、-B 和脚本路径；条件不成立时不注册，未产生可执行构建目标或测试结果。
# ## Constraints and lifecycle
# 上层 BUILD_TESTS 控制 tests 子目录是否配置，tests/CMakeLists.txt 显式 include 本文件。不能把变量非空等同于解释器已可执行，WindowsApps 别名也可能被 find_program 选中。
# -B 抑制导入字节码缓存；脚本以 mock 读取人工字符串，不生成宿主源文件。CTest 日志及其他运行产物仍应由调用方放在已登记的 E 盘构建/报告根。
# 没有 WORKING_DIRECTORY 覆盖、fixture 生成命令或 add_executable；通过脚本 __file__ 计算导入位置。超时或非零退出的结果由 CTest 判定，配置成功不证明四个 case 已通过。
# ## References
# tests/cmake/syntax-embedded-positions-tests.cmake:1
# tests/cmake/syntax-embedded-positions-tests.cmake:2
# tests/cmake/syntax-embedded-positions-tests.cmake:3
# tests/cmake/syntax-embedded-positions-tests.cmake:4
# tests/cmake/syntax-embedded-positions-tests.cmake:5
# tests/CMakeLists.txt:58
# tests/CMakeLists.txt:781
# CMakeLists.txt:183
# CMakeLists.txt:185
# tests/scripts/test_syntax_migration_embedded_positions.py:7
# tests/scripts/test_syntax_migration_embedded_positions.py:53
