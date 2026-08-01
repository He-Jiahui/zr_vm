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

## Syntax 上游追踪

| Syntax 节点 | 本计划消费的稳定输入 | 本计划退出责任 |
|---|---|---|
| [01/M3、M5](../syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md) | validated SemIR value/Place identity 与 canonical consumer projection | register/frame 只承载 ValueId/TypeId/Place，不从 source local 拼写重建 |
| [02/M1-M6](../syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md) | passing form、receiver effect、region/provenance 与 artifact callable | parameter/receiver/return/address-taken slots 和 debug provenance map |
| [03/M1、M3-M5](../syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md) | aggregate/ref-like/Span TypeLayout 与 lifetime | aggregate destination、stable address、root/ref map 与 FFI frame ABI |
| [04/M3-M7](../syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md) | owner reborrow、domain safepoint 与 artifact maps | owner slots、safepoint roots、handoff/transport frame identity |
| [05/M3-M4](../syntax/2026-07-18-05-property-unified-ast-design.md) | receiver/access call 与 ref-return Place | property call frame、ref-get address stability 与 region map |
| [08/M4](../syntax/2026-07-19-08-reflection-library-type-system-design.md) | exact generic/runtime construction context | invoke/constructor thunk frame 与 exact debug generic context |
| [10F/M3](../syntax/2026-07-19-10-native-ffi-module-package-design.md) | FfiSignature parameter/return/marshaller contract | native/callback frame ABI 与 GC/ref/owner slot map |
| [11/M4](../syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md) | generated function 的普通 callable/TypeLayout | generated/source function 使用相同 frame derivation/verifier |
| [12/M2、M4、M6](../syntax/2026-07-20-12-async-task-job-scheduler-design.md) | Task frame、domain scheduler 与 artifact/debug rows | state/parameter/root/safepoint frame layout 与 generation identity |
| [13/M2-M4](../syntax/2026-07-20-13-iterator-enumerator-yield-design.md) | iterator state/frame/resume contract | iterator locals、roots、resume/dispose ABI 与 debug locations |
| [14/M3](../syntax/2026-07-20-14-test-function-harness-design.md) | sync/async test function 与 runner isolation | test call frame 复用普通 ABI，不创建 hidden main/frame model |

逐节点 readiness 与完整退出 evidence 见[追踪矩阵](./syntax-contract-traceability.md)。

## 完成记录

[2026-07-19 typed register/codegen baseline](./07-codegen/2026-07-19-typed-register-codegen-baseline.md) 记录已存在的typed scalar/loop能力；aggregate、ref、environment与新artifact contract仍是open work。

[2026-07-30 ExecIR frame ABI verifier](./07-codegen/2026-07-30-execir-frame-abi-verifier.md) 完成 A7.2 的
fail-closed frame sidecar 校验与 retained-frame manifest/count 前置切片；CallableContract 派生的 receiver、
`in/ref/out`、return、spill 和 address-taken ABI 仍开放。

[2026-07-30 frame TypeLayout closure verifier](./07-codegen/2026-07-30-frame-type-layout-closure-verifier.md) 完成
A7.2B 的 canonical aggregate TypeLayout 解析、schema/hash、类型身份与 payload shape 全树闭包校验；不新增
manifest schema，CallableContract slot 派生与完整 A7.2 仍开放。

[2026-07-30 complete-frame parameter identity verifier](./07-codegen/2026-07-30-complete-frame-parameter-identity-verifier.md)
完成 A7.2C 的完整 frame table parameter marker 数量与 canonical slot identity 校验，同时保留 zero-frame 与
sparse typed-register hybrid；parameter direction/type、receiver/return/spill 派生与完整 A7.2 仍开放。

[2026-08-01 constructor bitmap layout verifier](./07-codegen/2026-08-01-constructor-bitmap-layout-verifier.md)
完成 A7.2D 的 constructor receiver 初始化 bitmap 尾布局、canonical TypeLayout 身份/shape 与全部物理 slot
envelope 校验，并在 code stripping 前拒绝不可达 malformed owner；field-init dataflow、return/destination ABI 与
完整 A7.2 仍开放。

