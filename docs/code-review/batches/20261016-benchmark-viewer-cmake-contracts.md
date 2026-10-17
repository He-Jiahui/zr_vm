---
related_code:
  - tests/cmake/benchmark-viewer-tests.cmake
  - tests/CMakeLists.txt
  - tests/benchmarks/test_benchmark_compare_viewer.py
  - tests/benchmarks/benchmark_compare_viewer_harness.js
implementation_files:
  - tests/cmake/benchmark-viewer-tests.cmake
plan_sources:
  - user: first-party caller-first contract review
  - docs/code-review/comment-standard.md
tests:
  - tests/benchmarks/test_benchmark_compare_viewer.py
doc_type: review-batch
---

# Benchmark Viewer CTest 登记契约

本批完整审查一个登记配置块：两层工具门槛、Node缓存查找、单一add_test及环境/超时属性，共7个CMake内置命令、39个有限词法token。原11LF/588B逐字保留，仅在EOF追加一份40行中文文档及分隔空行，现52LF/3952B。原token种类、拼写和字节/行列位置相同；该有限比较不宣称通用CMake AST或配置执行。

父tests/CMakeLists.txt:58查找Python，807行include片段；include沿用tests目录的CMAKE_CURRENT_SOURCE_DIR。两个工具变量均为真时才登记benchmark_compare_viewer_python；缺工具时登记缺席，不能称测试跳过或通过。find_program可复用普通变量/构建缓存值，不验证版本或启动能力，也不清理陈旧工具路径。配置调用方承担工具信任及缓存生命周期。

Python -B禁止该解释器写字节码，仍可读取缓存及进行其他I/O。ENVIRONMENT只设置Node路径与UTF-8编码，其余环境继续继承。CTest120秒覆盖整个Python进程；page()每次Node另限10秒，VM初始每段脚本另限2000毫秒，后续宿主文件回调不受那次VM限时。三个限时的范围不同。实际测试按__file__读取手写HTML/JS并注入内存场景；配置块不创建或清理夹具，也不提供并发锁。

明确复用Harness批次实际AFTER9作为本批修改前基线，新增BEFORE运行数为0。本轮采用完全相同的2447B控制器、两步argv、6个显式环境项和配置PATH前缀，使用物理E Python及物理Node v22.22.3；Node --check exit0，原9个unittest方法9通过、0失败、0错误、0跳过。完整结果JSON及stdout与基线相同，stderr仅一条耗时行不同，Node语法检查两条原始流相同。Root46872/helper47240/test52968/Node-check50056均已等待exit0。

历史12项有限输入仅本CMake纯注释字节变化；本轮另外登记4项保守输入，共14份仓库文件和2个物理工具，运行两端16项hash均保持。额外4项不倒称历史闭包。缓存前缀仍不存在，未生成测试夹具或网页。有限输入及显式环境比较不证明完整PATH、其他继承环境、系统共享库、原子快照或无ABA。

复用B中的生成CTest仍记录旧工具别名；本轮直接运行现有测试，不调用该别名、不刷新Macro/SSA配置图，也不宣称fresh CTest登记或执行、C构建、Linux、真实浏览器或安全沙箱验证。Python原测试创建并等待Node子进程；没有为记录子PID而改写方法、helper、断言或钩子。

新benchmark_viewer_cmake.tsv记录本配置块全部定义、父入口、调用与约束，选定1行check_batch无格式问题。其余497份台账保持lease字节；inventory只更新本文件已有pending行的rawSHA、reviewed状态和说明三格，其他工作树差异保留。1新规范键的交付须待独立实际源审查、实际暂存复核及git diff --cached --check通过；Root历史分批数此前2795，提交后为2796，不是全仓去重覆盖率。Python Viewer24与JS Harness23仅借用，不重复授信。

F1/I1、Rootlease、严格比较、基线adoption、原始运行流及逐项凭据保存在E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/benchmark-viewer-cmake-*。未改公开签名、命令、环境属性、程序行为、测试或fixture，未新增BUG/TODO。必要E证据与复用产物保留，手工释放0B，生命周期保持OPEN，全仓目标继续。
