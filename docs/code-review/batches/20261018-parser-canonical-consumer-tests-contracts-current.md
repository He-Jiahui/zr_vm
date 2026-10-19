---
related_code:
  - tests/parser/test_canonical_consumers.c
  - tests/parser/test_reference_callable_consumers.c
implementation_files:
  - tests/parser/test_canonical_consumers.c
  - tests/parser/test_reference_callable_consumers.c
test_files:
  - tests/parser/test_canonical_consumers.c
  - tests/parser/test_reference_callable_consumers.c
plan_sources:
  - "user: 全量审查、按需中文注释；公开 API 与程序行为保持不变；按功能模块提交。"
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_canonical_consumers.c
  - tests/parser/test_reference_callable_consumers.c
doc_type: review-batch
---

# Canonical consumer 与引用 callable 测试契约

## 审查单元及功能边界

本批以 `main` 的 `28e8dc59c9960578ce0da19e04119ec6717fcf31` 及其实际工作树为基线，审查两个源文件的123个单元。整仓键去重确认本批旧键0、新键123；语言字符串中的定义按字符串作用域命名，Unity hooks、注册和函数指针入口逐项追踪，语句块随所属单元审查。

九列台账分为 [主测试107项](../coverage/tests_parser_canonical_consumers.tsv) 和 [引用测试16项](../coverage/tests_parser_reference_callable_consumers.tsv)。这是同一 `zr_vm_canonical_consumers_test` 目标的两个翻译单元，合为一次功能模块交付；foundation27、source contract80、reference16 三个语义分组仍可逐项追溯。123项中103项为已注释、18项无需新增注释、2项为同一已证实问题的函数镜像，无未审查项及新增 TODO 家族。

70个 C 物理单元包含31个函数、4个前置声明、1个结构、15个固定对象宏与19个状态；53个人工 Zr 夹具单元包含26个函数/构造器/lambda body、1个无 body extern 接口、14个类型和12个值/别名状态。语言定义分别按所属 source 字符串标识作用域，同名符号不合并计数。逐单元定义、调用点、Unity函数指针入口、测试断言、约束和不加注释理由均见台账。

## 上下层协作及使用契约

合成 fixture 通过 artifact writer 在调用者缓冲区中写入固定节与行，随后 VM 和 AOT metadata 消费入口读取同一 canonical 身份、签名、布局及可选 domain-transfer/scheduler 合同。测试使用固定 token、hash、pointer-size layout 和单字节 code 占位；这些常量服务于此夹具，不是通用 artifact ABI。Debug/reflection 查询也只在该内存投影中解析元数据。

源码消费者测试 Parse/Compile 人工字符串，再查询 canonical type、resolved target、receiver effect、泛型关闭和签名往返。26个语言 body 没有经 `Function_Execute` 执行；extern `NativeAdd` 只有声明，没有原生动态库调用。AOT 目录中的相关实现参与本目标的当前构建，不能从历史目录说明推断停用；本批运行证据限于 metadata 消费入口。

`SZrConsumerArtifactFixture` 内部节视图含指向本对象的指针，初始化后序列化前须维持原地址及全部行有效，不可裸搬移后继续使用旧内指针。writer 同步借用栈对象和静态 code，输出缓冲由调用者持有；这些 helper 不保存栈指针，也不负责文件 I/O。

## 资源、失败和时序边界

共享 `g_state` 由 Unity 的 `setUp/tearDown` 顺序管理，仅覆盖该槽；reference 两个测试另建局部 VM。查询结果及 AST 节点均为借用，须在 AST/compiler state/semantic context 释放前使用。普通尾路径显式释放编译函数载荷、compiler state、AST 或局部 context/state；Unity 断言的非局部退出不会执行这些普通尾语句。

`consumer_release_compiler_function` 按 topLevel/current 是否别名避免重复释放载荷；其单分支不保证同时清空两个别名槽，不应视为可重入的完整 compiler-state reset。随后仍按用例的实际顺序清理 compiler state 与 AST。测试夹具只按本 binary 顺序使用，不提供并行访问同步或跨线程生命周期保证。

BUG 家族 `CANONICAL-CONSUMER-LOCAL-VM-UNITY-ABORT` 对应 CCONS-067 与 CCONS-070：局部 VM 创建成功后，`SemanticContext_New` 第一次 RawMalloc 普通返回 NULL，失败断言经 Unity longjmp 绕过局部 `State_Destroy`；默认 hook 仅销毁 `g_state`，该独立 VM 的清理与注销链可延后至进程结束。证据从 reference 源第195–196/368–369行，经 semantic.c 第195行、RawMalloc 包装和 Unity abort 路径追到默认 hook；详细定义和引用片段见台账。此结论来自完整静态失败路径，不声称进行了 OOM 注入，也不外推到每个断言；本轮仅注释记录。

