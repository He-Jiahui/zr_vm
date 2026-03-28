---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer.c
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/object.c
  - zr_vm_core/src/zr_vm_core/module.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer.c
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/object.c
  - zr_vm_core/src/zr_vm_core/module.c
plan_sources:
  - user: 2026-03-28 实现“ZR 全目标回归强化与 Field-Scoped using 语义计划”
  - .codex/plans/ZR 全目标回归强化与 Field-Scoped using 语义计划.md
tests:
  - tests/parser/test_parser.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/parser/test_compiler_features.c
  - tests/module/test_module_system.c
doc_type: module-detail
---

# Field-Scoped `using`

## 目标

字段级 `using` 让 `class` 和 `struct` 可以显式声明“这个字段属于实例生命周期管理范围”，而不是把所有 owning field 都自动升级为 cleanup 目标。

本轮已经打通的链路是：

1. parser 接受 `using var field: Type;`
2. semantic analyzer 为字段建立符号并登记 cleanup 元数据
3. compiler 把 managed-field 信息写入 prototypeData
4. module loader / runtime prototype 恢复 managed-field 表

## 语法

支持的字段声明形式：

```zr
struct HandleBox {
    using var handle: unique<Resource>;
}

class Holder {
    using var resource: shared<Resource>;
}
```

约束：

- `using` 只对显式标记字段生效
- `using` 只允许实例字段
- `static using var ...` 会在 semantic analyzer 和 compiler 两层都报错
- 语句级 `using expr;` / `using (expr) { ... }` 继续保留原有 block-scope 语义，不与字段节点复用

## AST 与语义层

`SZrStructField` 和 `SZrClassField` 都新增 `isUsingManaged` 标记，用来区分普通 field 和显式托管 field。

语义层的 deterministic cleanup plan 现在不再只覆盖语句级 `using`，还会记录字段生命周期来源：

- `ZR_DETERMINISTIC_CLEANUP_KIND_BLOCK_SCOPE`
  - 原有语句级 `using`
- `ZR_DETERMINISTIC_CLEANUP_KIND_INSTANCE_FIELD`
  - `class` 实例字段 cleanup
- `ZR_DETERMINISTIC_CLEANUP_KIND_STRUCT_VALUE_FIELD`
  - `struct` 值字段 cleanup

每个 cleanup step 还额外记录：

- `ownerRegionId`
  - 字段所属生命周期区域
- `declarationOrder`
  - 字段声明顺序，供后续 cleanup 排序和运行时策略使用

## 编译器与原型元数据

编译器把字段生命周期信息收敛到类型成员元数据，并继续序列化进 prototypeData。当前写入的关键字段包括：

- `isUsingManaged`
- `ownershipQualifier`
- `callsClose`
- `callsDestructor`
- `declarationOrder`

这批字段同时存在于：

- `SZrTypeMemberInfo`
- `SZrCompiledMemberInfo`

因此模块写出和读取 prototypeData 时，可以无歧义地区分普通字段和 field-scoped `using` 字段。

## 运行时原型恢复

`SZrObjectPrototype` 新增 managed-field 表：

- `managedFields`
- `managedFieldCount`
- `managedFieldCapacity`

模块加载 prototypeData 时会做两件事：

1. 恢复结构体字段偏移信息
2. 通过 `ZrObjectPrototypeAddManagedField(...)` 恢复 managed-field 元数据

这样运行时后续如果要做实例释放、字段覆盖或 struct value cleanup，就可以直接从 prototype 读取字段生命周期描述，而不需要重新猜测字段语义。

## 当前验证覆盖

已补的回归点：

- parser
  - `using var` 字段在 `struct` / `class` 中可解析
- semantic
  - 字段符号进入 semantic table
  - cleanup plan 能区分 instance field 和 struct value field
  - `static using` 产出 `static_using_field` 诊断
- compiler
  - prototypeData 持久化 managed-field 元数据
  - `static using` 在编译阶段被拒绝
- module/runtime metadata
  - prototypeData 重新加载后可恢复结构体字段偏移和 managed-field 表

建议回归命令：

```powershell
wsl bash -lc "cd /mnt/d/Git/Github/zr_vm_mig/zr_vm && ./build/codex-wsl-gcc-debug/bin/zr_vm_parser_test"
wsl bash -lc "cd /mnt/d/Git/Github/zr_vm_mig/zr_vm && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test"
wsl bash -lc "cd /mnt/d/Git/Github/zr_vm_mig/zr_vm && ./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_features_test"
wsl bash -lc "cd /mnt/d/Git/Github/zr_vm_mig/zr_vm && ./build/codex-wsl-gcc-debug/bin/zr_vm_module_system_test"
```
