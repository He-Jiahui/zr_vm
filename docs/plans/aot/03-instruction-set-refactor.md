# AOT 03：Place/Value/CFG 到 ExecIR

## 分层

```text
Syntax AST
  -> Bound/Semantic IR
     PlaceId + projections
     ValueId + canonical type
     CFG edges + dataflow facts
     cleanup regions
  -> backend-neutral ExecIR
  -> AOT C / AOT LLVM
```

Semantic IR必须区分 `LoadPlace`、`StorePlace`、`BorrowPlace`、`MovePlace`、`DropPlace`、`PropertyGet/Set/RefGet`、`ValueConstruct`、`GcNew`、`OwnConstruct`和普通`Call`。一个通用“load object然后运行时猜动作”的opcode不能承担这些职责。

## CFG 与 cleanup

- branch/join保存out definite assignment、availability、active loans与reachable facts的已验证结果。
- `return/throw/break/continue`、try/finally、partial construction、owner scope与using scope统一生成cleanup edge。
- ref struct/borrow escape在frontend拒绝；ExecIR只携带必要lifetime boundary与debug provenance。
- property accessor在binding后是普通call/Place projection；backing field不会由backend生成。
- bounds/null/weak-upgrade等runtime guard以显式IR op表示，优化器只能凭proof删除。

## ExecIR 要求

- 每个value/place具有Canonical TypeId。
- aggregate采用destination/SSA strategy的显式选择。
- effectful op声明mayThrow、mayAllocate、maySafepoint、reads/writes memory。
- cleanup和exception successor是结构化edge，不靠C `goto`文本反推。
- source map从Semantic IR稳定映射到ExecIR range。

## Syntax 上游追踪

| Syntax 节点 | 本计划消费的稳定输入 | 本计划退出责任 |
|---|---|---|
| [01/M2-M3](../syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md) | Place/Value、CFG、cleanup region 与前置 SemIR | 每个 source operation 形成唯一 validated ExecIR op/edge |
| [02/M2-M5](../syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md) | definite assignment、loan/NLL、receiver 与 escape facts | out/ref/move/borrow 只按已验证 facts lowering，不做 runtime borrow fallback |
| [03/M1-M5](../syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md) | destination Place、receiver effect、ref-like 与 bounds facts | ValueConstruct、aggregate projection、guard 与 cleanup op |
| [04/M1-M7](../syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md) | ownership operations、domain/transport 与 Drop plan | OwnConstruct/move/drop/bridge/safepoint/transport edge |
| [05/M2-M4](../syntax/2026-07-18-05-property-unified-ast-design.md) | explicit field Place、get/set/ref-get 与 region | Property access 收敛为 call/Place op，无 AST-shape lowering |
| [11/M4](../syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md) | typed generation 后的普通 bound declarations | generated body 与 source body 使用相同 SemIR/ExecIR validator |
| [12/M1-M6](../syntax/2026-07-20-12-async-task-job-scheduler-design.md) | async effect、Task/Job state、scheduler/domain/transport facts | suspend/resume/complete/fault/handoff 的显式状态与 cleanup edge |
| [13/M1-M4](../syntax/2026-07-20-13-iterator-enumerator-yield-design.md) | Enumerator witness、yield/state/frame contract | for/yield/resume/dispose 的显式 op、artifact 与 source map |

逐节点状态和 evidence 见[完整追踪矩阵](./syntax-contract-traceability.md)。

## 验收

覆盖 nested projection、ref-return property、out、move后分支、partial init失败、loop cleanup、try/finally、using close、resource Drop与async禁止跨越ref-like value。VM与AOT必须共享Semantic IR golden。

## 实施与晋级

1. **A3.1 Place/Value normalization**：所有local/field/index/deref/property-ref形成PlaceId与projection；rvalue、call和construct形成ValueId。旧stack/object opcode只能经versioned importer进入，不能成为新writer输出。
2. **A3.2 CFG/cleanup normalization**：把return/throw/break/continue/finally/Drop/Close/partial-init表示为显式edge和cleanup region，记录mayThrow/maySafepoint。
3. **A3.3 ExecIR validation**：验证每个op的Canonical TypeId、Place mutability/availability、successor、exception edge和source range；poisoned frontend fact不得进入codegen。
4. **A3.4 consumer parity**：VM、AOT C、AOT LLVM从同一golden读取，不允许backend自建property/borrow/owner lowering。

测试入口：`tests/parser/test_dataflow_engine.c`、`tests/parser/test_cfg_union_exhaustiveness.c`、`tests/acceptance/2026-06-20-semantic-stage1-cfg.md`、`tests/acceptance/2026-06-20-semantic-stage1-dataflow.md`、`tests/acceptance/2026-07-19-syntax-01-m2-place-cfg.md`。

必须加入的负例：maybe-uninitialized out、move后join、overlapping mutable loan、ref-return escape、finally内再次throw、constructor第N字段失败、unreachable cleanup和非法async suspension。退出时每个source construct都有唯一Bound/SemIR/ExecIR kind，并能roundtrip source map。
