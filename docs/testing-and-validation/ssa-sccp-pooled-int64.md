---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_constant_materialize.c
  - tests/parser/test_ssa_sccp_pooled_int64.c
  - tests/parser/support/ssa_sccp_pooled_int64_fixture.h
  - tests/parser/support/ssa_sccp_pooled_int64_fixture.c
  - tests/parser/support/ssa_sccp_pooled_int64_faults.c
  - tests/cmake/ssa-sccp-pooled-int64.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_constant_materialize.c
plan_sources:
  - .codex/plans/20261017-ssa-sccp-pooled-int64.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
  - docs/plans/ssa/00-measurement-contracts/03-differential-harness.md
tests:
  - tests/parser/test_ssa_pass_manager_scalar_metadata.c
  - tests/parser/test_ssa_sccp_pooled_int64.c
  - tests/parser/support/ssa_sccp_pooled_int64_fixture.h
  - tests/parser/support/ssa_sccp_pooled_int64_fixture.c
  - tests/parser/support/ssa_sccp_pooled_int64_faults.c
  - tests/cmake/ssa-sccp-pooled-int64.cmake
doc_type: validation
status: draft
---

# 非空常量池 signed Int64 SCCP 写回验证

## 当前证据状态

Source37 已观测真实 RED：53 个主 case、107 个有序 profile，25 个已登记功能失败、28 个主 case 通过；212 个实测故障观测全部完成，allocator live/errors 均为0。未知 fixture、诊断、链接或 ledger 错误不计功能 RED。基线非空池不写回及失败回滚 owner 替换保持为历史事实，原失败收据不升级为修复后结果。

实际RED后，生产修复已取得实际GREEN及独审：53主案全部PASS、113有序profiles、492条metrics、236/236实测故障完成，108726分配attempts且live/errors=0。四个owner预算测量各W=25，fresh完整扫1..25共100边界全PASS。metadata15与GC-parser99原始运行经离线reconciliation接受，有限MSVC七次syntax也已独审。提交和cleanup仍PENDING，完成计数保持36，不计第37项完成成果，不关闭完整02.01/SSA47。

历史缺口位于 SCCP rewrite：非空池直接跳过结果物化；空池 layoutId 是立即数，非空池 layoutId 是池索引，不能混用。旧模块 Optimize 失败后释放原模块并赋回 snapshot，会改变 owner 身份；新实现先克隆独立 candidate，整模块优化并真实 Verify 后才发布。FALSE只丢 candidate，恢复remarks count、清零result，保留原模块内容和各owner身份；预算耗尽的合法true可提交已验证部分优化。

## 交付合同

| 范围 | 本批要求 |
| --- | --- |
| 数值 | 规范 Int64 来源的 ADD/ARITHMETIC、SUB、MUL、NEG；支持负数及完整 64 位结果 |
| 常量池 | 只写已有非空池；生成 flags=0，按 typeToken、flags、bits 三元组去重；跨函数刷新视图 |
| DIV | 真实 MAY_THROW 与有效 effect 链保留；正常商可传播至后续纯消费者，DIV 本身不删除 |
| 不物化边界 | 溢出、NEG MIN、除零、MIN/-1、MUST_THROW，以及可观察 flag、effect 或 memory 边界 |
| 类型来源 | 独立私有 strict Int64 proof；窄 signed、FLOAT、UINT64 不能通过 COPY、PHI、多级算术重标成为有效来源 |
| 模块事务 | Clone 到 candidate，所有函数优化后真实 VerifyModule，成功才发布；false 保持原模块内容及 owner 身份 |
| 预算 | 耗尽可以返回 true 并提交有效的部分优化；不得留下未引用的新常量；false 与耗尽分开观察 |
| 公开接口 | 公共 PassContext、函数签名和分析缓存布局保持；模块 owner 通过私有显式参数传递 |

