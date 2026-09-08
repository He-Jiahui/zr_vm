---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compile_tool.h
  - zr_vm_parser/include/zr_vm_parser/declaration_transform_contract.h
  - zr_vm_parser/include/zr_vm_parser/attribute_contract.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_declaration_transform.c
  - zr_vm_parser/src/zr_vm_parser/declaration_transform_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_transaction.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_interfaces.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_declaration_transform.c
  - zr_vm_parser/src/zr_vm_parser/declaration_transform_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_transaction.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/parser-and-semantics/compile-time-typed-generation.md
  - docs/plans/syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md
tests:
  - tests/compileTime/test_declaration_transform_contract.c
  - tests/compileTime/test_compile_time_execution.c
  - tests/compileTime/test_compile_time_declaration_patch_transaction_cases.h
  - tests/compileTime/test_compile_time_decorator_shape_retention_cases.h
  - tests/parser/test_compile_tool_project_import.c
  - tests/fixtures/projects/syntax_reference_v1/src/compile_time_and_attributes.zr
  - tests/acceptance/2026-08-01-syntax-11-m4-patch-transaction.md
  - tests/acceptance/2026-08-01-syntax-11-m4-generated-source-map.md
doc_type: module-detail
---

# `zr.compile.declaration`

**状态：`current`（第一版 GeneratedField/diagnostic/interface/attribute surface）；
CompileTool N3 provider，public contract `zr.compile.declaration/v2`，hash
`fnv1a64:b4e4667f4100e100`。**

该模块为 declaration transform 提供不可变视图和类型化 Patch。它与
[`zr.compile`](compile.md) 同属 compiler-only phase，不能在 runtime `import` 中使用。
当前公开生成 variant 只有 `GeneratedField`；GeneratedMethod、GeneratedProperty 和
GeneratedType 的名字虽然保留在设计文档中，但 descriptor/validator 尚未发布，不能写入
生产源码。

## 最小源代码

```zr
let declaration = import("zr.compile.declaration");

#zr.compile.declarationTransform#
pub comptime fn deriveMarker(
    target: declaration.Struct
): declaration.Patch {
    let marker = init declaration.GeneratedField(
        name: "generated",
        type: typeid(bool),
        visibility: declaration.Visibility.public,
        mutability: declaration.Mutability.let
    );
    return init declaration.Patch(
        target: target.symbolId,
        additions: [marker]
    );
}

#deriveMarker#
pub struct Meter { pub let value: int; }
```

transform 是普通 `comptime fn`，attribute 只负责把它标为
`DECLARATION_TRANSFORM` role。编译器在目标 declaration 的 Expansion phase 调用它，
传入只读 view；返回的 Patch 经过完整验证后才重新进入 symbol、layout、metadata、AOT
reachability 和 LSP projection。

## 类型目录

### Signature phase 的不可变 view

| 名称 | qualified name | 用途 |
| --- | --- | --- |
| `SymbolId` | `zr.compile.declaration.SymbolId` | 稳定引用一个 semantic symbol。 |
| `DeclarationView` | `zr.compile.declaration.DeclarationView` | 通用 declaration 快照。 |
| `TypeView` | `zr.compile.declaration.TypeView` | 带 type capabilities、fields、methods、properties、interfaces 的类型快照。 |
| `Class` / `Struct` | `...Class` / `...Struct` | 将 target 限制为相应 declaration kind。 |
| `Function` / `Field` / `Method` / `Property` / `Parameter` | 同名 qualified name | 对应成员或 callable 的目标约束。 |

所有 view 的 descriptor 都标记 `immutableView=true`，minimum phase 为 `Signature`。view
不会暴露 writable AST、token/source buffer、compiler pointer、runtime object、native
pointer 或已有方法 body；跨 phase 使用低于字段 minimum phase 的数据会得到
`ZR_PARSER_DECLARATION_PATCH_ERROR_PHASE_CYCLE`。

### Expansion phase 的 Patch 数据

| 类型 | 当前字段/成员 | 备注 |
| --- | --- | --- |
| `Patch` | `target: SymbolId`、`additions: GeneratedDeclaration[]`、`interfaceAdds: TypeId[]`、`attributeAdds: AttributeData[]`、`diagnostics: CompileDiagnostic[]` | `target` 必填；其余集合可省略，evaluator 会补空数组。 |
| `GeneratedDeclaration` | 当前 union 只有 `GeneratedField` | 其它 variant 会被 validator 以 `ERROR_KIND` 拒绝。 |
| `GeneratedField` | `name: string`、`type: TypeId`、`visibility: Visibility`、`mutability: Mutability`、可选 `initializer: ConstantValue` | 名称必须是 ASCII identifier，不能与既有或同一 Patch 中的字段冲突。 |
| `CompileDiagnostic` | `isError: bool`、`message: string`、`target: SymbolId` | message 非空，target 必须等于 Patch.target；warning 非致命，error 使 Patch 失败。 |
| `AttributeData` | `typeId: TypeId`、`fieldValues: ConstantValue[]` | 先按 registered attribute schema、target、retention、repeatability 校验。 |

