---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.h
  - tests/parser/test_ssa_phi_identity.c
  - tests/parser/support/ssa_phi_identity_fixture.h
  - tests/parser/support/ssa_phi_identity_fixture.c
  - tests/parser/support/ssa_phi_identity_faults.c
  - tests/parser/support/ssa_phi_identity_support.h
  - tests/parser/support/ssa_phi_identity_builders.c
  - tests/parser/support/ssa_phi_identity_edge_cases.c
  - tests/parser/support/ssa_phi_identity_metadata.c
  - tests/parser/support/ssa_phi_identity_oracle.c
  - tests/parser/support/ssa_phi_identity_budget.c
  - tests/parser/support/ssa_phi_identity_transactions.c
  - tests/cmake/ssa-phi-identity.cmake
  - tests/parser/test_ssa_sccp_pooled_bool.c
  - tests/parser/support/ssa_sccp_pooled_bool_fixture.h
  - tests/parser/support/ssa_sccp_pooled_bool_fixture.c
  - tests/parser/support/ssa_sccp_pooled_bool_faults.c
  - tests/cmake/ssa-sccp-pooled-bool.cmake
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
  - tests/parser/test_ssa_dce_dead_phis.c
  - tests/parser/support/ssa_dce_dead_phis_fixture.h
  - tests/parser/support/ssa_dce_dead_phis_fixture.c
  - tests/parser/support/ssa_dce_dead_phis_faults.c
  - tests/cmake/ssa-dce-dead-phis.cmake
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_owner.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_internal.h
  - tests/parser/test_ssa_sccp_branch_sensitive_pool.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.h
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_faults.c
  - tests/cmake/ssa-sccp-branch-sensitive-pool.cmake
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_constant_materialize.c
  - tests/parser/test_ssa_sccp_pooled_int64.c
  - tests/parser/support/ssa_sccp_pooled_int64_fixture.h
  - tests/parser/support/ssa_sccp_pooled_int64_fixture.c
  - tests/parser/support/ssa_sccp_pooled_int64_faults.c
  - tests/cmake/ssa-sccp-pooled-int64.cmake
  - tests/parser/test_ssa_pass_manager_scalar_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_owner.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_constant_materialize.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
plan_sources:
  - .codex/plans/20261018-ssa-phi-identity.md
  - .codex/plans/20261018-ssa-scalar-completion.md
  - .codex/plans/20261017-ssa-scalar-pipeline.md
  - .codex/plans/20261017-ssa-sccp-bool-edges.md
  - .codex/plans/20261017-ssa-sccp-pooled-int64.md
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_phi_identity.c
  - tests/parser/support/ssa_phi_identity_fixture.h
  - tests/parser/support/ssa_phi_identity_fixture.c
  - tests/parser/support/ssa_phi_identity_faults.c
  - tests/parser/support/ssa_phi_identity_support.h
  - tests/parser/support/ssa_phi_identity_builders.c
  - tests/parser/support/ssa_phi_identity_edge_cases.c
  - tests/parser/support/ssa_phi_identity_metadata.c
  - tests/parser/support/ssa_phi_identity_oracle.c
  - tests/parser/support/ssa_phi_identity_budget.c
  - tests/parser/support/ssa_phi_identity_transactions.c
  - tests/cmake/ssa-phi-identity.cmake
  - tests/parser/test_ssa_sccp_pooled_bool.c
  - tests/parser/support/ssa_sccp_pooled_bool_fixture.h
  - tests/parser/support/ssa_sccp_pooled_bool_fixture.c
  - tests/parser/support/ssa_sccp_pooled_bool_faults.c
  - tests/cmake/ssa-sccp-pooled-bool.cmake
  - tests/parser/test_ssa_dce_dead_phis.c
  - tests/parser/support/ssa_dce_dead_phis_fixture.h
  - tests/parser/support/ssa_dce_dead_phis_fixture.c
  - tests/parser/support/ssa_dce_dead_phis_faults.c
  - tests/cmake/ssa-dce-dead-phis.cmake
  - tests/parser/test_ssa_sccp_branch_sensitive_pool.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.h
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_faults.c
  - tests/cmake/ssa-sccp-branch-sensitive-pool.cmake
  - tests/parser/test_ssa_sccp_pooled_int64.c
  - tests/parser/support/ssa_sccp_pooled_int64_fixture.h
  - tests/parser/support/ssa_sccp_pooled_int64_fixture.c
  - tests/parser/support/ssa_sccp_pooled_int64_faults.c
  - tests/cmake/ssa-sccp-pooled-int64.cmake
  - tests/parser/test_ssa_pass_manager_scalar_metadata.c
  - tests/parser/test_ssa_pass_manager_scalar.c
  - tests/parser/test_ssa_dead_source_places.c
  - tests/parser/ssa_dead_source_places_edges.inc
  - docs/acceptance/ssa-dead-source-places.md
