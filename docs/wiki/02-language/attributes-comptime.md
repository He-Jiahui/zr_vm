---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compile_tool.h
  - zr_vm_parser/include/zr_vm_parser/attribute_contract.h
  - zr_vm_parser/include/zr_vm_parser/declaration_transform_contract.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_declaration_transform.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_transaction.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_descriptor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_declaration_transform.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_transaction.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/parser-and-semantics/compile-time-typed-generation.md
  - docs/plans/syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md
tests:
  - tests/compileTime/test_compile_time_execution.c
  - tests/compileTime/test_declaration_transform_contract.c
  - tests/fixtures/projects/syntax_reference_v1/src/compile_time_and_attributes.zr
doc_type: language-reference
---

# Attribute 与编译期执行

ZR attribute 使用 `#...#` 包围的调用形状。attribute 本身是 metadata，不会因为名字看起来
像函数就自动在运行时执行；descriptor role、target、retention 和 provider phase 决定它
何时求值、写入哪份产物以及是否保留到 runtime。

## 语法形状

```ebnf
attribute       = "#", qualified-name, ["(", [argument-list], ")"], "#" ;
argument-list   = argument, {",", argument} ;
argument        = [identifier, ":"], expr ;
```

attribute 必须紧邻它标注的 declaration 或 member。未知 attribute 可以先进入 AST，但 binding
阶段会因为没有 registered role 而失败。重复性、参数命名和目标种类由
`ZrLibAttributeRoleDescriptor`/attribute contract 检查。

当前常用 role：

| attribute | 目标 | retention | 作用 |
| --- | --- | --- | --- |
| `zr.compile.conditional(name)` | 直接绑定的 void function | compile-time | 按 build feature 删除调用点 |
| `zr.compile.declarationTransform` | `comptime fn` | compile-time | 读取 declaration view 并返回 typed Patch |
| `zr.testing.test` | 测试函数 | Test phase | 生成 TestManifest entry |
| `zr.testing.case(...)` | 测试函数 | Test phase | 添加可序列化参数 case |
| `zr.testing.skip(reason)` | 测试函数 | Test phase | 生成 skip reason |

## compile-time block 和阶段

```zr
comptime if (import("zr.compile").build.feature("simd")) {
    fn dot(v: Vector4): float { return dotSimd(v); }
} else {
    fn dot(v: Vector4): float { return dotScalar(v); }
}

comptime {
    import("zr.compile").assert(sizeOf<int>() == 4, "requires 32-bit int");
}
```

所有 `comptime if` 分支先解析成 AST；只有选中的分支进入 name binding、类型推断和 lowering。
未选分支仍须语法可恢复，但其中未定义的 runtime 名称不会造成普通 unresolved symbol。
`comptime {}` 仅在 module scope 产生诊断/求值，不直接生成 runtime statement。

CompileTool 的阶段顺序是 `BuildFacts -> Signature -> Expansion -> Layout -> LateCheck`：

1. BuildFacts 锁定 `.zrp` feature、profile 和依赖摘要；
2. Signature 建立 declaration/type/function view；
3. Expansion 执行 transform 并准备 Patch；
4. Layout 重算字段布局、GC scan、interface relation 和 AOT metadata；
5. LateCheck 统一诊断并写入 artifact。

## effect 和隔离

| 执行上下文 | 允许 effect |
| --- | --- |
| `PURE_VALUE` | 仅 `PureValue` |
| `CHECK` | `PureValue` + `Diagnostic` |
| `DECLARATION_TRANSFORM` | `PureValue`、`Diagnostic`、`Patch` |

编译期 predicate 不能读取时钟、随机数、环境或文件系统；外部输入必须通过锁定的 build
dependency artifact。每次调用在 `CompileToolExecutionScope` 中运行，结束时恢复 provider
binding、别名、临时对象和 private helper。runtime 代码调用 compile-only export 会得到
`compiletool.phase_mismatch`。

