---
doc_type: code-review-batch
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_vectorize.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h
plan_sources:
  - docs/code-review/comment-standard.md
  - docs/plans/ssa/09-language-simd/02-numeric-vector-ir.md
tests:
  - tests/parser/test_ssa_numeric_vector_ir.c
  - tests/parser/test_ssa_vectorize_pass.c
  - tests/cmake/ssa-tests.cmake
---

# ExecIR 数值语义与向量合法化公共契约审查

本批完整103物理单元：13个公开函数声明、12个枚举、4个结构、10个typedef别名、63个对象宏及1个include guard。102份中文Doxygen覆盖27个参数、9个非void返回；guard保留无需注释结论。九列台账为coverage/zr_vm_core_exec_ir_numeric_support.tsv，101项commented、1项TODO、1项no-comment，无未审查项或新增BUG。仅追加EOF注释，原16298B/382LF完整字节前缀及所有程序坐标保留；13个实现定义和完整函数体作为借用证据，不新增实现信用。

## 调用、表示及生命周期

181个标识符的795个现有程序命中分布在6个源码文件；全部13个实现定义及真实函数体已核，包含兼容对象宏的替换入口、聚合头曝光、Parser产品和两个独立测试目标。没有直接调用的公开名称保留仓外消费者未知结论。策略和语义记录的初始化、版本/已知位校验、求交、摘要与每个失败输出分别记录，哈希不提供唯一身份或完整相等证明。

VectorLegalizationRequest.policy为可空宿主借用指针，调用期间必须稳定可读，既不接管策略，也不证明真实数组extent。结果仅是声明性计划：已形成VECTOR方案才有length/lanes与length%lanes的向量次数和尾数，SCALAR_FALLBACK两字段均为0；两种已形成方案的scalarIterations均为完整length。Init结果及任意失败路径不由这条约束统一覆盖。诊断与result/effective写槽要求独立，不提供内部同步；失败后是否清零或保留候选状态按实际早退顺序区分。

Legalize的唯一TODO保留待核归约许可：当前requiredFastMath包含关系与preserveSourceOrder/STRICT_ORDERED判定，并不自动要求UNORDERED_REDUCTION位。核查入口为exec_ir_vector_legalize.c:330、341–348及test_ssa_numeric_vector_ir.c:252–259；现有正例提供该位，未覆盖“RELAXED且不保序但缺该位”的组合。只记录协议意图与实现边界，未将未复现意图风险升级为BUG，未改行为。

## 当前依赖及验证范围

原R2和完整103语义独审不可覆盖。SSA后续把30个Core构造声明迁至exec_ir_mutation_internal.h，未迁移本批13个Numeric声明；本批只借用当前私有头和真实source-layout include-root链。原1546个EOL行pin中的201个当前坐标经原字节逐行核对，5个whole-file身份及3个新增借用上下文分别登记；这些外部迁移不是本批源码写入或信用。Vector目标的private路径由tests/CMakeLists.txt:769及source-layout-private-consumers.cmake:66、69、70、73、74补入，不能只看局部target块推断目录缺失。

实际修改前在复用的Windows Clang19.1.5 Debug构建运行既有Numeric和Vector两个目标：构建exit0、CTest2/2、两个普通assert main直接exit0且四个完整stdout/stderr流均为空。Numeric main静态调用9个有限场景，Vector main调用4个规划fixture；实际生成命令无NDEBUG，断言中的构造副作用得以执行。无Unity/footer或逐case trace，不以空日志推断全分支，也不声称真实SIMD算术、反向内存访问或完整IR保持；失败assert可跳过清理。每次有限观察域的源哈希、20项cache与argv记录在E盘证据；不代表全SDK、实际全部编译依赖闭包、完整程序/配置、原子隔离checkout、staged tree、全套测试、Linux或默认SDK。

修改后Clang stdin raw lexer两次exit0，1203个非注释token的kind/spelling/StartOfLine/LeadingSpace及原始行列一致，完整消费词法输出；严格证据6b00a65ac5ec5eec86a822a876116e5c145cdd31a11f96a12a04b4763012dc13。AFTER构建exit0、CTest2/2、Numeric/Vector直接程序均exit0，两程序完整stdout/stderr均为空且与BEFORE逐字节相等；4条argv及20项cache相同。BEFORE24frozen/3590observed、AFTER26frozen/3591observed，各自有限路径起止稳定；共同3590路径跨运行仅本NumericH102份EOF注释变化。增加冻结的layoutCMake已在原BEFORE观察域且哈希相同；boundary-registrationCMake为新增的1个当前观察路径，不回填其历史wholeSHA。AFTER摘要a177d9fb6b7c72ba68ca31c5e0f8e77afdeb49bcd7cb18b3f3ada7516cb92f00保留在E盘。实际source/leaf和暂存独审仍须核对当前内容，不以运行冒称staged tree验证。既有未使用编译flag及CMake长路径警告保留，未新增测试/源码副本或修改API、断言、构建配置。
