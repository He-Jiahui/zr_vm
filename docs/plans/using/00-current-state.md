---
doc_type: plan-detail
related_code:
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/runtime.h
---

# 00 · 现状盘点

本篇给后续设计提供事实基线：五个相关机制各自落在哪些文件、关键 struct/enum、完成度，以及与目标形态的差距。引用以 `file:line` 为准（行号为调研时快照，可能随提交漂移）。

## 1. `using` — 已是语句级 lifetime fence，但能力单薄

- 词法：[lexer.h:34](../../../zr_vm_parser/include/zr_vm_parser/lexer.h#L34) 有 `ZR_TK_USING`。裸 `using` 关键字在 parser 中被拒绝，要求写 `%using`（向后兼容护栏）。
- AST：[ast.h:773-777](../../../zr_vm_parser/include/zr_vm_parser/ast.h#L773-L777)
  ```c
  typedef struct SZrUsingStatement {
      SZrAstNode *resource;   // 资源表达式
      SZrAstNode *body;       // Block，可选（仅 using(expr){...} 形式）
      TZrBool isBlockScoped;
  } SZrUsingStatement;
  ```
- 解析：`parse_using_statement()` 在 `parser_statements.c`，支持 `%using(expr){...}` 与 `%using expr;` 两种形式。
- 编译：`compile_using_statement()` 在 [compile_statement.c:2197](../../../zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c#L2197)。它：
  1. `ZrParser_Semantic_ReserveLifetimeRegionId()` 预留一个生命周期 region；
  2. `register_using_resource_symbol()` 登记资源符号；
  3. 追加一个 `SZrDeterministicCleanupStep`（`callsClose = TRUE`、`callsDestructor = TRUE`）；
  4. 进入/退出作用域，块结束时按 cleanup plan 触发析构。
- cleanup plan 结构：[semantic.h:92-100](../../../zr_vm_parser/include/zr_vm_parser/semantic.h#L92) 的 `SZrDeterministicCleanupStep`，kind 取 `BLOCK_SCOPE / INSTANCE_FIELD / STRUCT_VALUE_FIELD`。

**差距**：`using` 目前只表达"块退出 → 关闭/析构资源"。它**不**承担：所有权种类绑定、`%import` 的条件加载守卫、union variant 解构。目标是把它升级为统一作用域原语。

## 2. `%`-所有权 — 类型限定符，已有完整运行时

- 限定符枚举：[ast.h:173-180](../../../zr_vm_parser/include/zr_vm_parser/ast.h#L173) `EZrOwnershipQualifier`（`NONE/UNIQUE/SHARED/WEAK/BORROWED/LOANED`）。挂在 [ast.h:258](../../../zr_vm_parser/include/zr_vm_parser/ast.h#L258) `SZrType.ownershipQualifier` 上。
- 内建操作：[ast.h:184-194](../../../zr_vm_parser/include/zr_vm_parser/ast.h#L184) `EZrOwnershipBuiltinKind`（`UNIQUE/SHARED/WEAK/BORROW/LOAN/UPGRADE/RELEASE/DETACH`）。
- 解析：`try_get_ownership_qualifier()` 在 `parser_expression_primary.c`，在 `%` 后读标识符；可用于类型注解、方法 receiver、构造表达式、cast。
- 运行时值种类：[value.h:28-45](../../../zr_vm_core/include/zr_vm_core/value.h#L28) `EZrOwnershipValueKind`（与限定符一一对应）。
- 控制块与 API：[ownership.h](../../../zr_vm_core/include/zr_vm_core/ownership.h)
  ```c
  struct SZrOwnershipControl {
      struct SZrRawObject *object;
      TZrUInt32 strongRefCount;        // 强引用计数
      TZrBool isDetachedFromGc;        // 是否脱离 GC
      SZrOwnershipWeakRef *weakRefs;   // 弱引用链表
  };
  ```
  转移函数：`InitUniqueValue / UniqueValue / BorrowValue / LoanValue / ShareValue / WeakValue / UpgradeValue / DetachValue / ReturnToGcValue / ReleaseValue`。
- 字段级写法已收敛到类型上：`var field: %unique T`（见 [owned-field-lifecycle.md](../../parser-and-semantics/owned-field-lifecycle.md)），字段级 legacy `%using` 已移出 public surface。

**差距**：所有权"种类"是一个**挂在类型上的 enum 字段**，不是一等类型。它无法作为泛型实参传播（`Array<Unique<T>>` 没有统一表达）、无法被泛型代码统一处理、`%` 语法与"编译器指令"语义混在一起。目标是把它变成内建泛型类型。

## 3. 泛型 — 编译期闭型实例化已成熟

- AST：[ast.h:198-211](../../../zr_vm_parser/include/zr_vm_parser/ast.h#L198) `EZrGenericParameterKind`（`TYPE` / `CONST_INT`）、`EZrGenericVariance`（`NONE/IN/OUT`）、`SZrGenericDeclaration`、[ast.h:271-274](../../../zr_vm_parser/include/zr_vm_parser/ast.h#L271) `SZrGenericType`。
- 参数约束：[ast.h:302-307](../../../zr_vm_parser/include/zr_vm_parser/ast.h#L302) `SZrParameter` 上有 `genericTypeConstraints / genericRequiresClass / genericRequiresStruct / genericRequiresNew`。
- 模型：开放泛型定义 + 闭型懒实例化 + 共享函数体（接近 C#，非 C++ 单态化）。整数 const 泛型以编译期归约值定身份（`Matrix<int,2+2>` ≡ `Matrix<int,4>`）。
- 方差：`in/out` 仅用于 interface 泛型参数；`%in/%out/%ref` 是独立的**参数传递**机制，非方差。

**这是本计划最重要的可复用基建**：所有权泛型化、union 泛型 variant、插件类型守卫都建立在这套闭型机制上。

## 4. `%import` / 插件 — 有 FFI 与插件注册表，但加载是"立即且无守卫"

- AST：[ast.h:434-436](../../../zr_vm_parser/include/zr_vm_parser/ast.h#L434) `SZrImportExpression { modulePath }`，解析 `%import("name")`。
- 编译期解析：`compile_statement.c` 在变量初始化处理 `var x = %import(...)`。
- 运行时模块：[module_loader.c](../../../zr_vm_core/src/zr_vm_core/module/module_loader.c) 执行 module entry、物化 prototype。
- 原生插件注册表：[native_registry.h](../../../zr_vm_library/include/zr_vm_library/native_registry.h) 有 `ZrLibRegisteredModuleInfo`、`EnsureProjectDescriptorPlugin()`（运行时加载 `.so/.dll` 描述符插件）。
- FFI 运行时：[runtime.h](../../../zr_vm_lib_ffi/include/zr_vm_lib_ffi/runtime.h) 有 `ZrFfi_LoadLibrary / GetSymbol / Symbol_Call / CreateCallback`。

**差距**：`%import` 现在是"导入即解析"，没有"按需 / 失败可降级 / 加载成功才让类型可见"的语言级语义。用户想要的"插件按需加载、`using` 判断是否已有效加载、加载了才走 block 内逻辑"目前**没有对应语法**。这正是 `using` 要新增承担的核心场景。

## 5. metadata / token — 有 typed metadata，但**没有全局 token，跨模块靠 name 字符串**

- `.zro` typed metadata：[io.h:107-350](../../../zr_vm_core/include/zr_vm_core/io.h#L107) 的 `SZrIoFunctionTypedTypeRef / TypedLocalBinding / TypedExportSymbol`；导出符号签名含参数类型、返回类型、源码范围。
- prototype 序列化：[constant_reference.h:20-88](../../../zr_vm_core/include/zr_vm_core/constant_reference.h#L20) `SZrCompiledPrototypeInfo` / `SZrCompiledMemberInfo`（`#pragma pack(1)`）。
- 编译期 ID：`semantic_facts.h` 的 `TZrTypeId / TZrSymbolId / TZrOverloadSetId / TZrLifetimeRegionId` —— **仅编译期存在，不落盘**。
- 跨模块引用：靠 `module.symbol` 名字 + `SZrIoFunctionTypedExportSymbol`（name + stackSlot + callableChildIndex）。
- AOT 静态直调：用 **flat function index**（编译期扁平化数组下标），无 token。

**与 C# 模型对照**：

| 能力 | C# (ECMA-335) | zr_vm 现状 |
| --- | --- | --- |
| 全局唯一 token | `0xTTrrrrrr`，table 标签 + RID | ✗ 无，仅编译期临时 ID |
| 跨程序集引用 | `AssemblyRef`+`TypeRef`+`MemberRef` | △ 名字字符串 + typed export |
| 方法/字段签名 blob | 结构化签名 blob | △ `SZrIoFunctionTypedExportSymbol`（已结构化但无 token 锚定） |
| AOT 消费 metadata | IL2CPP 用完整 token 表 | ✗ typed metadata 尚未被 AOT 后端消费 |

**差距**：没有稳定 token，就无法干净地做"DLL 间按签名访问"和"插件守卫按身份比对"。这是第 03 篇的核心。

## 6. union — 不存在

- 现有 `enum`（[zr_language_specification.md](../../zr_language_specification.md) §2.7）只能继承 `int/string/float/bool`，成员是常量值，**不能携带字段**。
- 没有 Rust `enum` / C# 的 discriminated union / tagged union。
- 现有 `switch` 语法是 `switch(v){ (1){...} (){default} }`，无模式匹配 / 解构 / variant 绑定。

**差距**：需要全新的 `union` 关键字、AST 节点、判别式布局、模式匹配 lowering。第 04 篇覆盖。

## 7. 小结：可复用基建 vs 需新建

**可直接复用 / 扩展**：
- 闭型泛型机制（第 1 等公民，承载所有权泛型 + union variant）。
- `SZrDeterministicCleanupStep` cleanup plan（承载 `using` 确定性析构）。
- `SZrOwnershipControl` 运行时控制块 + 转移 API（泛型化只是改前端表面，运行时不动）。
- typed metadata 序列化（在其上叠加 token 表）。
- FFI / 插件注册表（在其上叠加 `using` 加载守卫）。

**需新建**：
- 内建泛型类型 `Unique<T>/Shared<T>/Weak<T>/Borrow<T>/Loan<T>`（前端表面 + 与 `EZrOwnershipValueKind` 的映射）。
- `using` 的"条件守卫"形态（绑定 `%import` 结果、签名校验通过才进 block）。
- 全局 metadata token 表 + 跨模块签名解析器。
- `union` 全链路（lexer→parser→AST→semir→运行时判别式→模式匹配）。