旧 signedRepresentation 证明运行时存储类别，不能单独证明 canonical Int64 来源。新 proof 只由匹配类型的 Int64 池值建立；COPY/MOVE、可执行 PHI incoming 和纯算术逐层传递。来源丢失必须影响格合并和收敛，不能仅在末端检查结果类型。显式instruction type只允许0或Int64，同时真实result、池tuple及各operand必须符合canonical Int64证明。Owner PHI逐一合取全部真实可执行incoming，UNKNOWN会清除proof；owner条件分支保守覆盖全部正常后继，避免BOOL的legacy numeric事实遗漏边。实际joined value/proof决定收敛，轮次上限未收敛则清proof。Legacy CONVERT的signedRepresentation及NEG MIN的overdefined语义保留。

私有模块 pipeline 的调用方拥有 candidate；它不是另一个独立池回滚 API。公开模块 Optimize 才负责候选整体销毁、原 owner 身份保持和最终发布。函数级失败快照也不声称携带独立模块常量池。物化helper每扫描tuple收1work，复用预留rewrite1work，新append预留append+rewrite2work；ready=true后立即无分配/无失败地改指令。instructionId为1基，poolIndex为0基；ready=false只表示已标记预算耗尽，不能留下孤立新项。私有FALSE调用方必须丢whole candidate。

## 实际测试矩阵的验收要求

53个主case的稳定ID和名字已封定；历史RED107profiles与实际GREEN113profiles分别保留。GREEN实际通过94条FOLD、4条OWNER_MEASURE、100条OWNER_BUDGET、0条PENDING，共492条metrics，53个Unity case全PASS。覆盖包括：

- 第一个模块池为 Int64 40、2，真实 VerifyModule 后经公共 Optimize（禁 DCE），要求 ADD 变成指向 Int64 42 的池索引，再次真实 VerifyModule。
- 正常运算、负值、32 位以上结果、Int64 极值、精确去重、多函数共享、多次真实扩容与固定点。
- 三个合法 DIV 链，由纯 ADD/NEG 消费正常商；危险 DIV 的消费者不能被物化。
- flags key 对照采用未被 CONSTANT 引用的 opaque 非零 flags 元数据 tuple；不赋予其执行语义，也不引入不存在的 helper 或 test hook。
- FLOAT/UINT64 COPY 与 PHI 后的纯消费者链，以及 Int32→COPY Int64→ADD→NEG→ADD、直接 Int32+Int64→ADD→NEG 两条来源反例。
- 可观察动态 flag、真实 effect 链、实际 memoryTokenPool/range、空池协议、没有 owner 的公开函数入口、cache 失效和失败恢复。
- 逐次实测分配故障、第二函数阶段故障、真实 remarks 扩容失败、预算边界、清理与健康重试。
- 公开 Optimize 对越界池索引、池项与结果类型不符、显式指令类型不符的拒绝及 owner 保持；明确预期拒绝，不把非法 fixture 当成合法 nofold。

所有被当作正常运行的 fixture 先真实验证。故障注入前完成同输入的无故障前提；每个 owner 先由严格 allocator 证明 live span，再读内容。断言前完成自有清理，不以 stale 指针比较或读取制造通过。

## RED 的允许范围

RED 包含两类已知功能缺口：结果未物化，以及模块失败回滚替换 owner。后者仍须满足逻辑内容恢复、当前模块可验证及严格 ledger 安全。健康重试尚未物化也只能记为已知功能缺口。

未知诊断、fixture 验证错误、编译或链接错误、非法访问、泄漏及 ledger 错误都不能作为预期 RED。控制器须按实际 case 和子断言分类，不能只接受 CTest 非零或一个总失败数。GREEN 要求全部功能和安全断言成立。

## 预算的可观察性

公共 OptimizeResult 没有 workUsed 字段。RED 用现有 caller PassContext 与非 NULL 的零上限 budget 测量已有 pipeline 的工作，先断言 workUsed>0。GREEN 再通过必要的私有 owner 通道，以同一 caller context 实测物化路径，按该路径真实上限扫有限预算。

公共 Optimize 另做明确有限区间的预算检查，只观察公开 budgetExhausted、结果、最终 Verify 与池引用；不同路径的工作量不能当作同一个精确计数。禁止空循环 PASS，也不为测量新增公共字段或生产 test hook。

