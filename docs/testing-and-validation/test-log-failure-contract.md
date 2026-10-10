---
related_code:
  - tests/cmake/verify_test_log_failure.cmake
  - tests/CMakeLists.txt
  - tests/harness/test_zr_test_log_failure_probe.c
  - tests/harness/zr_test_log_macros.h
  - tests/harness/runtime_support.h
  - tests/test_support.h
  - tests/parser/test_compiler_features.c
  - tests/parser/test_compiler_regressions.c
  - tests/system/test_system_fs_module.c
  - tests/parser/test_parser.c
  - tests/parser/test_semir_pipeline.c
  - tests/compileTime/test_compile_time_execution.c
  - tests/parser/test_compiler_w2_performance_quickening.c
  - tests/parser/test_compiler_w2_quickening_array_add.inc
  - tests/container/test_container_metadata.c
  - tests/language_server/test_lsp_logical_semantic_query.c
  - tests/parser/test_const_keyword.c
  - tests/third_party/zr_unity/Unity/src/unity.c
implementation_files:
  - tests/cmake/verify_test_log_failure.cmake
  - tests/harness/zr_test_log_macros.h
  - tests/harness/test_zr_test_log_failure_probe.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 全仓库调用优先的职责、使用限制与设计目的注释审查
tests:
  - tests/harness/test_zr_test_log_failure_probe.c
doc_type: module-detail
---

# 测试日志故意失败探针契约

## 入口与调用方法

tests/CMakeLists.txt:83 创建 zr_vm_test_log_failure_probe，tests/CMakeLists.txt:88 选择仓库 Unity 配置，tests/CMakeLists.txt:89 链接 Unity。tests/CMakeLists.txt:93 使用 TARGET_FILE 传递当前配置的程序路径，tests/CMakeLists.txt:94 以 cmake -P 调用脚本。构建该目标后，可由 ctest -R '^test_log_failure_contract$' --output-on-failure 选择这一个 CTest 项；直接调用需提供 -DPROBE_EXE=<已构建路径>。

脚本的必填门禁位于 tests/cmake/verify_test_log_failure.cmake:21，仅验证路径变量定义且非空；不负责构建程序或预先证明文件存在。后续 tests/cmake/verify_test_log_failure.cmake:29 启动程序并获取实际结果。当前探针只登记 tests/harness/test_zr_test_log_failure_probe.c:101 的一个 RUN_TEST。

## 为什么子进程返回 1 仍可使 CTest 成功

tests/harness/zr_test_log_macros.h:153 设置 Unity.CurrentTestFailed，宏不走 TEST_ABORT。探针在 tests/harness/test_zr_test_log_failure_probe.c:77 故意失败后继续执行 tests/harness/test_zr_test_log_failure_probe.c:78 的赋值。tests/harness/test_zr_test_log_failure_probe.c:102 获取 UNITY_END 汇总值，tests/harness/test_zr_test_log_failure_probe.c:104 返回这个值。脚本用 tests/cmake/verify_test_log_failure.cmake:39 检查故意失败的预期退出 1，再分别检查 tests/cmake/verify_test_log_failure.cmake:47 的摘要和 tests/cmake/verify_test_log_failure.cmake:52 的尾部标记；三项联合满足后外层 CMake 成功。

一次失败指当前一个用例的 Unity 记账。该探针不能证明重复调用失败宏、多用例计数、并发失败状态或异常处理的行为。来自 tests/cmake/verify_test_log_failure.cmake:36 的双流拼接只用于模式匹配和失败诊断，不能推断 stdout 与 stderr 的实时先后顺序。

## 清理标记的证明边界

cleanup_reached 只是在失败宏之后赋值。当前探针没有资源取得与释放，setUp 和 tearDown 为空；标记及 UNITY_END 后的打印证明控制流继续，不证明资源释放、无泄漏或崩溃后的清理。当前候选把宏头、探针及脚本合并审查，按实际调用和 Unity 派发校正台账；词法清单不授予其余调用函数的完整语义审查信用。

## 待核实的限制

TODO: tests/cmake/verify_test_log_failure.cmake:28 未提供子进程 TIMEOUT。核对 tests/CMakeLists.txt:90 的注册入口及直接 cmake -P 用法需要的执行期限，再确定由 CTest 还是脚本管理挂起子进程；本审查没有复现当前探针挂起。

TODO: tests/cmake/verify_test_log_failure.cmake:47 使用子串匹配。若未来在 tests/harness/test_zr_test_log_failure_probe.c:101 增加用例，核对摘要的整行和数字边界，避免 11 Tests 中的 1 Tests 通过原条件。当前单用例配置不支持将该未来风险标为 BUG。

## 验证范围

CMake 候选替换原首行的歧义注释，保存其他 27 条原始物理行及 LF；宏头和探针只替换列明的旧注释并插入新注释，其他原始行、空行和逐行 EOL 保持一致。静态审查与补丁检查不产生构建、运行或故障注入信用。实际编译、脚本执行及修改前后比较必须使用各自的当前源码和终态收据。


