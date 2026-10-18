---
related_code:
  - tests/library/test_native_numeric_boundaries.c
  - zr_vm_core/src/zr_vm_core/value/value.c
implementation_files:
  - tests/library/test_native_numeric_boundaries.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 全仓首方逐单元调用链审查与中文注释
tests:
  - zr_vm_native_numeric_boundaries_test
  - tests/cmake/native-numeric-boundary-tests.cmake
doc_type: review-batch
---

# Native 数值边界测试契约

本批逐项审查原测试20个单元：12函数、2类型、5状态与1宏，新增17段中文Doxygen，三个简单状态沿“无需注释”结论。9列台账逐项列定义、直接/间接调用方、入口、职责、约束、测试证据和判断，无未审查项。公开Core、Library、native lane、异常及allocator实现均为借用调用证据，不重复授信或修改实现。

Allocation和固定账表描述宿主实际块的地址、尺寸与内存类别，只有[0, liveCount)有效。释放会把末项搬入空位，记录索引/地址不稳定；账表不延长真实块生命周期。tracked_allocator是Core构造及释放的串行回调，重分配/释放必须带回对应元数据，唯一编号回调每次给出1不承诺线程安全。32768容量上限不是OOM注入；满表时新块已经取得，abort可跳过尾部正常清理。

数字读取通过原context/descriptor与下层value/native检查协作。8个selector分别覆盖int_controls、finite_range、nonfinite、boundaries、null_output、type_diagnostic、arity_diagnostic、float_controls；宏保留具体表达式短路和提前失败语义。进程级Windows错误模式及CRT设置不恢复，测试依赖每个selector独立进程。正常尾部释放Global并检查liveCount；早退、断言或非局部异常不都保证走同一清理。stopGc仅用于本夹具时序，不证明分配期间GC迁移完整安全或真实并发。

原源码完整字节前缀不变，仅EOF追加文档；程序token、原位置及Clang StartOfLine/LeadingSpace在实际strict raw-lexer中保持。101处EXPECT是静态源码计数，不是执行断言轨迹；测试未证明NaN payload、完整uint64域、全部Math回调、OOM恢复或Sanitizer结论。本轮不新增测试、断言、行为、API、BUG或TODO。

修改前/后分别在同一个已登记E盘Debug静态构建中运行原8个selector，16个原进程均exit0、无超时，8份stdout及8份stderr前后逐字节相同（原程序安静成功）。三个相关目标的共享基线共22进程、24 runner用例，两轮均通过；本模块只计8个原selector。共44份输出流和FilePath另13个相对夹具状态的配对用于交叉验证，不把其他目标用例算作Numeric覆盖。CTest登记只作入口证据，实际为直接EXE运行。

BEFORE与AFTER实际build进程均await0，同一graph/cache/config；AFTER的93项编号动作含84次C编译，不把979个已选图节点全称为本轮新编译。构建窗口4637项有限输入稳定；编译后其他会话出现10份源码变动（1份AOT测试不在selected图，9份Parser源/头可能在图内），故当前整工作树构建通过为false。运行33项guard只覆盖6份EXE/PDB、6份图/缓存/Ninja元数据、15项物理工具与6份本任务源码，夹具另验；不扩成完整宿主闭包。

原慢依赖采集器被终止exit1、原AFTER外层driver退出码unknown均保留；build/helper实际已await0，后续5次只读Ninja查询及原11份原始输出完成配对核验。BEFORE三个保守blocker由有界补充单独处理，两个零依赖VALID对象有C/ASM实际输入说明；316项外部SDK头仅在FIRST构建后冻结。进程timeout从Popen后communicate开始，不等于包括进程创建的绝对墙钟上限。不授原子快照、无ABA、Linux、GC压力或故障注入信用。

F3及其独审I3、SourceI、BEFORE/AFTER独审和原输出/配对证据冻结在E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/对应native-numeric-boundaries-tests-*、library-review-comments-current-r1-*和library-review-native-*-r2-*。当前global.h引用行213→214已按已提交头文件原文核实，不沿用旧行号。

本批新建native_numeric_boundaries_tests.tsv精确20行；其他台账原文保持。inventory只更新该既有文件的rawSHA、pending→reviewed与说明三格，其余工作树记录保留。提交还需真实暂存独审及三次cached whitespace检查。20项均为当前全叶去重后新键，历史Root分批数2811+20=2831不代表全仓覆盖率。必要E证据、可复用构建与缓存继续保留，手工释放0B、生命周期OPEN，全仓目标继续。
