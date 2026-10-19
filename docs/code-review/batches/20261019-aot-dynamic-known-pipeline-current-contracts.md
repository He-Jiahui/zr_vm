---
related_code:
  - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c
  - zr_vm_aot/tests/parser/test_known_call_pipeline.c
implementation_files:
  - zr_vm_aot/tests/parser/test_dynamic_iteration_pipeline.c
  - zr_vm_aot/tests/parser/test_known_call_pipeline.c
tests:
  - zr_vm_dynamic_iteration_pipeline_test
  - zr_vm_known_call_pipeline_test
plan_sources:
  - user: 逐函数、类型、人工夹具及关键块审查当前调用与合同
doc_type: module-detail
---

# 动态迭代与已知调用流水线测试合同

本模块的物理范围是两个测试 C 文件。它们在当前 CMake 中仍由 parser/core 条件注册，不因位于 AOT 目录而停用。相邻的 `test_execbc_aot_pipeline.c` 只用于当前注册及同名静态 helper 交叉检索，不包含在本批全文审查中。

台账范围为 50 个旧 C 单元：43 个函数体、7 个本地类型及其 32 条成员声明。另单列 4 个语言夹具单元：`makeValues` 函数、`Counter` 类和两个成员方法。四个 case 数组的数据约束随所属测试记录，29 行数据不另外计成 Unity 用例。50 个旧三列键保留；4 个语言完整键在写入前的510叶43975键中均未命中，本批首次交付4个定义信用。

## 入口和执行边界

`zr_vm_dynamic_iteration_pipeline_test` 的 main 顺序运行两个用例。第一个验证动态 foreach 的 ExecBC 融合指令、当前 SemIR 的 opcode/deoptId 和 .zri 文本，再通过 VM 解释执行求和得到 6。第二个只生成默认 C/LLVM 文本并搜索迭代运行时片段；它不编译或执行这两个生成物。

`zr_vm_known_call_pipeline_test` 顺序运行五个用例。VM 来源表七行与 native 来源表六行只做浅层 quickening 后的 opcode 断言。运行期表八行直接构造已知调用 opcode，再解释执行得到 41、73 或 91。Counter 源码用例核对成员调用族并执行得到 14。最后八行矩阵生成 .zri 和严格 C/LLVM 文本，检查对应 opcode 和 unsupported 片段；它同样不编译或执行生成物。

两个 native 回调写入的 73/91 是结果槽中的整数，C 返回值 1 表示结果个数。参数、尾调用和零参组合仅限当前表，不表示任意调用程序覆盖。现有七个 Unity 用例与数据表行数分别报告，不能混算。

## 数组、VM 和文件的寿命

运行期 builder 分配根函数和可选的 VM 叶子，正常路径分别释放。quickening 测试将 heap 数组挂接到本地函数记录，再由根 Function_Free 收尾。后端矩阵把测试栈上的数组借给手工函数，不能把这组数组交给 Function_Free。GC 可追踪的值包装不建立显式 ownership control；borrowed 包装则明确关闭 GC 追踪，均要求对象在消费者结束前有效。

文本读取 helper 在内部失败分支关闭 FILE 并释放自己的缓冲；成功缓冲归调用测试 free。两个程序显式涉及 27 个相对文本路径：动态用例三个，已知调用八组各 .zri/.c/.ll 三个。它们受 cwd 影响，TEMP 不会迁移这些文件。普通成功路径尝试 remove；多数 remove 返回值没有断言，不能据进程正常退出宣称文件必已移除。后续运行需 Root 预登记独占 E/tests cwd、完整记录初末目录及残留，并保留未知文件。

## 已证缺陷和保留限制

BUG `AOTPIPE-CLEANUP-01` 是未运行复现的静态完整路径结论：有效 state/fixture 已建立、writer 的 prototype 校验通过后，若相对 .zri 目标不可打开，writer 普通返回 FALSE；Unity 断言通过默认 longjmp 离开测试体，空 tearDown 不销毁局部 VM。动态用例的 Function_Free/State_Destroy 和已知调用后端用例的 State_Destroy 因而被跳过。结论限于同进程后续用例期间资源及注册节点遗留，不声称这一触发已创建文本缓冲或文件，也不称操作系统进程退出后的永久泄漏。本批只注释，不修复、不增加故障用例。

