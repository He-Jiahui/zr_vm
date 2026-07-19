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

## 禁止模式

- 按 `Span`、`Unique`、`PoolHandle` 等名字分支。
- 由 C 字段宽度反推语言类型。
- 在 backend 内重新解析 module literal、property或 FFI descriptor字符串。
- 为了 AOT 方便改变 source-level ref/readonly/Drop 语义。
- 用“当前测试能跑”替代 artifact schema roundtrip 与 parity。

## 评审问题

每个 AOT change 必须回答：输入 fact来自哪里、在 artifact 中如何版本化、VM如何实现同一语义、失败路径如何清理、哪个测试同时覆盖 source/binary/AOT。
