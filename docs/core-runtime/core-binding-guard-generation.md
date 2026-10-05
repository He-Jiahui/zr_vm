---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_binding_guard.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/src/zr_vm_core/execution/execution_binding_guard.c
  - zr_vm_core/src/zr_vm_core/call_binding.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_binding_guard.c
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/03-guarded-caches.md
tests:
  - tests/core/test_ssa_guarded_caches.c
  - tests/acceptance/ssa-binding-guard-target-generation.md
doc_type: module-detail
---

# Binding guard VM target generation

`ZrCore_Execution_CheckBindingGuard` classifies a callsite witness using a
supplied frame generation, optional contract expectations and receiver facts.
Its return value and diagnostic distinguish expired generations from legal
shape misses. This local API does not dispatch the target or reset a binding.

## Independent generation checks

The binding's `generation` must match the caller-supplied `activeGeneration`.
For a resolved VM target with a non-null function and nonzero
`targetGeneration`, the function's `callBindingGeneration` must separately
match that recorded target generation. Caller and callee generation numbers
need not equal each other. A frame supplying its matching older generation
remains valid when the target witness also matches its own function.

The API does not fetch a newest global generation. A zero target generation
retains the existing unchecked-target convention; a null VM function retains
the existing target-missing check. Target pointers must already refer to valid
runtime metadata. This change adds no ownership or pointer-lifetime mechanism.

## Error priority

The check order is:

1. Invalid input and persistent contract validity.
2. Binding versus frame generation.
3. Optional module, signature and layout expectations, in their existing order.
4. Nonzero resolved VM target generation.
5. Receiver presence, shape and declared-slot bounds.
6. Existing target availability checks and successful hit reporting.

A stale VM target therefore returns `STALE_GENERATION` even when a receiver
shape ID or shape generation misses and the declared slot is in bounds. It
cannot become `SLOT_FALLBACK` or `SHAPE_MISS`. Contract expectation failures
still return their existing `CONTRACT_MISMATCH` classification before inspecting
that target generation. Other target availability precedence is unchanged.

For this VM stale result, diagnostic `bindingStatus` is
`ZR_CALL_BINDING_STALE_GENERATION`, `expected` is the recorded
`targetGeneration`, and `actual` is the observed function generation. This
matches the existing VM diagnostic in `ZrCore_CallBinding_Validate`; the guard's
former target-stale diagnostic used zero for both values. Target kind and
dispatch slot continue to identify the supplied binding. No ABI changes.

## Shape fallback and counters

A fresh VM witness still permits `SLOT_FALLBACK` for a shape miss when fallback
is allowed and the declared slot lies within the receiver's bounded slot table.
With fallback disabled it reports `SHAPE_MISS`. Matching shape facts and valid
target return `OK`.

Every failure, including fallback, increments `runtimeMissCount` once if a
cache entry is supplied and the count has not saturated at `UINT32_MAX`.
`OK` increments `runtimeHitCount` once with the same saturation rule. A miss
does not increment hits; a hit does not increment misses. Repeated calls report
the same classification while accumulating the corresponding count. Binding,
target metadata and receiver are not modified by this guard.

## Validation scope

The real Unity tests use local valid binding/function/prototype structures and
call the public guard. They cover both shape mismatch routes, allowed and
disabled fallback, repeated stale diagnostics, counter saturation, contract
priority, independent older caller generation and zero/null target controls.
They do not invoke the target callback or function.

Current C/H consumers of this API are the guarded-cache unit test; this finite
correction does not demonstrate interpreter dispatch integration, reload
leases, PIC bounds, GC movement, or completion of SSA milestone 03.03. Actual
build and execution evidence is recorded in the linked acceptance document.

## 2026-10-04 完整注释复审范围

本次只静态复核 guard 头与实现的43单元：5函数、3公开声明、3类型、17字段、8枚举项、2前置声明、头保护及4意义块。当前直接调用只有 guarded-cache Unity fixture：Check16处、Reset及Name各1处。Core库收集和fixture显式编译不等于生产派发消费者；本次未运行任何native、GC、reload或线程测试，历史验收记录仍仅支持其原始范围。

### 输入字段与借用约束

| 字段 | 用途与限制 |
| --- | --- |
| binding | 借用契约及目标；本函数不Invalidate或保活目标。 |
| cacheEntry | 可空；只改非原子饱和hit/miss，调用方独占写入。 |
| activeGeneration | 必须非零且与binding.generation一致；由调用方提供，不从frame读取。 |
| expectedModuleSignatureHash | 非零时比较moduleSignatureHash。 |
| expectedSignatureHash | 非零时比较signatureHash。 |
| expectedLayoutVersion | 非零时比较layoutVersion；失败数值仍记录hash。 |
| expectedLayoutHash | 非零时比较layoutHash。 |
| receiverPrototype | ownerTypeToken非零时必须存在；借用期间保持有效。 |
| receiverShapeId | 非零时比较当前shapeId。 |
| receiverShapeGeneration | 非零时比较当前shapeGeneration。 |
| allowSlotFallback | shape失配且声明槽仍有效时许可回退；不查询槽目标。 |

binding、目标VM函数、原型和可写输出须活到同步检查结束。没有分配、释放、GC根注册或target调用。ownerTypeToken分支不比较原型的类型token或layoutGeneration；native/AOT只核入口指针齐备，完整CallBinding_Validate的闭包代际检查不由本guard替代。

### 输出字段和八种分类

Diagnostic的result给guard分类，bindingStatus给底层状态，targetKind和dispatchSlot给当前绑定（无绑定时用NONE）；expected/actual按返回分支解释。所属代际为binding/input，VM目标代际为封存/当前函数；shape失败仍写shapeId，因此只generation失配时数值可相等；layoutVersion失败也写hash。输出不持有对象。

| result | 当前意义 |
| --- | --- |
| OK | 此检查通过；typed/virtual/interface允许TARGET_NONE延后选择。 |
| STALE_GENERATION | 所属代际或非零VM目标代际失配，优先于shape路由。 |
| CONTRACT_MISMATCH | 纯契约或非零expected见证失配。 |
| RECEIVER_TYPE_MISMATCH | owner存在时receiverPrototype为空；不证明完整类型token兼容检查。 |
| SHAPE_MISS | shape见证失配且不能按声明槽回退。 |
| SLOT_FALLBACK | 可重新按当前范围内声明槽查找；提前返回，不经过目标齐备switch，仍miss。 |
| INVALID_SLOT | 匹配shape路由中的声明槽越过nextVirtualSlotIndex。 |
| TARGET_MISSING | 输入/非零代际缺失或目标入口不齐备等，结合bindingStatus解读。 |

### Reset与完整静态范围

Reset保存contract、bindingLocation及kind/instructionIndex/memberEntryIndex/deoptId/argumentCount，再清空整个cache并恢复这些字段。PIC槽、目标、generation和计数清零；借用指针不Release。fixture断言其中部分保存字段及计数，不声称全部字段具有运行验证。Name返回静态字符串，未知值unknown，调用方不得释放。

43行台账仅授此source2完整静态注释审查范围。旧parser手动代际测试没有调用此guard，其错误反向证据已移除；CheckContract两条旧caller改为实现内真实callee操作的完整路径。除此不把旧表其他行或历史BUG/TODO升级为当前审查。
