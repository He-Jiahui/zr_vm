---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_owner.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_internal.h
  - tests/parser/test_ssa_sccp_branch_sensitive_pool.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.h
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_faults.c
  - tests/cmake/ssa-sccp-branch-sensitive-pool.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_owner.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_constant_materialize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
plan_sources:
  - .codex/plans/20261017-ssa-sccp-bool-edges.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
tests:
  - tests/parser/test_ssa_sccp_branch_sensitive_pool.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.h
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_faults.c
  - tests/cmake/ssa-sccp-branch-sensitive-pool.cmake
source_refs:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_owner.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_internal.h
  - tests/parser/test_ssa_sccp_branch_sensitive_pool.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.h
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_faults.c
  - tests/cmake/ssa-sccp-branch-sensitive-pool.cmake
doc_type: testing-guide
status: in-progress
---

# 分支敏感 pooled SCCP：Source38 有限验证

## 范围与当前状态

本轮只通过真实 IR 构造、Core Verify 和实际公开 Optimize/私有 owner SCCP/pipeline 验证非空模块池的分支敏感传播。不是运行 VM、Oracle、Interpret、FFI、网络或 MaterializeState；分支指令与 CFG/PHI 存储保持，观察点是选择边后的 PHI40/50→纯 ADD2 能否物化42/52，以及未知路径是否仍不折叠。

当前 **实际验证已通过，待focused-r2/有限MSVC补证独审及提交/清理**。Source38 focused r2、当前生产的强Source37 53/113、metadata15、GC99回归及当前五C有限MSVC syntax已有真实通过证据；三项回归独审已接受，focused-r2与MSVC补证独审在本次更新时仍pending。不是Linux/fullW0、完整MSVC或02.01/SSA47签收。历史完成数仍37=17功能/10修正/5验证/5文档，本次文档维护增加0成果。

