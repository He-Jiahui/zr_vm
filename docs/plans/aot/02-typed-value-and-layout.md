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
