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

# @unit benchmark_compare_viewer_python_registration
# ## Summary
# 在 tests 目录配置阶段，把实际 HTML Viewer 的 Python/Node 边界回归注册为一个 CTest 测试。
# 先要求上层提供可用的 Python 路径，再搜索或复用 Node；两个变量均为真时才发布测试及其属性。
# 此块不执行 Python/Node，不创建编译目标；测试发现与实际解释器启动是不同阶段。
# ## Parameters
# 本单元是配置块，没有函数形参。ZR_VM_BENCHMARK_PYTHON_EXECUTABLE 由上层 find_program 或调用方配置提供。
# ZR_VM_BENCHMARK_VIEWER_NODE 是本块 find_program 的结果/缓存变量，名称候选为 node、nodejs。
# CMAKE_CURRENT_SOURCE_DIR 必须仍指仓库 tests 目录；include 不把它改为本片段所在的 cmake 子目录。
# 这些配置值须指向可信、可启动且兼容测试脚本的工具；条件为真不等于版本或可执行性验证。
# ## Returns
# 没有函数返回值。满足双门槛时，登记 benchmark_compare_viewer_python，并把 Node 路径、UTF-8 输出编码和总超时交给 CTest。
# Python 变量为假时不搜索 Node；Node 变量为假时不注册测试，不能把这种缺席解释为一次跳过或通过。
# 后续测试命令的非零退出、启动失败和超时由 CTest 报告，本配置块不包装或消除这些失败。
# ## Constraints and lifecycle
# 父目录一次 include 此片段，并已 enable_testing；路径和测试名应在该目录作用域保持有效。
# find_program 默认持久化搜索结果，已有可复用的普通变量/缓存值会影响是否重新搜索；本块不重置陈旧路径。
# 未找到程序的 NOTFOUND 值使门槛为假，后续配置可重新搜索；切换工具须由配置调用方管理相应构建目录的缓存。
# Python 命令带 -B，禁止该解释器写入字节码缓存，但不禁止读取已有缓存，也不封闭其他文件、导入或子进程 I/O。
# ENVIRONMENT 仅为此测试设置 BENCHMARK_VIEWER_NODE 和 PYTHONIOENCODING；未列出的宿主环境仍由测试进程继承。
# 工具路径须能按 CMake 命令参数及环境列表表示；该登记不是任意不可信路径或宿主环境的认证边界。
# CTest 的 120 秒约束整个 Python 测试进程；page() 对每次 Node 调用另设 10 秒，VM 初始每段脚本另设 2000 毫秒。
# VM 限时不包住后续宿主派发的文件回调，三个限时不可互相替代；此片段也不提供跨进程的锁或资源隔离。
# 脚本按 __file__ 定位手写 HTML/JS，以内存 JSON 注入场景；本块不创建或清理测试夹具，不改变其有限浏览器模型。
# ## References
# - CMakeLists.txt:9
# - CMakeLists.txt:185
# - tests/CMakeLists.txt:58
# - tests/CMakeLists.txt:807
# - tests/cmake/benchmark-viewer-tests.cmake:1
# - tests/cmake/benchmark-viewer-tests.cmake:2
# - tests/cmake/benchmark-viewer-tests.cmake:4
# - tests/cmake/benchmark-viewer-tests.cmake:8
# - tests/cmake/benchmark-viewer-tests.cmake:9
# - tests/benchmarks/test_benchmark_compare_viewer.py:48
# - tests/benchmarks/test_benchmark_compare_viewer.py:57
# - tests/benchmarks/test_benchmark_compare_viewer.py:61
# - tests/benchmarks/test_benchmark_compare_viewer.py:133
# - tests/benchmarks/benchmark_compare_viewer_harness.js:34
# - tests/benchmarks/benchmark_compare_viewer_harness.js:45