## conditional call-elision

```zr
#zr.compile.conditional("trace")#
fn trace(message: string): void {
    import("zr.system.console").printLine(message);
}

fn update(): void {
    trace("tick");
}
```

feature 关闭时，compiler 删除 bound call 和全部参数 lowering；但仍执行 name lookup、访问
检查、overload 选择和 type-check。role 只接受直接静态绑定、void 返回、无 virtual/interface/
override、无 ref/out 参数和无 callable/dynamic conversion 的函数。要删除整个 declaration，
使用 `comptime if`，而不是 conditional attribute。

## declaration transform

```zr
#zr.compile.declarationTransform#
pub comptime fn addMarker(target: zr.compile.declaration.Struct): zr.compile.declaration.Patch {
    let field = init zr.compile.declaration.GeneratedField(
        name: "generated",
        type: typeid(bool),
        visibility: zr.compile.declaration.Visibility.public,
        mutability: zr.compile.declaration.Mutability.let
    );
    return init zr.compile.declaration.Patch(
        target: target.symbolId,
        additions: [field]
    );
}
```

transform 签名必须是 ordinary、non-generic、non-async `comptime fn`，只有一个 immutable
declaration view 参数，返回 `declaration.Patch`。参数不能有默认值或 `ref/out` passing mode。
当前 published variant 只有 `GeneratedField`；GeneratedMethod/Property/Type 仍属于 planned
surface。

Patch validator 的顺序是：检查 target/symbol 和 expansion round；检查集合指针/数量上限；
检查 field name、TypeId、visibility、mutability 和 collision；检查 interface/attribute
重复；最后检查 diagnostic target/message。transaction 在 detached arrays 中完成 metadata、
symbol、layout、source-map 准备，全部成功才交换指针；任一步失败都恢复原数组长度、
`nextSymbolId` 和 metadata pointer。

## 预算、缓存和确定性

fuel、call depth、heap bytes、aggregate count、generated declaration count 和 diagnostic count
均有上限。预算失败不会部分提交 usage，错误名为 `comptime.budget_exceeded`。cache key 包含
provider/module、source digest、调用位置、context、typed args、版本、package/lock hash、
artifact entry 和 public contract；导入快照前验证 magic、大小、SHA-256 和排序，失败保留旧
cache。

## C 接口

CompileTool descriptor 查询：

```c
const SZrParserCompileToolModuleDescriptor *module =
    ZrParser_CompileTool_FindModule("zr.compile");
const SZrParserCompileToolCallableDescriptor *callable =
    ZrParser_CompileTool_FindCallable(module, ZR_PARSER_COMPILE_TOOL_ROLE_BUILD_FEATURE);
TZrUInt64 hash = ZrParser_CompileTool_ComputePublicContractHash(module);
```

声明 transform 的执行入口是 `ZrParser_CompileTimeDeclaration_Execute`；Patch 校验入口是
`ZrParser_DeclarationPatch_Validate`。view、AST、字符串和数组均借用 compiler state，不能
保存到下一代 module。build dependency 使用 `ZrParser_CompileToolArtifact_OpenBuildDependency`
和 `..._Close` 成对管理；lock/version/hash 不匹配时不得执行 provider。

## 错误边界

- compile export 出现在 runtime scope：`compiletool.phase_mismatch`；
- feature 未在 manifest 声明：typed evaluator error，而不是静默 false；
- diagnostic message 为空或 target 非法：Patch 失败；
- 生成字段重名、TypeId 无效或超过 10000 个 addition：transaction 回滚；
- transform 递归触发自身或跨阶段读取 view：`recursive_transform`/`phase_cycle`；
- provider public contract、package hash 或 entry hash 失配：binding 不建立。

另见 [`zr.compile`](../03-modules/compile.md)、[`zr.compile.declaration`](../03-modules/compile-declaration.md)
和[编译器流水线](../08-compiler-pipeline.md)。