source_refs:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.h
  - tests/parser/test_ssa_phi_identity.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
  - tests/parser/test_ssa_dce_dead_phis.c
  - tests/parser/support/ssa_dce_dead_phis_fixture.h
  - tests/parser/support/ssa_dce_dead_phis_fixture.c
  - tests/parser/support/ssa_dce_dead_phis_faults.c
  - tests/cmake/ssa-dce-dead-phis.cmake
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_owner.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_internal.h
  - tests/parser/test_ssa_sccp_branch_sensitive_pool.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.h
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_faults.c
  - tests/cmake/ssa-sccp-branch-sensitive-pool.cmake
doc_type: milestone-detail
status: planned
---

# 02.01 Pass 管理、SCCP、复制传播与 DCE

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 建立确定的 pass pipeline，并先实现容易验证的常量传播、SCCP、copy propagation 与 DCE。

**Architecture：** pass manager 声明分析依赖和失效集合，变换前后运行 verifier。标量变换以严格语义 evaluator 为唯一常量计算入口。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M6；基础优化可在 M1 foundation 后独立推进。
- 前置：[01.05 直接解释 Oracle 与无优化后端投影](../01-execir-ssa/05-oracle-projections.md)。
- 交付：pass registry、分析缓存/失效协议、标量 passes 和优化事件接口。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

### 有限 source temporary place 前置

实际 literal i64 SCRIPT 的共享 CoreExecIR 保留未使用 PLACE_BASE 地址及
EXTERNAL_ENTRY provenance。现有 DCE tombstone 和 VM emitter NOP 不压缩共享
ValueId 或 operand/result pools。独立事务入口的契约见
[dead-source places](../../../parser-and-semantics/ssa-dead-source-places.md)，
实际 RED/GREEN 状态见[验收记录](../../../acceptance/ssa-dead-source-places.md)。
它只接受真实 literal SCRIPT 身份及其精确 NOP 形式，保留 source/instruction
记录；rich metadata 显式拒绝。该有限入口不是完整 DCE/pass pipeline 门禁。
该有限 Windows gate 已通过30/30 focused与8/8相关消费者，完整 DCE pipeline
未完成。后续 primitive frame 使用显式目标布局事实，Linux矩阵仍OPEN。M6 与
SSA47 保持 OPEN；本任务直接修改 checkout，以 Git OID、源码/日志哈希和
receipt 记录版本，不生成源码快照副本。

