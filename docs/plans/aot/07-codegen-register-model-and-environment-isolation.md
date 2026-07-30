# AOT 07：Typed Register、Call Frame 与 Environment

## 目标

建立不依赖universal boxed value的typed execution model，同时保持GC、ref provenance、异常cleanup与debug可见性。register是ExecIR value carrier，不是源码变量或无类型机器槽。

## 模型

```text
VirtualRegister {
  valueId;
  canonicalTypeId;
  registerClass: int | float | ref | address | aggregateHandle;
  gcRootKind;
}

FrameLayout {
  parameters; returns; locals; spillSlots;
  gcStackMap; refProvenanceMap; cleanupState;
}
```

- scalar使用typed register；aggregate按ABI选择register tuple、caller destination或stack slot。
- address-taken local与Place projection具有稳定storage，不得因临时register复用破坏ref。
- GC ref在safepoint可枚举；native/raw pointer不能误标为GC root。
- environment/capture有独立layout与generation，不能把module globals、closure capture和frame local混成一个字典。
- call frame由Canonical CallableContract计算passing/return/receiver effect；callsite marker已在frontend验证。

## 工作包

1. register class与frame schema roundtrip。
2. typed scalar local/loop/branch parity。
3. aggregate、ref/out与ref-return。
4. closure/module environment隔离。
5. safepoint stack map、exception cleanup与debug variable location。
6. 删除untyped/boxed fallback或把明确dynamic boundary隔离为thunk。

## 验收

覆盖递归、深层loop、spill、address-taken local、nested closure、module reload generation、GC compaction、throw/finally与native callback。performance guardrail分别统计boxing、value construction、spill与thunk命中。

## 完成记录

[2026-07-19 typed register/codegen baseline](./07-codegen/2026-07-19-typed-register-codegen-baseline.md) 记录已存在的typed scalar/loop能力；aggregate、ref、environment与新artifact contract仍是open work。

[2026-07-30 ExecIR frame ABI verifier](./07-codegen/2026-07-30-execir-frame-abi-verifier.md) 完成 A7.2 的
fail-closed frame sidecar 校验与 retained-frame manifest/count 前置切片；CallableContract 派生的 receiver、
`in/ref/out`、return、spill 和 address-taken ABI 仍开放。

## 实施包与证据

1. **A7.1 register schema**：为每个register class定义合法Canonical Type/representation、copy/move规则与serialization；invalid class/type pair在ExecIR verifier失败。
2. **A7.2 frame ABI**：从CallableContract生成parameter/receiver/return/spill/address-taken slots，覆盖`in/ref/out`、aggregate destination和nested callable return。
3. **A7.3 environment**：分别建立module globals、closure captures、async/task state和function frame layout；缓存键包含ModuleIdentity/generation，禁止跨environment复用slot。
4. **A7.4 GC/debug maps**：每个safepoint列出GC roots、non-GC address、ref provenance和debug variable location；spill/register allocation后重写map。
5. **A7.5 optimization guardrail**：比较boxed fallback、typed register、spill和thunk计数，任何zero-frame/elision必须保留exception/GC observable behavior。

现有证据可从`tests/core/test_aot_gc_root_frame.c`、`tests/acceptance/2026-07-06-aot-07-s6-gc-reference-register-stage-acceptance.md`和typed scalar acceptance系列延续。目标新增aggregate/ref/out/closure/module reload/compacting GC/throw-finally组合。

退出条件：所有live GC value在safepoint可枚举；address-taken Place在生命周期内地址稳定；同一ExecIR在C/LLVM frame layout golden中ABI等价；dynamic boundary是显式thunk而非全局boxed回退。
