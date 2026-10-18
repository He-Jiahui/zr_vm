---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
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
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.h
plan_sources:
  - .codex/plans/20261018-ssa-phi-identity.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
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
  - docs/testing-and-validation/ssa-phi-identity.md
doc_type: module-detail
module: parser
---

# 普通 PHI 的同源身份传播

## 当前状态

五个 PHI 生产文件、十一测试源和两处测试类型修正均已独立静审接受。实际配置图
为主测试39、GC回归34个编译角色。完整矩阵首次实际运行21组：20组通过，
唯一失败为direct SCCP早期三处分配失败没有覆盖调用前的诊断哨兵。
原运行保持失败；其它合法语义、预算与事务字段的实际覆盖见验证记录。

既有SCCP worker已最小拆分存储校验与分配失败分支；第六生产文件的修正经
EDA4E6D4静审接受，测试断言保持不变。原12个C文件的有限MSVC语法结果已
接受；新增SCCP单C语法经2D89B6E0独审接受，合为当前13C有限语法证据。
完整矩阵重试已自然退出0，21/21组及974/974分配失败点通过，经5A51F4D4独立实际审查接受；
六项依赖回归已自然通过，232/232个Unity用例及2350/2350个故障场景通过，
独立实际审查8EE2FF6D接受。提交和产物处置以本批清单及最终finite-close收据
为准；这些有限证据不等于02.01整叶或完整47叶验收。

实际验证及原失败收据的范围见[验证记录](../testing-and-validation/ssa-phi-identity.md)。

## 目标和调用位置

`p = PHI(a, a)` 表达相同 SSA 身份的选择。即使 `a` 是未知 external 值，
仍可在满足下述条件时把普通 use 中的 `p` 替换为 `a`。这不要求常量证明，
也不改变 SCCP 的常量 lattice。

工作归现有 COPY alias 职责：SCCP 内完成身份传播，原有 DCE 随后移除无用
logical PHI。scalar pass 列表保持 SCCP、DCE 两阶段，不添加第三个 pass，
不以外层反复运行转换来弥补链解析缺口。

## 文件职责

| 文件 | 职责 |
| --- | --- |
| `exec_ir_copy_aliases.c` | 原入口、旧 COPY-only incoming purity 限制，以及准备、写入、释放顺序。 |
| `exec_ir_phi_identity.c/.h` | 原图定义与别名解析、descriptor 和 tombstone、slot domain/owner、替换计划。 |
| `exec_ir_alias_availability.c/.h` | 从真实 CFG 证明定义、普通 use、PHI incoming 和 throwing result 的可用性。 |
| 原 SCCP / DCE / manager | 保持已有调用、逻辑 PHI 清理及各自事务合同。 |

私有 helper 不新增 public ABI。普通 Parser 源码 glob 包含新 `.c`；有限独立
验证入口把两个新 `.c` 分别加入已有 pooled scalar 和 GC-parser 两个私有
Parser target。两者都实际包含原 COPY alias 源码，须各自提供新 helper。
Root hook 从最终 target 属性导出六个消费者的源码图和设置，保留原 TSV 格式。

入口先调用 `PhiIdentityPrepare` 完成原图证明和替换计划，再调用不分配、
不会失败的 `PhiIdentityApply`，最后由 `PhiIdentityDispose` 释放计划。准备时
预算耗尽会丢弃整个计划并保守成功；scratch OOM 或容量溢出返回失败。

## 原图证明

解析器必须区分 MISSING、EXTERNAL、INSTRUCTION 和 PHI_ENTRY。PHI 是块入口
定义，不能伪造 instruction ID，也不能仅依赖 `values[].definition` 推断定义。

所有 COPY/PHI 依赖均从原图有界解析到同一个非 self 根。正向、逆物理顺序和
跨块链都使用同一证明；正在访问或已拒绝的节点不能当作成功叶子。self PHI、
相互依赖、COPY/PHI 混合环、`PHI(a,p)` 及仅由 backedge 提供的根不获别名。
不按 SCCP executable edge 子集忽略其它 ordered incoming。