compiler_quickening.c 包含大量后期 opcode 选择，不能继续承载 SSA 数据流。新增 exec_ir/passes 目录独立实现，保留 quickening 为 ExecBC 投影消费者。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c` | 只保留消费已证明事实的适配入口 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c` | pass 次序、验证、预算、分析失效 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c` | 可执行边与常量格 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c` | 保留可观察行为的删除/复制传播 |
| 计划新增测试 | `tests/parser/test_ssa_pass_manager_scalar.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## Source37 有限进展：非空池 Int64 写回（已提交与有限关闭）

实际RED保留53主案/107profiles、25已知功能FAIL与28PASS、212故障及live/errors0的历史证据。私有owner通道、typed Int64 materializer及candidate事务已取得实际GREEN及独审：53PASS/113profiles/492metrics、236/236故障完成、108726 allocation attempts、live/errors=0。基线池不写回及失败owner替换仍是历史RED事实，不升级旧失败收据。

本批只优化IR并真实Verify：canonical Int64池来源及result/operands proof，指令type可为0或Int64，生成tuple按(Int64, flags0, bits)去重；ADD/ARITHMETIC、SUB、MUL、NEG纯操作可写回。DIV的MAY_THROW/effect保留，正常商仅供后续纯consumer；危险DIV/溢出/非Int64/可观察边界保持。PHI对全部可执行incoming合取，UNKNOWN清proof；Source37历史owner实现保守覆盖正常后继并在未收敛cap清proof；当前Source38 typed求解替代规则见下一节。Legacy CONVERT/signedRepresentation、NEG MIN overdefined、NULL owner及空池协议保留。

公开Optimize先Verify并Clone candidate，所有函数与pool最终Verify后发布；FALSE只丢candidate并恢复remarks/result，原owner身份保持。私有FALSE要求调用方丢whole candidate，不承诺局部pool回滚。扫描每tuple1work、reuse预付rewrite1work、append预付append+rewrite2work，ready后立即无失败改指令，预算成功部分提交不得留下孤立常量。函数失败capture不自带独立module pool。

实际GREEN覆盖case30两真实UNKNOWN CFG反例；case47两私有入口×两diag各测W=25，fresh扫1..25共100边界全PASS。metadata15（排除MaterializeState）及GC-parser99经原24phase离线reconciliation接受：19编译、25精确cache、2link/2CTest、Unity15/99全PASS；原控制器failed收据保持false，未重复native。有限MSVC7C/8phase语法独审通过，99headers/106 consumed、34protected保持，仅syntax W0。实际提交`729c950d969663230e0171d60592939aabd60a00`及postcommit独审接受；6个自有文件清理释放2080877B，91个复用构建文件17409863B与必要raw保留，finite-close及post-cleanup接受。完成计数37=17功能/10修正/5验证/5文档，本次同步增加0；不勾下方整叶出口，不授fullW0/Linux/VM或完整02.01/SSA47信用，完整目标与未知归属边界仍active。

详见[实现合同](../../../instruction-generation/execir-pass-pipeline.md#nonempty-pooled-int64-writeback-and-module-transaction)与[验证记录](../../../testing-and-validation/ssa-sccp-pooled-int64.md)。实际报告根为`E:/cargo-targets/zr_vm/reports/ssa-sccp-pooled-int64-01a0fe2b/`。

## Source38 有限进展：分支敏感 pooled SCCP（已提交与有限关闭）

本轮在非空模块池的私有 owner 路径实现单调 typed lattice：外部/不支持来源为 TOP，支持但未求解的定义为 BOTTOM；canonical BOOL 仅 bits0/1、六种 strict Int64 比较及合法 COPY/PHI 能证明分支。已知条件选择真实 successor 槽，TOP 两边可达；BOTTOM 先暂缓，停滞后保守升 TOP 并展开残余边，再求固定点。PHI 按 predecessor occurrence 精确匹配实际边槽，覆盖 duplicate successor 和依赖先前剪边后 BOOL PHI 的 nested 条件；不会把 external UNKNOWN 当作常量证明。CFG/PHI 存储本身不压缩。

`exec_ir_sccp.c` 保留调度、scratch/cache/纯 consumer 写回；新增 `passes/exec_ir_sccp_owner.c` 负责 owner facts/edge 求解，`passes/exec_ir_sccp_internal.h` 共享私有 facts/checked 算术与 complete 接口。预算或防御 cap 导致 incomplete 时跳过 cache/rewrite 并清旧 SCCP facts；真实改写仍失效旧 cache，包括 changed=NULL。Source37 materializer 的 tuple(flags0)、预付预算、DIV/effect 保留、公有 candidate 原子发布及私有 FALSE 丢 whole candidate 规则继续成立；NULL owner/空池 legacy 不改。

实际 GREEN 22/47 全 PASS，39 FOLD；direct/pipeline×NULL/writable diag 四组各独立测 W=113，fresh 1..113 共452边界。故障125/125/248/248合计746/746，prefix27/52、remarks站109/134，late-function 与 remark 真实见证通过；388768 attempts/live0/errors0。31TU中4次fresh compile+27单一RED历史精确缓存+1link，fresh compile warning/error0。旧 failed RED 与离线 reconciliation 原样保留，不重复 native 或升级原失败收据。

当前focused-r2、三项回归及有限MSVC补证独审均已接受，实际提交与有限关闭完成。main计数类型按Unity字段改为UNITY_UINT后，focused configure/build/test r2实际通过：1fresh main compile+30 exact GREEN-r1 cache+1link，fresh warning0；22/47、452预算边界、746故障与388768/live0/errors0保持。

三项当前生产回归已独审：31自然0phase=25compile+26精确GREEN cache+3link+3CTest，强53/113 PASS、239fault、四W=51共204边界、117024attempts/live0/errors0，metadata15/GC99全部PASS。GC日志99 LEGAL=21 standalone+78 fresh baseline，另有78 MALFORMED，不能把原case分类当成日志行数。原MSVC r2一C4244记录保持；修复后1fresh main /W4/Zs+4 unchanged实际syntax复用=5currentC，96actualheaders/35protected保持、warning/error/unknown0，补证独审334CA901接受；focused-r2独审D9532A0D接受。实际提交`c90666fa38ef4eff3d5b4ceaed54744e9cad8eaf`，postcommit独审63B71E82与finite-close A0DBE118接受。标准清理5个自有文件释放2026503B，610必保pins复核一致且task tmp为空；保留build inventory390文件/69825703B包含other/unknown，不宣称全属Source38或全已验。当前完成38=18功能/10修正/5验证/5文档，本次文档同步adds0；只有限syntax零警告，不授fullMSVC/W0/Linux信用，不勾本叶任何完整出口；不授 VM/Oracle/FFI、Linux/fullW0 或完整47信用。详见[实现合同](../../../instruction-generation/execir-pass-pipeline.md#branch-sensitive-nonempty-owner-analysis-source38-validation-in-progress)与[Source38验证记录](../../../testing-and-validation/ssa-sccp-branch-sensitive-pool.md)。

## Source39 有限交付：rooted普通PHI DCE

普通PHI只在结果存活时传播incoming，由observable指令及GC/deopt/state引用根化，
无根self/mutual cycle与其纯producer可删除。只按旧物理顺序压缩phiPool，用prefix
重映射全部block.phis；共享incoming指针/count/capacity/bytes及surviving ranges不写，
CFG/ValueId/InstructionId身份保持。Token按typed consumer分类，不索引ordinary used；
普通/legacy PHI即使与token range别名仍独立验证1..valueCount。Core允许的初始零token
保持。五scratch、验证、marking、全部publish work预付在首次mutation之前；预算中断
TRUE/changedFALSE且IR不变，OOM FALSE在publish之前，PHI-only修改TRUE及真正repeatFALSE。

实际RED8有效，原116语义profile前后Verify全部有效；早期夹具前置错误证据保留而未
冒充RED。实际GREEN构建4compile+27精确cache+1link零诊断，默认21case/140profiles全PASS，
120语义profile全部pre/run/post/stable/expected1、CHANGE12/FIXEDPOINT12通过。8组测W85共
680个fresh边界，public有限上界202×2=404（并非实测public W）；610/610实际分配故障与
retry/cleanup通过，532777attempts/live0/errors0，无UBSan诊断。相应raw/收据及严格parser
已登记且GREEN独审2F818FD5接受。三回归实际24phase=18compile/33cache/3link/3CTest，
强53/113、四W51共204边界、239fault/117024attempts/ledger0，metadata15与GC99全PASS，
原始协议独审FC3716B2接受。四C有限MSVC r2实际自然0/诊断0，94actualheaders/228protected
保持；首轮中文include采集拒绝的失败收据保留，修复采集后复验通过，不扩大平台信用。
实际功能提交`6e6bc819b35ccee5ac3fea2bbabb389af5840213`与postcommit独审7AB375DF接受，精确9路径、
双He身份、7409条范围外index与foreign cached保持。当前39项=19功能/10修正/5验证/5文档；
产物清理保留结果见本批清单收据。此进度同步adds0，不勾整叶完整出口。详见[验证记录](../../../testing-and-validation/ssa-dce-dead-phis.md)。

## Source40 有限进展：非空池 BOOL 比较写回

独立读码确认owner typed lattice已有六种严格Int64比较的BOOL事实，旧rewrite仅接收Int64算术。最小实际RED已真实构建与Core/Parser Verify：2案4profiles，BOOL写回缺口1FAIL、disabled SCCP对照1PASS；全preverify/run/postverify/safety为1，634分配attempts、live/errors0。31fresh C单元与1link零诊断，Root与Luna独审接受；此前collector/无关foreign前置拒绝保留为失败证据。

三生产文件已实现非空池BOOL写回，focused实际证据独审A9214238接受22案/84profiles全部PASS：八组private实测W=37，完整1..W共296边界；public显式上界138×2=276点，不是实测public W；746/746实际故障与重试通过，459698attempts/live0/errors0。31TU当前证据为先前27个逐项accepted phase与4次fresh compile、1link、focused CTest，fresh诊断0，原warning失败收据保持失败。有限MSVC /W4 /Zs独审AC90E867接受8个当前C、98actualheaders/233protected保持；前5个未变实际syntax与修复旧Int64 main计数类型后3个fresh syntax组合，不授对象、链接、运行时、Linux/fullMSVC/fullW0信用。

实现复用唯一typed scalar materializer，按(type,flags0,bits)去重，BOOL仅0/1；COMPARE谓词token改CONSTANT必须写BOOL；严格来源、可观察元数据边界、预算ready与public候选事务保持。Source37 case32与Source38 COMPARE witness按精确BOOL语义迁移，保留旧安全断言。必要五项IR-only回归实际全PASS：pooled Int64 53/113及239故障、branch-sensitive 22/47及746故障、Source39 DCE 21/140及610故障、metadata15、GC99；三组故障与重试的ledger均0。回归35个fresh role对象与26个精确provider cache、5link/5CTest，actual r2 0DC200AA；原collector因正常link stderr拒绝的整体失败收据保持，未重复已通过编译。提交、自有清理与有限完成计数见总索引和本批收据；未执行VM/Oracle/ExecBC/FFI/MaterializeState。实现与验证详见[BOOL写回记录](../../../testing-and-validation/ssa-sccp-pooled-bool.md)。整叶与完整47出口不勾选。

## Source41 普通 PHI 身份传播：有限验证

在现有SCCP/COPY alias职责中传播`PHI(a,a)`普通身份，由既有DCE删除无用
logical PHI，scalar列表仍为两阶段。descriptor、全局消费、真实CFG/异常可用性
和全部共享slot owner均在写入前证明；token与身份metadata保守门禁保持。
原五生产源和十一测试源已有独立静审；fixture类型修正6581保持断言含义。
同B实际图D768C344核实主图39、GC图34、153条compdb。

完整首跑12补编、1link接受，21组20PASS/1FAIL；只有direct前三个OOM站仍留A5
诊断，原收据294A8994保持失败。284B75A5仅接受构建前缀，并核实524合法profile、
992次调用、W180/245与public有限上界554；974实际故障仅971完成。第六生产源
SCCP最小拆分storage与alloc失败，EDA4E6D4静审接受，测试oracle不变。当前完整
重试经5A51F4D4独审接受：21/21组、974/974故障点、owner全部平衡。原12C与
新增SCCP1C有限MSVC语法经3D114506及2D89B6E0接受。六回归已自然通过
232/232个Unity用例、2350/2350个故障场景，8EE2FF6D独立实际审查接受。
精确提交和自有产物处置以本批artifacts.json及finite-close.json为准；
这些有限证据不代替本叶全部编号任务及出口验收。
详见[模块合同](../../../instruction-generation/execir-phi-identity.md)与
[验证记录](../../../testing-and-validation/ssa-phi-identity.md)。

## 可逐项执行的重构任务

- [ ] **1. 定义 pass contract** 登记 requires/preserves/invalidates 与 changed 标记；改变 CFG 必须失效 dominators/loops/liveness。记录每 pass 时间和 IR hash，支持单 pass 关闭进行二分。

- [ ] **2. 实现 SCCP** 值格使用 unknown/constant/overdefined，边有 executable 状态；phi 只合并可执行前驱。整数溢出、除零、NaN、负零按语言规则计算。

- [ ] **3. 实现复制传播与 DCE** 替换等价 ValueId 后维护 use-list；无使用不等于无效果，保留 call、alloc 的可观察失败、drop、throw、barrier、await 和 volatile/native 读取。

- [ ] **4. 控制迭代与 remark** 工作队列收敛，设 compile-time budget；发布成功/阻断事件给 11.02，预算耗尽输出有效但较少优化的 IR。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
for pass in selectedPipeline:
    Verify(ir)
    ensureAnalyses(pass.requires)
    result = pass.run(ir, compileBudget)
    invalidate(result.changedDomains - pass.preserves)
    Verify(ir)
    emitRemark(pass, result)
DCE.erase(i) only if noUses(i) and provenUnobservable(i)
SCCP.phi = join(values from executableIncomingEdges)
```