## 公共日志接口的调用契约

ZR_TEST_START 输出标题，ZR_TEST_INFO 说明验证主题；两者不采样时钟或执行断言。ZR_TEST_PASS 输出调用者提供的耗时和 Pass 文本，不清除 Unity 失败，也不是单独的通过判定。ZR_TEST_DIVIDER 和 ZR_TEST_MODULE_DIVIDER 分别为用例和外层分组提供诊断边界，Unity 的开始、派发和结束仍由 runner 组织。

字符串在同步输出期间有效且非 NULL，宏不保存或取得其所有权。timer 可为 SZrTestTimer 或具有兼容 startTime/endTime 字段的值记录；宏分别读取两个字段，因此表达式应稳定且无副作用，并避免与局部 zrTestElapsed 标识符冲突。调用者决定采样边界；宏不取得 VM 或其他资源，也不会代为清理。stdout 刷新没有错误反馈，不能保证并发分组、跨流时序或崩溃前消息全部可见。

tests/harness/zr_test_log_macros.h:153 只写当前用例失败标记。UnityDefaultTestRun 在 tests/third_party/zr_unity/Unity/src/unity.c:2298 调用 setUp，在 :2299 派发用例，在 :2303 调用 tearDown，并在 :2306 调用 UnityConcludeTest。后者在 :574 优先处理忽略状态，未忽略失败才在 :585 增加失败数；最终 UnityEnd 在 :2359 返回汇总。这些是第三方只读支持事实，不表示已审完 Unity 库。

## 时钟口径与调用者责任

SZrTestTimer 的旧 CPU 时间说明不能覆盖全部平台。MSVC CRT 的 clock() 记录自 CRT 初始化以来的墙上经过时间，与 ISO C 的处理器时间口径不同；不可取得或不可表示时返回 (clock_t)-1。因此 CLOCKS_PER_SEC 换算后的 ms 仅用于该平台的片段诊断，不能直接当成跨平台性能指标或 CTest 超时依据。[Microsoft clock 文档](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/clock?view=msvc-170)。

Root 另只读核对本机安装的 UCRT clock.cpp，它用 QueryPerformanceCounter 差值及范围检查实现此口径；本机源文件并不证明运行 DLL 与该源逐字同版。该文件 SHA256 bfc72602c4bb362064d29897ffd7f114d6b6c040b984fb16f3ba65bc20329e8c；安装目录仅作为私人证据，不放入正式仓库数字锚点。

TODO: 检查实际 clock() 调用失败或样本差不可表示时各调用者应如何报告无效计时。当前类型和输出格式没有有效性状态，入口为 tests/harness/test_zr_test_log_failure_probe.c:74 的采样、tests/harness/zr_test_log_macros.h:120 和 tests/harness/zr_test_log_macros.h:150 的差值；本探针两端相等只是避免未初始化字段，未覆盖这些异常。

## 调用身份与仍需扫描的范围

当前 C-family 词法扫描覆盖 2917 个首方源文件，发现 23 个消费文件、2021 次直接或别名宏调用，并区分 3 组局部宏别名；这是搜索清单，不是 2021 个调用的完整人工语义验收。test_parser.c 的 TEST_FAIL_CUSTOM 先补终点再调用公共失败宏；test_semir_pipeline.c 的 TEST_* 是公共宏别名；compileTime 的局部分隔线 wrapper 同样转接公共宏。

test_compiler_w2_quickening_array_add.inc 不自行包含头文件，由 tests/parser/test_compiler_w2_performance_quickening.c:994 包含，其宿主已直接包含公共日志头。同名 SZrTestTimer 在 container 和 language_server 文件中有各自的本地定义，不据此算作公共类型消费者。test_const_keyword.c 的 ZR_TEST_FAIL_UNITY/STATE 是另一个会中断的接口，不能套用公共 ZR_TEST_FAIL 的继续执行说明。

TODO: 按私有 all-call-sites.tsv 逐个补齐其余调用函数的完整控制流、时钟失效与资源退出审查；当前 Root 阅读了各文件的代表调用和部分失败窗口，截断或未显示的窗口没有阅读信用。宏接口候选与词法清单不能把这些大型调用文件推进为 reviewed。

## 候选范围与实际执行证据

本候选三个源文件共有 25 个定义或接口单元：CMake 八项、头文件十一项、探针六项。tests_cmake_harness.tsv 替换旧脚本一行，另十五条数据行原字节保持；tests_harness.tsv 将旧十二条宏/探针记录展开十七条，另183条数据行原字节保持。导航 index 存在外来修改，不在候选中。

修改前的真实有限证据仍是 comments-test-log-failure-before-r1：当前两份 C 源手动编译、链接，并执行原脚本，四步自然退出0；脚本内部的探针预期失败1。这不是整套 CMake 构建、CTest 运行、计时错误注入或候选修改后的验证。新范围同时涉及宏头、探针和脚本，原仅允许脚本变更的 after wrapper 不适用；必须派生匹配三条源路径的版本，再经独立审查和实际应用后执行。
