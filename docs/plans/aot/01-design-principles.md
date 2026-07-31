# AOT 01：设计原则

## 唯一输入

AOT 接收 versioned `CompilationArtifact`：

```text
CanonicalTypeTable
TypeLayoutTable(target)
Symbol/CallableContractTable
SemanticIrModule(Place, Value, CFG, cleanup)
ModuleDependencyTable(ModuleIdentity)
NativeImportTable(FfiSignature)
Reflection/PreserveRoots
DebugMap
```

frontend token、旧 `%xxx` qualifier、source import spelling和runtime object representation都不是 backend contract。

## 核心原则

1. **语义先于优化**：move/drop/borrow/receiver/property lowering先在 Semantic IR 明确，backend不猜。
2. **布局由 target 决定但身份稳定**：TypeId跨target稳定；size/alignment/ABI class/LayoutHash按target计算。
3. **VM/AOT同义**：异常、cleanup、GC barrier、checked cast、bounds和native failure的可观察行为一致。
4. **证明驱动消检**：只有 CFG/range/escape/layout facts证明后才能去除检查；未证明时保留。
5. **显式边界**：GC、ownership、native、reflection和dynamic module调用都通过注册 contract/thunk连接。
6. **可诊断失败**：unsupported lowering携带 IR op、TypeId、source range和target，不静默回退到错误语义。

## 完整追踪原则

1. 每个 AOT work package 必须声明它消费的 Syntax 稳定节点，并链接[完整追踪矩阵](./syntax-contract-traceability.md)。
2. Syntax 里程碑完成只解除上游阻塞；AOT 的 `ready`、`partially_verified` 与 `completed` 必须由后端证据独立决定。
3. 每个 Syntax 稳定节点必须有且只有一个矩阵行；一个节点可投影到多个 AOT 子系统，但不能以多个局部记录重复宣称整行完成。
4. Syntax contract 改变 TypeId、Place、ABI、artifact 或 runtime capability 时，所有受影响 AOT 行先降级再验证，不允许沿用冲突的历史结论。
5. 没有直接机器行为的 Syntax 节点仍要记录 artifact/trim 边界；不得通过省略行表达 `not_applicable`。

## 分阶段依赖

- `06A`、`07A` 和 `10R` 是可独立晋级的基础设施节点，不证明 full-AOT 语义闭合。
- `10F` 只在 02 passing、03 layout、04 ownership/native lifetime 与 10R identity 就绪后冻结 FfiSignature ABI。
- `10C` 汇聚 08、09、11-14 的 owner provider contract；不能在这些 owner gate 之前创建 AOT 私有占位身份。
- `06B` 等待 08、10-14 目标 contract 与相关 AOT 投影；`07B` 又等待 06B 和 08-14 的 current-reference 纵向矩阵。

## 禁止模式

- 按 `Span`、`Unique`、`PoolHandle` 等名字分支。
- 由 C 字段宽度反推语言类型。
- 在 backend 内重新解析 module literal、property或 FFI descriptor字符串。
- 为了 AOT 方便改变 source-level ref/readonly/Drop 语义。
- 用“当前测试能跑”替代 artifact schema roundtrip 与 parity。

## 评审问题

每个 AOT change 必须回答：对应哪个 Syntax 节点、输入 fact 来自哪里、在 artifact 中如何版本化、VM 如何实现同一语义、失败路径如何清理、哪个测试同时覆盖 source/binary/AOT C/AOT LLVM，以及该证据使哪一行状态发生变化。
