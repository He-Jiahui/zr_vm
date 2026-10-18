---
related_code:
  - tests/core/test_execution_integer_boundaries.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
implementation_files:
  - tests/core/test_execution_integer_boundaries.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 全仓首方逐单元调用链审查与中文注释
tests:
  - zr_vm_execution_integer_boundaries_test
  - execution_integer_boundaries
  - tests/cmake/runtime-compiler-boundary-tests.cmake
doc_type: review-batch
---

# 解释器整数边界测试契约

本批逐项审查原测试13个物理单元：8函数、2类型、1宏及2个文件内静态表。11段中文Doxygen覆盖职责、使用时机、参数/返回、packed输入不变量、失败状态和资源边界；setUp/tearDown空钩子分别记录“无需注释”理由。定义、调用方、Unity入口、逐项契约与测试证据见execution_integer_boundaries_tests.tsv，无未审查项。依赖的Core执行器、host fixture与Unity只作借用证据，不重复授信。

OperandForm的7值是测试私有布局选择器，不是VM opcode或持久化协议；必须与ArithmeticForm.opcode配对。FORM token paste构造有限表共31项（加14、减11及其他形式），宏不验证任意布局组合。operand0/operand1是重叠视图，对应packed handler按各自约定解释。check每次新建VM与4条instruction、3个constant和6槽的函数夹具；LOAD_STACK_CONSTANT与LOAD_STACK_LOAD_CONSTANT这两种形式先把左输入实体化到slot3；不能把本表扩成所有opcode或所有operand布局已验证。

check通过原DefaultGlobal设置宿主回调，再由Execute运行；唯一编号12345不承诺线程安全。预期溢出失败走线程错误及ThreadReset路径，成功结果按inline Int64读取。正常顺序是Function_Free先于GlobalDestroy；前者清host root、处理owning GC tombstone，不等于手工释放任意GC对象。销毁后读取本测试的inline Int64不表示一般对象结果也能跨VM生命周期保留。Unity断言、OOM或Throw可能绕过局部正常尾部；空tearDown不补做资源清理。

test_checked_add_and_subtract_variants覆盖指定加/减形式的边界，test_checked_negation覆盖指定负号形式，test_masked_shifts核有限mask计数。241为源码条件完成时的静态check调用计数（225加/减、4负号、12移位），其中152预期失败、89预期成功；它不是241项Unity测试或实测断言轨迹。expected arithmetic error输出属于原负例，不记为失败，也不泛化为完整算术域证明。

原文件原始CRLF前缀6653B/136行逐字保持，EOF追加11段文档，原函数、宏、注册、指令、断言和程序token坐标均未改。实际Clang raw-token种类/拼写/StartOfLine/LeadingSpace及原坐标通过；本轮不改行为或API、不新增测试、BUG或TODO。F1文档中“旧B无此target/运行待填”是准备时状态，已用本批新鲜构建与原用例结果替换。

修改前和修改后分别实际构建zr_vm_execution_integer_boundaries_test，两个build进程均await0。同一Debug静态配置的原EXE各运行一次，每次Unity3用例全部PASS；stdout/stderr逐字节相同。stderr原预期overflow内容17724B/456行，其中Error行为84 add、66 sub、2 neg。三个目标共享验证共22进程、24 runner用例；本模块只计这一个进程的3个原Unity用例。没有以241静态调用数代替执行证据。CTest execution_integer_boundaries登记是入口证据，实际为直接执行原EXE。

共享构建窗口4637项有限输入稳定，AFTER的93项编号动作含84次C编译；不把979个图节点全称为新编译。其后其他会话修改10份源码（AOT测试1不在selected图，另9份Parser源/头可能在图内），不授当前整工作树构建通过信用。运行33项guard仅6份EXE/PDB、6份图/缓存/Ninja元数据、15项工具和6份本任务源码，FilePath13个相对夹具状态另行校验。全组44份前后输出流及13个夹具状态均一致。

BEFORE旧三个保守blocker及有界补充、两个零依赖VALID对象、316项SDK头只在FIRST构建后冻结的限制保留。原慢依赖collector被终止exit1，后续5次只读Ninja查询与原11份输出成功核验；原AFTER外层driver退出码unknown，build/helper实际await0，不补造历史退出码。timeout从Popen后的communicate开始，不保证包含创建耗时的墙钟上限。不授完整宿主闭包、无ABA、原子快照、GC压力、OOM注入、Sanitizer或跨平台验证信用。

冻结F1/I1、源应用/strict lexer/SourceI、原BEFORE/AFTER输出和独审在E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/对应execution-integer-boundaries-tests-*、library-review-comments-current-r1-*、library-review-native-*-r2-*。本批新建13行叶台账，并在inventory追加一个当前缺失的首方测试文件记录；其他行和叶台账保持。真实暂存独审与三个cached whitespace检查通过后提交。13个全叶去重新键令历史Root分批数2831+13=2844，不代表全仓覆盖率；必要E证据/构建/缓存/夹具保留，手工释放0B、生命周期OPEN，全仓目标继续。
