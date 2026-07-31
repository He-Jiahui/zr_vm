# AOT 06：分层实施路线

## 两轴实施模型

AOT 同时遵守两个顺序：

1. **后端依赖轴**：Foundation -> Typed execution -> Control/cleanup -> ABI/backend -> Runtime bridges -> Packaging/stripping -> Convergence。
2. **Syntax readiness 轴**：每个 work package 只能消费[完整追踪矩阵](./syntax-contract-traceability.md)中已达到上游 gate 的 canonical contract。

Syntax 文档编号不是 AOT 实施顺序。AOT 不能因为某个高层 fixture 可运行而绕过较低层 Type/Layout/Place/cleanup/schema gate，也不能因为 Syntax 里程碑完成而自动宣称后端完成。

## 后端 Gate

| AOT gate | 必需 Syntax readiness | 退出 artifact / 行为 | 不得提前宣称 |
|---|---|---|---|
| A1 Foundation | `01/M1-M4`；按需 `03/M1`、`10R/M1-M2` | canonical type/place/CFG/schema writer-reader golden | 任意新语法可执行 |
| A2 Typed execution | A1；`01/M5`、`02/M1-M4`、`03/M1-M4`、`05/M1-M4` | VM/AOT scalar、aggregate、ref、property parity | owner、async、iterator 已完整 |
| A3 Control/cleanup | A2；`02/M3-M5`、`04/M1-M3`、`05/M2-M4` | normal/abrupt/partial-init exact cleanup | scheduler/transport/iterator cleanup 已闭合 |
| A4 ABI/backend | A2-A3；`03/M1-M5`、`04/M4-M7`、`10F/M3` | C/LLVM call/return/native ABI、target layout parity | 跨 target/layout/schema 兼容 |
| A5 Runtime bridges | A3-A4；`04/M1-M7`、`08/M1-M5`、`09/M1-M5`、`12/M1-M5`、`13/M1-M3` | GC/owner/reflection/pooling/task/iterator runtime contracts | full metadata/trim/package closure |
| A6 Packaging/stripping | A1-A5；`05/M5`、`08/M3-M5`、`10R`、`10C`、`11/M3-M5`、`12/M6`、`13/M4`、`14/M1-M4` | ModuleIdentity/FfiSignature/DebugMap/frame/TestManifest `.zro/.zrm` closure | source/binary/current reference 全等价 |
| A7 Convergence | A1-A6；`06B/M4-M5`、`07B` 与 08-14 所有适用行 | interp/binary/AOT C/AOT LLVM、artifact、LSP/debug 纵向矩阵 | 全仓完成，除非全矩阵和 allowlist 证实 |

这里的 gate 是后端共享依赖，不替代 Syntax 自己的 M1-Mn。具体逐节点输入、AOT owner 和状态始终以[矩阵](./syntax-contract-traceability.md)为准。

## 分阶段 Syntax 节点

### 06A 与 06B

- `06A/M1-M3` 交付 inventory、migration frontend 和 repository dry-run。AOT 在这一阶段只登记旧 writer/opcode/string fallback、重建命令和 targetNotPromoted owner。
- `06B/M4-M5` 在 08、10-14 及相关 AOT 行闭合后执行 atomic cutover 和 cleanup。旧 reader 若保留，只能作为显式 conversion/report 工具，不能进入 current execution path。

### 07A 与 07B

- `07A` 建立 fixture 和 coverage manifest 骨架，允许 feature 状态为 blocked/unsupported。
- `07B` 必须在 06B、08-14 和 A1-A6 后端 gate 后运行同一 fixture 的 interp、binary-first、AOT C、AOT LLVM、artifact、LSP/debug 矩阵，才可晋级 current reference。

### 10R、10F 与 10C

- `10R/M1-M2` 冻结 ModuleSpecifier/ModuleIdentity、manifest、artifact entry 和 provider phase substrate。
- `10F/M3` 从 canonical callable/layout/ownership contract 生成 FfiSignature；VM/libffi 与 AOT 共用测试向量。
- `10C/M4-M5` 汇聚 08、09、11-14 已晋级的 provider inventory、TypeId、phase、roots 和 consumer；不替 owner plan 补实现。

## 每个 Work Package 的固定步骤

1. 在矩阵中定位唯一 Syntax 节点和当前 AOT 状态。
2. 添加 artifact/IR golden 与一个预期失败的 focused test；上游未就绪时保持 `blocked_by_syntax`。
3. 实现最低共享能力，不在 backend 增加 fixture 名、member 名、类型名或 source spelling 特判。
4. 先跑 canonical/CFG/layout leaf，再跑 core/runtime、artifact writer-reader、backend target、project/CLI 和 consumer parity。
5. 写 evidence-scoped 完成记录，并只更新对应矩阵行；部分证据使用 `partially_verified`。

## Promotion Gate

每个 AOT gate 必须同时满足：

- 输入 Syntax 节点和 schema version 明确；未映射或未晋级输入 fail closed。
- 旧 reader/writer/opcode 的迁移或拒绝策略明确。
- VM、binary-first、AOT C、AOT LLVM 的行为、异常和生命周期计数一致。
- diagnostic 可定位到 IR op、TypeId、source range 和 target。
- debug verifier 可检查 layout/frame/root/cleanup；性能指标有 baseline。

单工具链、单 backend、单 fixture 或单个 completion record 只能晋级子切片，不能把整行或整个 AOT gate 标为完成。

## 与既有实现的关系

已有 typed register、loop、reflection、metadata、reachability 和 frame verifier 是 A2、A4-A6 的输入 baseline。只有接入对应 Syntax 节点的 Canonical TypeRef/Place/ModuleIdentity/FfiSignature/FrameLayout/TestManifest，并通过本路线的四执行路径矩阵后，才算目标 AOT 完成。