实现合同见[pass pipeline](../instruction-generation/execir-pass-pipeline.md#branch-sensitive-nonempty-owner-analysis-source38-validation-in-progress)，完整未勾出口见[02.01计划](../plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md)。Source37已关闭证据见[pooled Int64记录](ssa-sccp-pooled-int64.md)，不将其旧结果冒充本次新生产回归。

## 实现与安全边界

| 文件 | 当前责任 |
| --- | --- |
| `passes/exec_ir_sccp_owner.c` | owner-only BOTTOM→exact typed constant→TOP 单调 facts、可执行 edge 槽、PHI occurrence、预算化固定点与残余 BOTTOM 展开 |
| `passes/exec_ir_sccp_internal.h` | 私有 facts、checked Int64算术、complete/incomplete接口；不改公共 cache ABI |
| `passes/exec_ir_sccp.c` | legacy/owner分派、scratch寿命、cache发布/失效、已有纯Int64 consumer写回 |

canonical BOOL仅0/1能证明条件；其他编码保守TOP，不宣告全局不合法。六种strict Int64比较、匹配类型的COPY/PHI可传播证明；external/不支持来源TOP，受支持但未求解定义BOTTOM。已知条件按true0/false1槽择边；TOP展开两边；BOTTOM停滞后保守展开，避免依赖nested条件被过早冻结。重复successor按前驱出现次数配对，不能只按(source,target)合并。loop/unknown控制组保持安全，CONVERT/retagged COPY不制造proof。

预算中断或cap未收敛不导出不完整facts、不改写并清旧SCCP cache；changed=NULL仍跟踪真实改写并失效cache。既有非空池 materializer 仍按(Int64,flags0,bits)去重，lookup/reuse/append+rewrite预算预付；公开模块先Verify、Clone candidate、最终Verify才发布，FALSE保持原owner身份并丢candidate，私有FALSE要求调用者丢whole candidate。DIV保留MAY_THROW/effect，不能删除observable边界。NULL owner/空池legacy语义保持。

## 真实 fixture 与协议

唯一focused CTest selector为 `^ssa_sccp_branch_sensitive_pool$`。主入口22个独立Unity case；47个有序profile包括39 FOLD、4预算profile、4故障profile。每个profile先释放自有IR/remarks/cache，核ledger后再Unity断言；无mock Optimize、无未来符号或poison pointer。协议 `BOOL_EDGE_CASE/PROFILE/FOLD/MEASURE/BUDGET/TRACE/FAULT/SUMMARY` 对字段和顺序封定，CASE在Unity自然返回后输出。

矩阵覆盖true/false、六比较、BOOL COPY、相同PHI与duplicate槽、依赖第一层剪边的nested BOOL PHI、external/true+UNKNOWN PHI、非canonical数值来源、相异PHI、loop稳定/未知、legacy NULL-owner/空池、dead DIV0及MIN/-1、observable consumer、重复固定点、预算与实际OOM。BOOL bits>1未作为本轮额外profile运行；实现对此保守TOP，不扩大fixture实测范围。

每次实际输入与最终输出均真实Verify；missing-fold是注册功能RED，健康和安全失败不算RED。故障站从当次健康allocator trace实测，逐站触发并核owner/content/remarks/resultzero、真实trace前缀、重试Verify/fold和释放；不读取意外释放的旧owner来伪证回滚。

## 已接受实际 GREEN

| 项 | 实际观察 |
| --- | --- |
| Unity / profiles | 22 PASS / 0 FAIL / 0 ignored；47/47 profiles PASS，39 FOLD |
| 初始r1配置与构建（历史保留） | 31TU；4实际fresh compile+27来自单一actual RED历史的精确缓存+1link；5 build phases自然0、streams关闭、handles释放 |
| 消警后focused r2 | main唯一unsigned→UNITY_UINT匹配Unity.TestFailures计数类型；fresh configure，1fresh main compile+30 exact GREEN-r1 cache+1link，fresh warning0；22/47与预算/故障/ledger指标保持，补证独审pending |
| 编译诊断 | 四处fixture消警后的fresh编译warning/error0；非全仓W0 |
| 私有预算 | entry0=direct RunModuleSccpPass，entry1=实际注册单SCCP RunModulePassPipeline；各×NULL/writable diag独立测W=113，fresh扫1..113，共452边界 |
| 故障 | TRACE125/125/248/248，共746/746完成；prefix27/52，remark站109/134；可写diag组具有真实late-function及remark见证 |
| ledger | 388768 allocation attempts；live0/errors0 |
| 三项实际回归（已独审） | 31自然0phase：25compile+26精确GREEN cache+3link+3CTest；强53/113、239fault、四W51共204cutoffs、117024attempts/ledger0；metadata15及GC99全PASS |
| 当前有限MSVC | 1fresh main /W4/Zs+4 unchanged C actual syntax复用=5currentC；96actualheaders/35protected保持，warning/error/unknown0；原r2一C4244保持历史，补证独审pending |
| 当前输入 | 独审首尾378去重pins匹配，215actual headers、31objects、4products；实际argv仅增加/showIncludes观察依赖 |

四组W来自本轮真实私有entry/context，不拿公开Optimize不可见workUsed冒称精确上限。公共maxWork1只作每diag一次partial-success见证；budgetExhausted成功不是失败回滚，最终Verify且无孤立新增常量。不同入口或未来生产的W需重新实测，不固定113为算法合同。

## 不可变证据与失败历史

全部实际报告位于 `E:/cargo-targets/zr_vm/reports/ssa-sccp-bool-edges-01a0fe2b/`：

- `green-configure-r1-receipt.json`：183226B，SHA `EAF10175F34388645A658EAC71692107E52C40778A054A3D69C3B1A2E7C5ED99`。
- `green-build-r1-receipt.json`：1620977B，SHA `D09C4EE4EDC07694529D7486F885EB3A88B45A63A009BD7DD942830ED636A7B8`。
- `green-test-r1-receipt.json`：116538B，SHA `D007C1A4AFA59B85E3E092BB53712F7C8AF0C9EB72C34F53AF3C3DBB732D8A8B`。
- `green-independent-review.json`：4608B，SHA `84610198EA04472B0935BF74E15458E85791EA9603085856E11B6B08ECB4814B`，有限actual GREEN接受。

消警后的实际证据（不覆盖上述r1历史）：

- `green-configure-r2-receipt.json`：184027B，SHA `FE7CA4EC39CABBD733BA397CBEE3D5DC2C95FB31333847CD8A7316107EDD2309`。
- `green-build-r2-receipt.json`：1610397B，SHA `868AEE665151B7B797D05241D33C801CA23778B7D4E8883082088A2E88E787BB`。
- `green-test-r2-receipt.json`：116618B，SHA `AC634D02C5808A0C551BFAD134E6C3479F40B87244FA5F7B636D17CD33E0A4CE`。
- `regression-r1-receipt.json`：2883187B，SHA `3C9C59154D365447934E3D9B10988CADB623BE9EBC9E0C2F9F54C6AD7EDEFB04`；`regression-independent-review.json`：14905B，SHA `33D43DBEF2DDBE18449A615FEA9BBA718D0CF0FB77B8B036E0AE0D90315B58FD`，accepted。
- `msvc-syntax-main-r1-receipt.json`：152159B，SHA `CBF86DFA835E8C2427B009CDE9C48B453D01517746446CA7A2A92EF909C6CC45`；只表示1次fresh main语法验证加4个未变C的既有actual syntax身份复用，不能称5次重编。

focused-r2和MSVC补证独审尚待最终报告，不预填其SHA或接受结论。GC99的实际事件包括21个独立合法case与每个畸形case之前的78个fresh kind0合法baseline，因此是99 LEGAL行和78 MALFORMED行；Unity仍99case全PASS。旧Source37 W25/236fault与本次强回归W51/239fault分别保留，不混作同一次测量。

`red-test-r1-receipt.json`因标准CTest失败stderr通知而保留accepted=false；`red-reconciliation-receipt.json`在不重复native前提下接受原始功能RED，原failed收据不升级。此前configure/build原始失败证据也保留。27缓存对象逐项匹配同一 `red-build-r1-receipt.json` 的source/argv/tool/env/dependencies/object，未将同源foreign对象或mtime当作cache来源。

## 待补证独审与发布清理

三项当前生产回归已实际通过并独审；metadata15明确排除旧checkpoint MaterializeState例。focused-r2与有限MSVC当前证据已通过，补证独审仍pending。Git发布、独立postcommit、清理与finite-close尚未完成；当前仍37项完成成果。没有全叶勾选、完整MSVC/fullW0/Linux或执行后端信用；后续仅依实际新证据补充本节。