每个被穿越的 COPY/PHI edge 都要求：

- 原始两端和最终根的 `typeToken`、nullability 精确一致。
- ownership 为 UNKNOWN；没有 PLACE_ADDRESS 或 PROMOTABLE_PLACE。
- 没有全局消费 tombstone。MOVE、DROP、DROP_IF_INITIALIZED 的消费记录须先
  扫描，包含出现在候选定义之后的消费；后续登记不能恢复被消费的身份。
- 使用保守的实际定义和可用性证明，不穿透 cast、convert 或 type-test。

这些条件也约束旧 COPY 登记。旧路径不能先擦掉不匹配的类型事实，再让新
PHI 路径或第二次优化调用错误收敛。

## CFG、异常和 slot

可用性使用真实边、可达性和支配关系，并检查同块指令顺序。direct SCCP
不能把缓存的 `immediateDominator` 当作事实。普通 observable、effect、memory
或合法 CALL 前驱不使 SSA 身份失效；新 PHI identity 不套用旧 COPY-only
incoming 的整前驱 purity gate。

throwing terminator 的结果还需要正常边和 exceptional successor closure 证明。
只有块支配关系不够；也不能简单禁用全部 throwing 根来代替这项证明。

新旧 store 都须保护 active token 覆盖和共享 backing 的所有 owner 上下文。
无法证明时跳过 slot；multiply-owned slot、ordinary/token overlap 和指令
`phiRange` overlap 可保守跳过。ordered duplicate predecessor 是不同 occurrence，
不得按集合去重。

结果保持 CFG、ValueId、InstId、sourceId、incoming backing/count 和非目标字节。
最小 RETURN witness 的 incoming 必须原字节不动；其它普通 PHI use 仅允许
修改已证明的精确普通 slot。只有现有 DCE 负责 logical PHI 的清理。

## metadata 和发布顺序

新身份传播遇到 stateMap、gcMap/gcRoots、deoptStates/Values/Aggregates/
AggregateFields 或 frameLayout 时冻结，不在本功能内修补这些表。frame 的
物理布局即使没有直接 ValueId，也不能从门禁漏掉。source-map-only 可作为
允许传播的正例。旧合法 COPY-only 行为仍须服从新的 descriptor 和 slot guard。

```mermaid
flowchart LR
    A[原图与全局消费] --> B[有界根解析]
    B --> C[真实 CFG 可用性]
    C --> D[slot domain 与 owner]
    D --> E[完整替换计划]
    E --> F[无分配写入]
    F --> G[原有 DCE]
```

所有新 scratch 分配和原图证明须在任何旧、新写入之前完成；写入阶段不再
分配。容量运算检查溢出；新扫描、图解析、owner 和 CFG 工作纳入实际 budget。
预算不足不发布未完成证明。没有普通 PHI 或没有候选时避免无关的 CFG 工作。

direct SCCP 在 alias helper 之前可能已经发布 cache，因此不能据本功能承诺
direct FALSE 恢复完整 cache/context。manager 和 public Optimize 分别按照真实
快照、candidate、remark、result 及发布合同验证失败和 retry。

## SCCP 分配失败诊断

worker先独立验证存储；失败时直接保留其具体诊断。仅在存储合法后的scratch
分配失败分支无条件调用NULL安全的`PassDiagnostic(OUT_OF_MEMORY, ...)`，随后
按原顺序释放三个临时数组。诊断是本次调用的输出，不要求调用者把旧code置NONE。
该修正不改健康分配数量、先后次序、预算计量或IR/cache/remark事务边界；
不能通过清零测试哨兵或降低OOM断言绕过原失败。

## 参考责任边界

本地 JDK `cfgnode.cpp` 的 `unique_input` 和 Mono `mini/ssa.c` 的相同 PHI 输入
处理提供了职责参照，不移植其 TOP/self/cast 等更强规则。JDK
`TestPhiElimination.java` 对 NotNull/OopMap 的回归说明 descriptor 保真不可省略。
zr_vm 的 ownership、token、state map 和异常可用性仍以本仓库的实际 IR 合同为准。
