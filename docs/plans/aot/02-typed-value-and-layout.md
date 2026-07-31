# AOT 02：Canonical TypeRef、Typed Value 与 Layout

## Contract

`CanonicalTypeId` 表达结构身份，`TypeUse`表达使用点能力，`TypeLayoutId`表达target布局。三者不可合并为字符串名或单个 runtime type pointer。

```text
TypedValue {
  typeId;
  representation: scalar | aggregate | ref | gcRef | owner | fatRef;
  location: virtualRegister | stackSlot | address | constant;
}

TypeLayout {
  size; alignment; abiClass;
  fields[]; gcPointerMap; ownershipMap;
  copyKind; moveKind; dropKind; gcScanKind;
  layoutHash;
}
```

## 规则

- struct 按 Canonical TypeLayout value construction，`init T(...)`降低为 destination-first construct；不产生隐式 GC allocation。
- class/reference以GC ref表示；resource class/Unique/Shared/Weak按 ownership contract表示。
- `ref T`是address + region/provenance语义；Span/PoolRef等 fat/ref-like value的字段布局由注册 TypeLayout决定。
- readonly不必改变物理layout，但必须保留在 TypeUse/receiver/call contract中，防止调用writable member。
- generic instantiation在substitution后计算closed layout；open generic不进入machine lowering。
- native-visible layout必须显式声明ABI stability并把target、packing、field ABI纳入hash。
- `GcFree`是closed layout证明；old generation、长期池化或“通常没有指针”都不能代替pointer map。

## 里程碑

1. Canonical type/artifact roundtrip。
2. scalar、tuple、array、struct、ref、owner、fat ref layout golden。
3. aggregate copy/move/drop classification。
4. C与LLVM ABI classification parity。
5. `.zro` dependency layout hash mismatch diagnostic。

## Syntax 上游追踪

| Syntax 节点 | 本计划消费的稳定输入 | 本计划退出责任 |
|---|---|---|
| [01/M1、M4](../syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md) | Canonical TypeId/TypeUse 与 versioned type section | machine value 分类、type/layout roundtrip 与 mismatch 拒绝 |
| [03/M1、M3-M5](../syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md) | struct/ref-like/fat-ref TypeLayout、copy/GC/owner maps | target size/alignment/offset/hash、aggregate ABI、no-boxing construct |
| [04/M1-M4、M7](../syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md) | Unique/Shared/Weak/GcBox representation 与 ownership/GC maps | copy/move/drop class、root/barrier map 与 domain-aware layout |
| [05/M2、M4](../syntax/2026-07-18-05-property-unified-ast-design.md) | explicit field layout 与 ref-return Place type | field/ref-get ABI classification，不生成隐藏 backing field |
| [08/M1、M3-M4](../syntax/2026-07-19-08-reflection-library-type-system-design.md) | descriptor category、preserved layout 与 construction contract | reflection-visible layout 与 machine layout 使用同一 TypeId/hash |
| [09/M3-M4](../syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md) | slab GcScanKind、StableSlot 与 pool layout schema | GcFree/Mapped/Barriered map 与 artifact hash |
| [10F/M3](../syntax/2026-07-19-10-native-ffi-module-package-design.md) | FfiSignature 所引用的 closed TypeLayout | target ABI class、unsupported ref-like/owner marshalling 拒绝 |
| [11/M4](../syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md) | typed patch 生成的普通 canonical declaration | generated type 与 source type 走同一布局管线 |
| [12/M2](../syntax/2026-07-20-12-async-task-job-scheduler-design.md) | Task private frame TypeIds 与 field maps | task frame layout、root map 与 backend ABI 一致 |
| [13/M3](../syntax/2026-07-20-13-iterator-enumerator-yield-design.md) | Iterator private frame TypeIds 与 field maps | iterator frame layout、root map 与 backend ABI 一致 |

逐节点状态和证据边界见[完整追踪矩阵](./syntax-contract-traceability.md)；Syntax 完成不自动晋级本计划。

## 完成记录

[2026-06-24 typed layout baseline](./02-type-layout/2026-06-24-typed-layout-baseline.md) 证明已有布局基础；Canonical TypeRef与全部目标类别仍需按本文 gate 收敛。

## 实施与晋级

| 子阶段 | 输入 | 交付 | 失败/边界 |
|---|---|---|---|
| A2.1 canonical value classes | syntax 01 TypeNode/TypeUse schema | scalar/ref/gcRef/owner/fatRef/aggregate分类表 | open generic、invalid qualifier组合拒绝 |
| A2.2 closed layout | target triple、field TypeIds、layout policy | size/alignment/field offsets、GC/owner maps、copy/move/drop kind | recursive-by-value、overflow、unsupported alignment |
| A2.3 ABI classification | closed TypeLayout + CallableContract | parameter/return ABI class、destination-first aggregate contract | target不支持的by-value/ref-like ABI |
| A2.4 artifact compatibility | TypeId/LayoutHash/public contract | `.zro` roundtrip与dependency hash check | stale target/schema/layout mismatch |

验证入口包括`tests/parser/test_canonical_type_graph.c`、`tests/core/test_type_layout_metadata_contracts.c`、`tests/core/test_type_layout_inline_copy.c`和`tests/acceptance/2026-06-20-aot-m1-type-layout-metadata.md`。目标新增用例必须覆盖readonly/ref struct/Span、resource owner、union/tuple、generic closed instance以及GcFree/GcMapped/GcBarriered。

退出证据：同一closed TypeId在writer/reader/VM/AOT C/AOT LLVM得到一致LayoutHash和field/GC map；错误layout在link/load前稳定失败；value construction profile证明普通`init struct`无boxing或GC allocation。