[2026-08-01 receiver role frame verifier](./07-codegen/2026-08-01-receiver-role-frame-verifier.md)
完成 A7.2E 的 patch-38 canonical receiver role 消费、完整 identity、slot 0 parameter 与 complete/sparse/zero
frame 一致性校验；不从名称重建缺失 role，parameter direction/type、return/destination、spill/address-taken 与
完整 A7.2 仍开放。

[2026-08-01 parameter binding identity verifier](./07-codegen/2026-08-01-parameter-binding-identity-verifier.md)
完成 A7.2F 的 producer-order parameter prefix、set-wide identity availability、stack slot 与 SymbolId/PlaceId
唯一性校验，同时保留无 typed-local table 与全零 legacy tuple 兼容；direction/type equality、default、
return/destination、spill/address-taken 与完整 A7.2 仍开放。

[2026-08-01 ExecIR parameter layout projection](./07-codegen/2026-08-01-execir-parameter-layout-projection.md)
完成 A7.2G 的 verified parameter prefix 到内部 ExecIR parameter layout 投影，并让 MethodInfo signature 按
runtime parameter slot 消费该投影；typed-local 是权威输入，legacy metadata 仅在 count 完全相等时按索引复制，
不完整 metadata 保持 unknown；passing direction/default、return/destination 与完整 A7.2 仍开放。

[2026-08-01 value-SemIR parameter layout consumption](./07-codegen/2026-08-01-value-semir-parameter-layout-consumption.md)
完成 A7.2H 的 generic/shared inline-struct `CALL_TYPED` 参数布局消费：callee 通过 ExecIR flat index 解析，
shared-method 选择仅接受 exact-count、无 receiver role、已投影 OBJECT/ARRAY 且 caller VALUE slot 容量合法的
参数表；unknown/mismatch fail closed 到普通 inline-struct 路径，不新增 public/manifest schema。receiver、
direction/default origin、return/destination、spill/address-taken 与完整 A7.2 仍开放。

[2026-08-01 receiver-aware typed-call layout consumption](./07-codegen/2026-08-01-receiver-aware-typed-call-layout-consumption.md)
完成 A7.2I 的 index-0 canonical receiver 消费：receiver 已包含在 `CALL_TYPED.argumentCount`，继续使用
`operand0 + 1 + argumentIndex` 参数窗口；unknown/组合/错位 role 与 unknown receiver TypeRef fail closed，
不改变 runtime/dictionary/public/manifest schema。实例方法 producer、direction/default origin、return/
destination、spill/address-taken 与完整 A7.2 仍开放。

## 实施包与证据

1. **A7.1 register schema**：为每个register class定义合法Canonical Type/representation、copy/move规则与serialization；invalid class/type pair在ExecIR verifier失败。
2. **A7.2 frame ABI**：从CallableContract生成parameter/receiver/return/spill/address-taken slots，覆盖`in/ref/out`、aggregate destination和nested callable return。
3. **A7.3 environment**：分别建立module globals、closure captures、async/task state和function frame layout；缓存键包含ModuleIdentity/generation，禁止跨environment复用slot。
4. **A7.4 GC/debug maps**：每个safepoint列出GC roots、non-GC address、ref provenance和debug variable location；spill/register allocation后重写map。
5. **A7.5 optimization guardrail**：比较boxed fallback、typed register、spill和thunk计数，任何zero-frame/elision必须保留exception/GC observable behavior。

现有证据可从`tests/core/test_aot_gc_root_frame.c`、`tests/acceptance/2026-07-06-aot-07-s6-gc-reference-register-stage-acceptance.md`和typed scalar acceptance系列延续。目标新增aggregate/ref/out/closure/module reload/compacting GC/throw-finally组合。

退出条件：所有live GC value在safepoint可枚举；address-taken Place在生命周期内地址稳定；同一ExecIR在C/LLVM frame layout golden中ABI等价；dynamic boundary是显式thunk而非全局boxed回退。
