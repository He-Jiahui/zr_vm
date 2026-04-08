---
related_code:
  - zr_vm_parser/src/zr_vm_parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler_struct.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_support.c
  - zr_vm_core/src/zr_vm_core/module_prototype.c
  - zr_vm_core/src/zr_vm_core/object.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler_struct.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_support.c
  - zr_vm_core/src/zr_vm_core/module_prototype.c
  - zr_vm_core/src/zr_vm_core/object.c
plan_sources:
  - user: 2026-04-08 Rust-First Ownership / GC 分层设计
  - .codex/plans/Rust-First Ownership  GC 分层设计.md
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_compiler_features.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/module/test_module_system.c
  - tests/fixtures/projects/classes/src/math.zr
doc_type: module-detail
---

# Owned Field Lifecycle

## 目标

ownership world 里的字段生命周期现在直接写在字段类型上，而不是再额外叠一层字段级 `%using` 语法。

当前面向用户的规则是：

- `var field: %unique T`
- `var field: %shared T`
- 语句或 block 级 `%using` 仍保留为 lifetime fence
- 字段级 legacy `%using` 语法已经移出 public surface

## 源级写法

当前推荐写法：

```zr
%owned class Bag {
    var value: %unique Resource;
    var cache: %shared Cache;
}

struct HandleBox {
    var handle: %unique Resource;
    var count: int;
}
```

迁移方向很直接：

- 旧字段级 lifecycle 标记迁到字段类型本身
- block cleanup 继续使用语句级 `%using`
- parser 对 legacy field-scoped `%using` 产出迁移诊断，不再把它当有效字段 surface

## Parser 与语义层

parser 现在把字段生命周期分成两个世界：

- 字段类型里的 ownership qualifier
  - `%unique`
  - `%shared`
  - 其他非 owner qualifier 仍只表示类型能力，不自动变成 owner teardown surface
- 语句级 `%using`
  - 继续表示 block / scope 级 close fence

这意味着字段语义不再依赖“字段上是否额外写了 `%using` 前缀”，而是直接依赖字段类型。

semantic analyzer 侧当前做两件事：

1. 始终为字段注册正常的 field symbol。
2. 当字段类型是 direct `%unique` 或 `%shared` 时，登记 deterministic cleanup metadata。

cleanup plan 里仍保留原来的区分：

- `ZR_DETERMINISTIC_CLEANUP_KIND_STRUCT_VALUE_FIELD`
- `ZR_DETERMINISTIC_CLEANUP_KIND_INSTANCE_FIELD`

因此 language server / semantic metadata 仍然能区分：

- struct value field teardown
- class instance field teardown

只是触发条件已经从“显式 field-scoped `%using`”改成“字段类型本身是 owner”。

## Prototype Metadata 与 Runtime 恢复

编译器继续把字段生命周期信息序列化进 prototype metadata，关键位包括：

- `ownershipQualifier`
- `callsClose`
- `callsDestructor`
- `declarationOrder`

对当前 direct owner field 来说：

- `isUsingManaged` 不再代表 public surface
- `ownershipQualifier` 成为恢复 managed-field 行为的主入口

module prototype materialization 恢复 managed field 时，当前以 `ownershipQualifier != NONE` 为主判据，因此 direct owner field 可以稳定恢复为 runtime managed-field table。

这让后续行为继续统一：

- field teardown
- owner field override / replace
- struct value cleanup
- class instance cleanup

## 当前边界

这轮收敛后的边界是：

- direct `%unique/%shared` field 是正式 surface
- 语句级 `%using` 继续保留
- field-scoped `%using` 只剩迁移诊断，不再是语言设计目标

仓库内部仍保留少量 legacy metadata 位用于兼容已存在的编译产物结构，但新源码路径不再依赖它们表达字段 owner 生命周期。

## 验证覆盖

当前已对齐的验证包括：

- parser
  - 直接 owner field 可解析
  - legacy field-scoped `%using` 报迁移诊断
- compiler / prototype metadata
  - direct `%unique/%shared` field 写入 ownership metadata
  - legacy field-scoped `%using` 不再作为新 surface 写入
- language server semantic metadata
  - direct owner field 会登记 cleanup plan
  - struct/class field cleanup kind 仍可区分
- module runtime metadata
  - prototypeData roundtrip 后仍可恢复 managed field 表
- project fixture
  - `tests/fixtures/projects/classes/src/math.zr` 已迁到 direct owner field 语法