## 源码与文档保持性

105段中文 Doxygen 以完整 EOF 注释块加入（main98、reference7），使用 Summary、Parameters、Returns、Constraints and lifecycle、References 等适用段落及显式符号引用。原始两个源文件前缀、字符串、原有行号和字节偏移均保持，公开签名和程序行为不改。18项无需新增注释的固定身份宏、code占位及期望字符串已有明确作用，理由逐项保留。

主测试原文件已超过千行。本批保留现有两个翻译单元及21用例共享 hooks，新增内容只有文档，没有新增运行职责；因此不在注释任务中迁移测试布局。后续若继续增加实现，优先沿 artifact fixture/foundation 与源码 metadata 消费测试边界拆分；该说明不是新的实现 TODO。

静态原报告的43处整文件依赖在当前基线中42处原针相同，`zr_vm_parser/include/zr_vm_parser/compiler.h` 多了其他会话的93字节 `isDeclarationShell` 字段行。当前桥接验证精确逆向后恢复旧文件；所有2734处引用行的原始 UTF-8、EOL、行号及绝对字节偏移仍精确。该字段及其工作树归属由当前两阶段共同基线承接。109项可检索调用证据按规范化路径、行号及完整文本的多重集核对，保留原始 `rg` 输出顺序的观测差异。

目标实际闭包包含988个 C 对象及1个 MASM 对象、981个不同 C 源、16个 FileAPI 目标和15个 archive。1034个头文件成员是潜在成员清单，实际 selected input 为1965条路径；tinydir 的空 C 单元与 libffi 的 ASM 零依赖分别有精确批准，不能推广为缺失依赖记录的豁免。两阶段只有本批两源的输入针及构建图对应叶针按批准内容改变。

## 现有验证与证据边界

同一 Windows x64 Clang Debug 配置在修改前后分别构建 `zr_vm_canonical_consumers_test`，再直接运行该 EXE：

| 阶段 | 构建实际结果 | Unity 实际结果 |
| --- | --- | --- |
| BEFORE | exit 0，7.094 秒 | exit 0，21 个有序 PASS，21 Tests / 0 Failures / 0 Ignored / OK |
| AFTER | exit 0，3.328 秒 | exit 0，同一21个有序 PASS，同一21/0/0及OK |

两份完整 Unity stdout 都是2789字节、SHA-256 `88c72ff59ed117ed4ec87571605a9faa07aed87b42b8f0cb43c20a2fee30e5e0`，stderr为空。每阶段2个执行步骤及4个运行后 Ninja 查询均等待退出0，6次fixture快照为空；21个driver Git与4个Root启动前Git也各自等待退出0。原始统计行自带一个尾部空格，解析只允许尾部水平空白，日志原字节保留。

唯一配对记录 `parser-canonical-consumer-tests-current-native-final-pair-r1.json`（`0bf73fb1cc22d5703d3b646d83045d783dab798208d05f40466549259eb67e18`，23150字节）由Root31668实际退出0。它逐阶段重解析原始查询、核对16个输出与1006个构建产物，并保留实际deps mtime；跨阶段只在比较依赖成员时排除mtime。产物中EXE、PDB及两个本批OBJ哈希改变，大小相同，其余针相同。唯一独立配对复核见 `parser-canonical-consumer-tests-current-native-final-pair-independent-review-r1.json`；其真实生成与完整复读终态绑定在交付台账中。

直接测试入口无额外 argv，120秒为本任务单进程超时策略；当前 CMake 只证明目标构建入口，不将其表述为 CTest 运行。每阶段独占 E 盘 fixture 目录与 TMP/TEMP/TMPDIR，清除8个现有 profile/trace变量；实际输出与目录最终状态分别记录。21个 C 用例的有序 PASS 与 Unity 总计不能被当作所有语言 body、所有回调、OOM/活跃 GC、线程安全、完整 VM/AOT 执行或其他平台的覆盖。

两叶共123行的首8列保持静态审查结果，第9列使用已逐项复核的完成理由；18项无需注释的理由原样保留。调用方、入口、断言证据、约束及结论均可按九列追踪。`check-batch` 实际退出0；全仓清单check修改前后实际退出[1, 1]，诊断输出相同。既有stale清单与用户Git ignore权限诊断保留在checks记录；本批验收不等于全仓已覆盖。

清单只更新两源的sha256/status/note三列，其他会话49行86单元格的工作树内容保留；导航仅追加本批链接。七条提交路径为两源、两叶、单个批次说明、清单与导航，使用独立Git索引精确串行提交，其他会话既有四条暂存记录继续保留。

保留原始 F/I、当前基线桥接、BEFORE/AFTER 原始输出、唯一独审和串行提交记录。产物生命周期 OPEN，实际手工释放0 B；共享 E 构建/cache及其他会话资产继续保留。