`Visibility` 的取值为 `private`、`protected`、`public`；`Mutability` 的取值为 `let`、
`var`。这些是 typed constructor 的枚举投影，不是可自由声明的新 runtime enum。所有
constructor 必须使用 named arguments；未知、未命名或重复字段会立即产生
`compiletool.constructor` 错误。

## transform 签名规则

`compiler_declaration_transform.c` 只接受以下形状：

```text
ordinary, non-generic, non-async comptime fn
    (one value parameter: immutable declaration view)
    : declaration.Patch
```

参数不能有默认值、`const`、`ref`/`out` passing mode；函数必须有 body，且不能把 target
view 转换成 runtime callable。违反这些条件分别报告
`declaration_transform.signature`，不会进入 evaluator。

## Patch 校验与事务

`ZrParser_DeclarationPatch_Validate` 按如下顺序 fail-closed：

1. `view.symbolId` 与 `patch.target` 必须有效且相等；`expansionRound` 必须为零。
2. `additions`、`interfaceAdds`、`attributeAdds` 各自不能超过
   `ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS`（10000），有数量时指针不能为空。
3. GeneratedField 的 name、TypeId、visibility、mutability 必须有效；不能碰撞既有
   member，也不能在同一 Patch 重复；不能携带另一个 declaration-transform attribute。
4. interface TypeId 必须非零且互不重复；diagnostic message 非空、target 必须是当前
   target。

校验通过后，transaction helper 先在 detached arrays 中准备 field metadata、semantic
symbol、interface relation、attribute metadata 和 source-map provenance；所有 native
array、GC values 和 generated strings 都被临时 root。只有 rebind、layout、interface
contract 和 diagnostic 预检全部成功才交换指针并释放旧数组，任何失败都恢复原长度、
capacity、`nextSymbolId` 和 metadata pointer。一个 target 只允许一轮 Expansion，避免
递归 transform 或 phase cycle。

GeneratedField 进入普通 field pipeline，因此会参与 field size、GC scan kind、const
assignment、interface requirement 和 AOT metadata；它不是绕过 compiler 的“文本拼接”。
保留的 transform provenance 会写入 `.zri` 的 `GENERATED_SOURCE_MAPS` 区段。

## C 接口

```c
EZrParserDeclarationPatchError ZrParser_DeclarationPatch_Validate(
    const SZrParserDeclarationView *view,
    const SZrParserDeclarationPatch *patch);
EZrParserDeclarationPatchError ZrParser_DeclarationView_ValidatePhaseAccess(
    EZrParserCompilePhase currentPhase,
    EZrParserCompilePhase minimumFieldPhase);
const TZrChar *ZrParser_DeclarationPatch_ErrorName(
    EZrParserDeclarationPatchError error);
```

compiler 侧的执行入口为：

```c
TZrBool ZrParser_CompileTimeDeclaration_Execute(
    SZrCompilerState *cs,
    SZrAstNode *node);
TZrBool ZrParser_Compiler_ApplyCompileTimeTypeDecorators(
    SZrCompilerState *cs,
    SZrAstNode *typeNode,
    SZrAstNodeArray *decorators,
    SZrTypePrototypeInfo *info);
TZrBool ZrParser_CompileTime_ApplyMemberDecorators(
    SZrCompilerState *cs,
    SZrAstNode *memberNode,
    SZrAstNodeArray *decorators,
    SZrTypeMemberInfo *memberInfo);
```

这些 API 的 view、AST、字符串和数组均由 `SZrCompilerState` 借用；调用者不能在
compiler state 存活期间修改 view backing storage，也不能把其中的 `const TZrChar *`
保存到下一代 module。若需要跨 artifact 保存，应使用 writer 产生的 metadata/source map。

## 错误与当前边界

常见错误名包括 `argument`、`target`、`round`、`budget`、`kind`、`name`、`collision`、
`type`、`visibility`、`mutability`、`recursive_transform` 和 `phase_cycle`；可用
`ZrParser_DeclarationPatch_ErrorName` 转换为稳定文本。`CompileDiagnostic.isError=true`
时，事务在追加任何 generated member 前终止；warning 只进入编译诊断流。

当前版本明确不支持通过 Patch 直接改写已有 declaration、注入任意 AST/token、递归触发
另一个 transform、生成 method body 或把 compile-time object 泄漏到 runtime。未来若增加
新 GeneratedDeclaration variant，必须先扩展 descriptor、validator、layout、artifact、
AOT、LSP 和独立 reference gate，不能仅在 parser 中放行名称。
