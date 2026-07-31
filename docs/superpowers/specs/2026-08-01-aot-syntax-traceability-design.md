# AOT 与 Syntax 完整追踪矩阵设计

## 目标

让 `docs/plans/aot` 对 `docs/plans/syntax` 的当前权威设计形成完整、可审计、不会复制语义的后端投影。任何 Syntax 主题、阶段节点或里程碑都必须能定位到对应 AOT 责任、产物、晋级门和独立状态；任何 AOT 工作包也必须能反向定位到其 Syntax 上游。

## 权威边界

- `docs/plans/syntax/README.md` 及其链接的 01-14 设计定义语言语义、依赖图和 Syntax 晋级门。
- `docs/plans/aot` 不重新定义语法、Canonical TypeRef、Place/Region、ownership、property、module、Task、Iterator 或 TestManifest 语义。
- Syntax 里程碑完成只表示 AOT 可以消费其稳定投影，不自动把 AOT 工作标记为完成。
- AOT 历史完成记录只证明记录中明确列出的 evidence scope；修订不得改写其完成时间、命令或结论。
- 当前未提交的 Syntax 文档只作为只读上游；本次修订不修改 `docs/plans/syntax`。

## 采用方案

保留现有 AOT 后端子系统结构，在其上增加双层追踪：

1. 主题级矩阵覆盖 Syntax 01-14，并显式拆分 `06A/06B`、`07A/07B`、`10R/10F/10C`。
2. 里程碑级矩阵覆盖每份 Syntax 设计中的稳定 M1-Mn 或阶段节点。

不把 AOT 目录复制成 Syntax 目录镜像。镜像会重复语言设计、割裂同一个后端工作包并增加漂移风险。

## 文档结构

### 1. 索引

更新 `docs/plans/aot/index.md`：

- 声明 Syntax README 是唯一上游索引。
- 提供 Syntax 01-14 的完整主题级投影。
- 显示阶段节点依赖，而不是把 06、07、10 当作单一 gate。
- 链接详细追踪矩阵和各 AOT 子系统计划。
- 定义同步规则与未映射节点的 fail-closed 处理。

### 2. 详细追踪矩阵

新增 `docs/plans/aot/syntax-contract-traceability.md`。每行采用以下字段：

| 字段 | 含义 |
|---|---|
| Syntax 节点 | 稳定编号与精确文档链接，例如 `04/M7`、`10F` |
| Syntax 输出 | AOT 可以依赖的 canonical contract，不复制表层设计正文 |
| Syntax gate | 上游文档定义的晋级条件或状态记录入口 |
| AOT 责任 | lowering、ABI、artifact、runtime bridge、reachability 或 parity 工作 |
| AOT 计划 | 对应 AOT 文档和工作包 |
| AOT 产物/证据 | 必须存在的 schema、IR、thunk、map、manifest、测试矩阵 |
| AOT 状态 | 独立于 Syntax 状态的受控状态值 |

矩阵必须覆盖 Syntax README 中的全部十四个主题及每份设计的全部里程碑标题。未来新增 Syntax 节点时，缺少 AOT 行即视为计划同步失败。

### 3. AOT 主计划

更新 `00-current-state.md`、`01-design-principles.md` 和 `06-implementation-blueprint.md`：

- 当前状态明确列出未覆盖的 Syntax 上游和已有 baseline 的证据边界。
- 设计原则加入完整追踪、不自动晋级和阶段节点规则。
- 实施路线从固定 M1-M7 单线改为共享 AOT gate 加 Syntax node readiness；仍保持 foundation 到 convergence 的后端依赖顺序。

### 4. AOT 子系统计划

更新 `02-05`、`07-12` 的顶层计划，在每份文档增加“Syntax 上游追踪”表。表中只列该子系统真正消费的节点，并链接详细矩阵；不复制历史记录正文。

## AOT 状态模型

矩阵只使用以下状态：

- `blocked_by_syntax`：所需 Syntax contract 尚未达到其晋级门。
- `ready`：Syntax contract 可消费，但 AOT 尚未开始。
- `in_progress`：AOT 工作正在进行，未满足完整退出门。
- `partially_verified`：已有历史或子切片证据，但未覆盖该行完整产物和矩阵。
- `completed`：该行所有 AOT 产物与四路径验证均已满足。
- `not_applicable`：该 Syntax 节点没有机器后端行为；必须写明原因和仍需验证的 artifact/trim 边界，不能留空。

没有证据时禁止从 Syntax 的 `completed` 推导 AOT `completed`。现有 baseline 默认最多映射为 `partially_verified`，除非完成记录逐项证明该矩阵行的全部范围。

## 依赖和数据流

```text
Syntax README / design / milestone record
  -> canonical contract readiness
  -> AOT traceability row
  -> AOT subsystem work package
  -> artifact / ExecIR / ABI / runtime / reachability output
  -> leaf verification
  -> VM, binary-first, AOT C, AOT LLVM parity
  -> independent AOT status transition
```

上游节点变更时，先把受影响 AOT 行降为 `blocked_by_syntax` 或 `ready` 并重新审计，不允许保留与新 contract 冲突的完成结论。

## 验证

文档修订完成后执行以下检查：

1. Syntax 主题集合必须精确覆盖 `01..14`。
2. 阶段节点集合必须包含 `06A/06B`、`07A/07B`、`10R/10F/10C`。
3. 从十四份 Syntax 设计抽取的 M1-Mn 标题都必须在详细矩阵出现。
4. 每个 AOT 顶层子系统计划必须至少有一个反向 Syntax 链接，或明确说明仅为共享 backend foundation。
5. 所有 Markdown 相对链接必须指向存在的文件。
6. `git diff -- docs/plans/syntax` 必须为空于本任务写集；历史 AOT completion record 不得被修改。
7. 状态为 `completed` 的行必须链接完整 AOT 证据；否则降为 `partially_verified`。

## 非目标

- 不修改编译器、VM、AOT backend、测试代码或 artifact schema。
- 不重新评审或改写 Syntax 语义。
- 不把当前 AOT baseline 扩大解释为完整后端支持。
- 不删除、搬迁或合并既有 AOT 完成记录。