实际GREEN的case47保留公开有限预算区间，再分别调用RunModulePassPipeline(SCCP-only)和RunModuleSccpPass。每入口×NULL/writable diagnostic用非NULL零上限budget独立测W=25，两相同ADD均fold且仅新增1项，覆盖append与reuse；fresh candidate完整扫1..25，共100边界。每次work<=limit，limit<W exhausted，limit=W全fold且不exhausted；真实Verify、prefix/context view/scratch保持、无孤立项，清理ledger0。这些私有计数不冒充公开Optimize的精确工作上限。

## 构建、回归与执行边界

源码直接使用 E:/Git/zr_vm 的实际 checkout，以 Git OID、差异和文件哈希记录身份，不创建源码副本。产物及临时文件只进入 E:/cargo-targets/zr_vm 的已授权目录；复用既有同工具链配置的有限 owner 构建树。

历史RED使用clang-cl/Ninja/Debug/C11/MD及UBSan，真实29TU已编译链接执行；实际GREEN配置图为Core15+Parser10+测试5，共30TU，新增materializer已由真实配置/编译/依赖/链接证明。复用对象仍须匹配源码、完整argv、实际依赖及对象哈希；链接保留实际对象、map、archive和UBSan provenance，不授未知历史对象信用。

只运行明确登记的metadata target。旧module_constant_pool_verifier main实际调用RunOracleEx，本批不运行也不声称通过；本批池准入子profile覆盖所需边界。scalar实际使用metadata15 wrapper，排除调用MaterializeState的原第16例；GC-parser99也全PASS。原运行24phase（环境1、编译19、链接2、CTest2）均自然0、双流关闭，25对象精确复用。GC事件严格为21个独立legal、78个malformed前健康kind0基线和78个malformed，逐案核对顺序及Unity99/0/0。原failed回归收据因控制器错误“GC legal21 incomplete”保持false；r4修复仅对原始日志离线重解析并核对当前输入/对象/产物，新reconciliation accepted，未重编或重跑native。

MSVC实际七次/Zs语法检查、7唯一C、含环境共8phase均自然0且双流关闭；99实际headers/106 consumed的BEFORE/AFTER匹配，34受保护构建产物保持，warnings/errors/unknown均0。仅有限syntax W0，没有MSVC对象/链接/运行时/fullW0或Linux信用。

本批不执行 VM、SourceExecute、FunctionExecute、Oracle、Interpret、FFI、network/security、运行时 materialization 或 resume。私有“常量物化”只生成 IR 池项，不执行运行时值恢复。本轮不授 Linux、全量 W0、完整后端或整叶完成信用。

## 证据与产物归属

实际阶段证据均在同一物理报告目录，关键文件：

- native-red-test-r2-receipt.json / native-red-test-r2-review.json：真实53/107/25功能FAIL、212故障与ledger0。
- production-root-independent-review.json与production-sccp-authoring-r2.json：Root生产静审及SCCP构造器/可选type0修复身份；SCCP独审由另一审查员提供，不以自审替代。
- green-fixture-independent-review.json：静态53/113/94FOLD、两UNKNOWN CFG与四独立预算测量协议；不是实际GREEN结果。
- native-green-test-r1-receipt.json / native-green-test-r1-independent-review.json：实际53PASS/113profiles/492metrics、236faults、100owner预算与ledger0，独审接受。
- pooled-regressions-r1-receipt.json：保留原控制器失败；regressions-r1-reconciliation.json：绑定原24phase/raw与当前389 BEFORE、300 consumed、44objects、8products，接受metadata15/GC99；不升级旧收据、不重复native。
- msvc-syntax-r1-receipt.json / msvc-syntax-r1-independent-review.json：7C/8phase、99headers/106consumed、34protected保持，仅有限syntax W0。

任务清单及原始验证证据位于：

`E:/cargo-targets/zr_vm/reports/ssa-sccp-pooled-int64-01a0fe2b/`

创建输出前登记 owner、物理路径、依赖和保留条件。阶段结束先确认自有进程自然终态，随后清理明确归属的临时 helper、私有 Git metadata 和失效产物；保留当前复用构建及必要失败证据。既有 B11、Parent1 未知项及其他会话状态原位保留，不能据此宣称全局清理关闭。