代码改动失败与预算耗尽不是同一状态：前者停止编译并报告，后者保留最后有效 IR。默认严格数值，不允许用宿主 C 未定义溢出计算常量。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 菱形中不可执行分支除零 | 不可误触发异常 |
| 无使用但会抛异常的调用 | 仍执行 |
| 负零、NaN、整型边界常量 | 和 oracle 一致 |
| 重复跑 pipeline | 达到固定点且 hash 稳定 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_pass_manager_scalar` 和可执行目标 `zr_vm_ssa_pass_manager_scalar_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_pass_manager_scalar_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_pass_manager_scalar$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 每个 pass 独立差分与 verifier 通过；不添加函数级标注；编译预算可控。

**失败恢复：** 关闭单 pass 后仍能生成可执行 IR；保留最小失败 fixture，不能移除负测规避问题。

**文档交付：** 新增 docs/instruction-generation/execir-pass-pipeline.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrOptimizeOptions SZrExecIrOptimizeOptions;
typedef struct SZrExecIrOptimizationResult SZrExecIrOptimizationResult;
TZrBool ZrParser_ExecIr_Optimize(SZrExecIrModule *module,
    const SZrExecIrOptimizeOptions *options, SZrExecIrOptimizationResult *result);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| analysis revision | pass manager 在 CFG/value/effect 变更时递增 | 分析缓存按 revision 校验，不能只靠 changed bool |
| SCCP lattice | worklist evaluator | 仅 executable edges 贡献 phi |
| pass budget/remark | profile 配置与执行统计 | 超预算保留有效 IR，失败停止 codegen |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 实现空 pass pipeline、依赖登记、前后 verifier 和失败快照；注入一个故意损坏 IR 的测试 pass。

- [ ] **批次 2：** 实现 SCCP/copy propagation，使用显式数值 evaluator；测试不可达异常和常量格循环收敛。

- [ ] **批次 3：** 实现 DCE 与 bounded fixed-point；删除 use-list、source map、state map 的孤儿引用，保留可观察分配和 drop。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange unused call that appends event E
act DCE
assert call and event E remain
arrange unknown -> constant -> overdefined propagation cycle
assert convergence and no stale analysis reuse
arrange pass exceeds budget after valid transform
assert last valid IR retained and budget remark emitted
```

### 迁移结束检查

优化 passes 不读取 bytecode PC 或 cache target pointer；pass 开关只影响优化强度，不能决定语义是否合法。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