TODO `AOTPIPE-AOT-EXEC-01` 保留原四行的同一证据缺口：unsupported 子串缺席、requireExecutableLowering 返回真或 runtime helper 符号出现，均不能替代生成物的编译和实际执行。解释执行获得的 6/14/41/73/91 也不能转记为 AOT 本机运行信用。

## 注释等价与现有验证

本批只更正两条独立注释行，并在 EOF 添加完整中文文档。动态 SemIR helper 不搜索独立 deopt 备份；函数值包装的 ownershipKind 实为 NONE。原程序原子、字符串、行号及 UTF-8 列保持；这属于静态文本核验，不是 C 编译器 AST 或词法等价证明。

本批已完成同一实际构建和两个既有入口的修改前后验证：

前后两次构建均退出0；直接运行dynamic2项、known5项，每阶段7PASS/0FAIL/0IGNORE，有序原始Unity结果和footer逐项相同。stdout只有2+5个clock数值、共7行变化，精确计时字段归一后全字节相同，stderr逐字节相同；不作性能结论。两阶段起始cwd已证为空，结束目录均实际为空，27个已登记相对生成物无残留。该普通成功验证没有触发静态fopen/Unity清理BUG。

实际配对报告aot-parser-pipeline-tests-current-native-pair-r1.json（SHA256 a04525104d2cfcf1c6fc3a33c80daccca63f9806f6c805e49a7be2e007b68ad8）；原始before e057667df071cb1383e4b1ff1182a13fb723dae126902d870bc872b3c6802f51、after bffa40720213e80658785d9c23b08326e97ea8c87ff30e30ef40d0bfa58b8739。源码应用7abbe83c65c31d799e059360bacd56eca31ca7e292b4ce986049975020599a37。

现有构建目标是上述两个 Unity executable，物理 EXE 由本次 FileAPI 映射。实际沿同一物理E盘B串行build后直接运行两个无参数入口，保留了stdout/stderr、真实退出、有序PASS行及2/5用例footer。120 秒只可作为本任务 direct watchdog；现有 language_pipeline 聚合 runner 的 execute_process 未设置 TIMEOUT。clock 输出为诊断动态字段，原始流须完整保留。测试失败后未到达的断言不授 PASS。

## 台账与后续维护

现有混合叶 `zr_vm_aot_tests_runtime.tsv` 中仅选择 50 行，header 和其他 825 行保持原始字节。本批增加4个语言单元，inventory只更新两个所属路径的状态/哈希/说明格并保留旧说明前缀；静态独审已接受，源码应用与前后真实验证完成，提交前另复核实际暂存差异。现有需求页中仅把本dynamic的旧flat路径更正到实际位置，历史批次和历史测试数字不改。

本批新注释的BUG:/TODO:仅补6个ASCII冒号以符合用户标签规范，原F/I不重写，语义及原1407行程序/字符串的拼写、坐标和EOL不变。工作树dynamic保持CRLF，Git暂存按已有属性转LF；inventory记录WT字节哈希，源应用和暂存另针LF哈希。

TODO的具体后续核查入口为tests/cmake/aot-generated-targets.cmake:12、:142注册的既有shared-library smoke源；还须审其执行断言能否覆盖本动态迭代和known-call矩阵。本批未运行这些入口、不借其名称授予生成C/LLVM执行信用。三个TODO决策行及另一BUG行内的同族TODO共同保留该缺口。

混合叶54选中行之外825行及表头逐字节保留；inventory外来49行86格保持。全局check前后同一既有stale（均1），不宣称全仓验收。完整879行check-batch的实际结果及选中54行归属判断另记checks；它只验证格式、键及非空行锚，调用语义由F/I、Root和独立暂存复核承担。
