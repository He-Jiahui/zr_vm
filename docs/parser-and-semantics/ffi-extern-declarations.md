---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_ast_constants.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/ffi_contract.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_callable_decorators.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_enum_decorators.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ffi_callable_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ffi_contract.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_ffi.c
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_common/include/zr_vm_common/zr_ffi_contract.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_native_imports.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_native_imports.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_internal.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_contract.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/ffi_contract.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_callable_decorators.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_enum_decorators.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ffi_callable_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ffi_contract.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_ffi.c
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_common/include/zr_vm_common/zr_ffi_contract.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_native_imports.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_internal.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_contract.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
plan_sources:
  - user: 2026-03-29 实现“zr %extern 源级 FFI 声明计划”
  - user: 2026-03-29 extern 语法用于注册外部 ffi
  - user: 2026-04-06 struct 值类型与 native wrapper 分层方案
  - user: 2026-04-06 新的 source-level wrapper decorator surface 和具体 handle_id lowering runtime 完善
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/12-code-stripping.md
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_type_inference.c
  - tests/parser/test_prototype.c
  - tests/ffi/test_ffi_module.c
  - tests/ffi/ffi_fixture.c
  - tests/module/test_module_system.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_extern_enum_decorator_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_diagnostic_fix_smoke.js
  - tests/parser/test_aot_c_code_stripping.c
  - tests/acceptance/2026-07-30-aot-11-12-native-import-contract-reachability.md
doc_type: module-detail
---

# `native extern` FFI Declarations

## Scope

`native extern` 是 zr 当前的静态源级 FFI 声明面。compiler 会把每个函数声明编译成
canonical `SZrNativeImportContract`，VM、`.zro`、AOT C 和 libffi 共同消费这一份契约，
不会在静态调用路径临时构造或重新解析 signature object。

旧 `%extern` 不再是兼容语法。production parser 只识别它以产生
`legacy_syntax_removed` 迁移错误，并且不会创建 `ZR_AST_EXTERN_BLOCK`。需要显式动态加载时，
使用 `zr.ffi` 的动态 API；源级静态声明统一使用 `native extern`。

当前静态契约支持：

- extern function
- extern struct
- extern enum
- extern delegate

当前不支持：

- 非顶层 `native extern`
- extern class / interface
- 显式 `union` 关键字

## Syntax

块级默认库：

```zr
native extern("user32") {
    #zr.ffi.entry("GetTickCount64")#
    pub fn GetTickCount64(): u64;

    delegate Unary(value: f64): f64;

    #zr.ffi.pack(1)#
    struct PackedPair {
        var tag: u8;
        var value: u32;
    }

    #zr.ffi.underlying("u32")#
    enum Mode {
        Read = 1,
        Write = 2
    }
}
```

单声明形式：

```zr
native extern("kernel32") pub fn Sleep(ms: u32): void;
```

v1 允许的 decorator 面：

- `#zr.ffi.entry("Symbol")#`
- `#zr.ffi.callconv("cdecl"|"stdcall"|"system")#`
- `#zr.ffi.charset("utf8"|"utf16"|"ansi")#`
- `#zr.ffi.kind("struct"|"union")#`
- `#zr.ffi.pack(n)#`
- `#zr.ffi.align(n)#`
- `#zr.ffi.offset(n)#`
- `#zr.ffi.value(n)#`
- `#zr.ffi.underlying("u32")#`
- 参数级 `#zr.ffi.in#` / `#zr.ffi.out#` / `#zr.ffi.inout#`

`delegate` 是具名 FFI 签名类型，不是 prototype，不参与 `init Type(...)`、`new Type(...)` 或普通调用。

## Parser And AST

parser 只把 `native extern` 解析成 `ZR_AST_EXTERN_BLOCK`。旧 `%extern` 在进入该函数前即被
移除诊断截断。内部成员节点细分为：

- `ZR_AST_EXTERN_FUNCTION_DECLARATION`
- `ZR_AST_EXTERN_DELEGATE_DECLARATION`
- `ZR_AST_STRUCT_DECLARATION`
- `ZR_AST_ENUM_DECLARATION`

decorator 不再通过“运行一段 compile-time expression”取值。`compiler.c` 会直接按 AST 结构识别 `zr.ffi.*` decorator 路径，再提取字面量参数：

- string 参数走 `extern_compiler_decorators_get_string_arg(...)`
- int 参数走 `extern_compiler_decorators_get_int_arg(...)`

这条路径的目的很明确：extern 元数据是 declaration metadata，不是 compile-time executable program。

### Callable Decorator Validation And Diagnostics

extern function / delegate 的 callable decorator 由 parser/compiler 的
`ZrParser_Compiler_ValidateExternCallableDecorators(...)` 单一校验。实现位于
`compiler_extern_callable_decorators.c`，按 decorator AST identity、call shape 和结构化
argument value 校验，不把名称列表或参数解析复制到 LSP。

callable ABI 值域接受 `c`、`cdecl`、`stdcall` 和 `system`；`c` 与 `cdecl` 都投影为
canonical C ABI。charset、error/cleanup policy、callback lifetime/thread/exception、platform
和 required capability 也由同一 validator 校验。delegate 只接受其声明面允许的 ABI 与
charset 子集。

非法 callable decorator 发布稳定 descriptor `2019` / code `invalid_decorator`：

- primary range 精确覆盖 opening `#` 到 closing `#`；
- severity 为 error，category 为 semantic；
- suggestion/cause 由 parser diagnostic builder 生成；
- fixes 为空，no-fix reason 为 `REQUIRES_USER_DECISION`。

normal compiler 和 LSP analyzer 都调用同一公共 validator。LSP 只消费 compiler 发布的
persistent semantic diagnostic fact，再投影 protocol range/code/message/cause/suggestion/
disposition；不得按 decorator 名、AST 文本或本地 allowed-value table 重建规则。

### Struct And Field Decorator Validation And Diagnostics

extern struct 及其字段由 parser/compiler 的
`ZrParser_Compiler_ValidateExternStructDecorators(...)` 单一校验。声明级规则为：

- `kind` 只接受 `"struct"` 或 `"union"`；
- `pack` 和 `align` 只接受正的、`uint32` 范围内的二次幂；
- 字段 `offset` 只接受 `uint32` 范围内的非负整数；
- 字段 `charset` 只接受 `"utf8"`、`"utf16"` 或 `"ansi"`。

这些规则消费 decorator path、call shape 和 literal value，不按类型名、字段名或源码文本
猜测。normal compiler 与 LSP analyzer 都调用同一个公共 validator；LSP 原有的 struct/field
decorator walker 和参数形状检查已删除。非法值复用 descriptor `2019` / code
`invalid_decorator`，保留 error severity、semantic category、canonical message/cause/suggestion、
零 fixes 和 `REQUIRES_USER_DECISION`。

struct member lookahead 使用完整 parser cursor 快照，因此字段 decorator 的 primary range 精确
覆盖 opening `#` 到 closing `#`。LSP 和 stdio 只投影该 persistent semantic diagnostic fact，
不得恢复本地 allowed-name/value table 或 `compiler_error` 并行兜底。

### Enum And Member Decorator Validation And Diagnostics

extern enum 及其成员由 parser/compiler 的
`ZrParser_Compiler_ValidateExternEnumDecorators(...)` 单一校验。声明级
`underlying` 必须是单个 string literal，并且只接受 `i8`、`u8`、`i16`、`u16`、
`i32`、`u32`、`i64` 或 `u64`；成员级 `value` 必须是单个 integer literal。
未知 decorator、错误 call shape 和非法 structured value 均 fail closed。

normal compiler 与 LSP analyzer 调用同一个公共 validator。非法 enum/member decorator
发布 descriptor `2019` / code `invalid_decorator`，primary range 覆盖完整 decorator，
并保留 canonical message/cause/suggestion、零 fixes 与
`REQUIRES_USER_DECISION`。LSP 原有的 enum/member walker 与 integer-shape helper 已删除；
semantic analyzer 和 stdio 仅投影 persistent semantic query fact，不得按 decorator 名称、
AST/source text、message 或本地 underlying table 重建规则，也不得补发平行
`compiler_error`。

## Semantic And Type Visibility

extern 声明在编译前先经过 `ZrParser_Compiler_PredeclareExternBindings(...)` 预注册：

- extern struct / enum 先注册 runtime prototype metadata
- extern function 先把签名注入普通 type environment
- extern delegate 先把签名名义类型暴露给后续参数和 `zr.ffi.callback(...)`

extern `struct` / `enum` 同时具备“zr 类型可见性”和“FFI layout descriptor 可见性”：

- extern struct 支持 `init Type(...)` 值构造和 `new Type(...)` boxed 构造
- extern enum 支持 `Enum.Member`、`init Enum(value)`、`new Enum(value)`
- extern delegate 只能当签名类型使用

类型推导消费的是已注册 declaration metadata，不再依赖“普通函数返回匿名 object 充当外部类型”。

## Boxed Struct Value Model

`native extern` 中的 `struct` 代表“值语义聚合”，但运行时实现不再尝试把不定长字段布局直接塞进固定大小的 `SZrValue`。

当前模型是：

- `SZrValue` 里仍只保存对象引用
- struct 对象内部保存 descriptor 和字段存储
- 普通可复制 struct 的赋值 / 传参会走专门的 deep-clone 路径，而不是普通 object 引用拷贝

这让 extern struct 能承担：

- by-value argument / return
- nested extern struct
- pointer pointee overlay
- FFI 读写布局描述

同时保住 zr VM 的固定尺寸 value 布局。

如果 struct 字段里带有 field-scoped `using` 或其它非可复制 ownership 语义，type inference 会把它视为 move-only：

- 普通赋值不允许隐式复制
- 普通按值传参也不允许隐式复制

这条规则对 source struct 和 extern struct 一致成立。

## FFI Boundary Wrapper Lowering

native resource 不再要求上层透出裸 `Ptr`。当前 v1 采用“wrapper object + FFI 边界 lowering”的分层模型：

- extern struct 仍是 layout/value type
- native handle / pointer wrapper 仍是 class-like object
- 自动 lowering 只发生在 `native extern` / `zr.ffi` 调用边界

普通 zr 语义里保持严格分层：

- 普通赋值不做 wrapper -> pointer / delegate 隐式转换
- 普通函数调用不做 wrapper -> pointer / delegate 隐式转换
- 容器写入也不做这类隐式转换

当前已经打通的 source-level boundary 兼容包括：

- extern `delegate` 参数接受 `CallbackHandle`
- extern `pointer<T>` 参数接受 `BufferHandle` / `PointerHandle` 风格 wrapper
- extern 整数 handle 参数接受 `#zr.ffi.lowering("handle_id")#` + `#zr.ffi.underlying("...")#` 标注过的 source wrapper class

这条兼容只在 `native extern` 函数 overload 选择和参数检查里生效；普通类型系统仍把 wrapper 当普通对象类型处理。

## Source Wrapper Decorator Surface

v1 没有新增 `userdata` 关键字，而是直接复用 class declaration：

```zr
#zr.ffi.lowering("handle_id")#
#zr.ffi.underlying("i32")#
class ModeHandle {
    var handleId: i32;
}
```

当前 source-level wrapper decorator surface 只对 `class` 合法，支持：

- `#zr.ffi.lowering("value"|"pointer"|"handle_id")#`
- `#zr.ffi.viewType("ExternStructName")#`
- `#zr.ffi.underlying("i8"|"u8"|"i16"|"u16"|"i32"|"u32"|"i64"|"u64")#`
- `#zr.ffi.ownerMode("borrowed"|"owned")#`
- `#zr.ffi.releaseHook("native_symbol")#`

实现约束：

- parser 现在允许顶层声明前连续出现多条 decorator，再统一绑定到后续 class / struct / function
- `compiler_class.c` 会把这些 `zr.ffi.*` wrapper decorators 直接编译成 type decorator metadata，而不是走普通 runtime decorator expression 执行路径
- LSP semantic analyzer 会在 class declaration 上校验这些 decorator 的参数和值域
- `zr.ffi.viewType(...)` 当前要求名字解析到同一 source file 里的 extern struct

`zr.ffi.underlying(...)` 当前只在 `lowering("handle_id")` 下有语义。它描述 wrapper 过 FFI 边界时应该降到哪种整数 ABI 类型；source-level decorator 目前只接受固定宽度整数名 `i8/u8/i16/u16/i32/u32/i64/u64`，与 runtime ABI lowering 支持集保持一致。

## `handle_id` Lowering Runtime

`handle_id` lowering 现在走和 pointer wrapper 一样的“prototype hidden metadata + boundary-only marshalling”模型：

- source class wrapper 的 compile-time metadata 会在 module prototype materialization 时桥接成 runtime hidden fields：
  - `__zr_ffiLoweringKind`
  - `__zr_ffiUnderlyingTypeName`
  - 以及可选的 `__zr_ffiViewTypeName` / `__zr_ffiOwnerMode` / `__zr_ffiReleaseHook`
- `zr_vm_lib_ffi` marshalling 只在构建 FFI scalar argument 时读取这些 hidden fields
- 当目标 ABI 参数是整数标量且 underlying 名称匹配时，runtime 会优先从 wrapper 对象的：
  - `__zr_ffi_handleId`
  - 或公开字段 `handleId`
  读取值，再降成 ABI 整数

这条 lowering 不会泄漏到普通 zr 调用：

- 普通函数 `fn FlipLocal(mode: i32): void` 仍然不能接受 `ModeHandle`
- 普通赋值、容器写入、非 FFI member call 也不会自动把 wrapper 当整数 handle 用

因此 `native extern` / `zr.ffi` 边界继续是唯一发生 implicit lowering 的地方。

## Canonical Static Contract

每个 `native extern` function 产生一项 schema v4 `SZrNativeImportContract`，核心字段包括：

- schema version、稳定 `symbolId`、`declaringModuleId`
- 独立的 ABI `signatureHash` 与结构化 `callable.contractHash`
- callable 参数的 canonical type hash、passing form、escape 上界、入口/出口初始化状态、
  temporary admission 和 call-site marker
- library locator、entry point、platform availability 和 required capabilities
- target pointer size、endianness、ABI hash、calling convention 和 variadic 标记
- return/parameter type kind、size、alignment、canonical/layout hash
- aggregate field pool，以及 `in/ref/out`、marshalling、ownership/nullability
- charset、error/cleanup policy、callback lifetime/thread/exception policy
- document、offset、行列范围等 source mapping

ABI signature 与 language callable contract 是两个不同的 canonical vector。例如 `value i32`
和 `in i32` 可以具有相同 ABI signature hash，但必须具有不同 callable hash。common 层会分别
校验两个 hash，并检查 `in/ref/out` 与 passing form 的交叉约束，禁止重新计算单侧 hash 后绕过
方向语义。

aggregate/union 的 layout hash 由 canonical `SZrTypeLayout` 构造和验证，不再由 FFI builder
维护第二套临时布局算法。类型准入依赖 canonical capability 与实际 layout facts，而不是
`Span`、`Task`、`Unique` 等具体名字：名为 `Span` 的本地 blittable struct 可以进入 ABI；带
ownership/ref-like/resource/GC-reference 能力或不受支持的 generic shape 仍 fail closed。

common 层对契约和值域执行统一校验并计算稳定 little-endian FNV hash。compiler 把契约
挂到 `SZrFunction.nativeImportContracts`；`.zro` 按显式字段序列化，不依赖宿主结构体布局。
AOT C 与 LLVM 都生成完整的结构化 callable vector，并通过 ABI 14 的 module/code
registration 同时发布表指针和数量；loader 拒绝两侧不一致或损坏的表。

启用 AOT code stripping 时，writer 在 ExecIR 之前校验完整 function tree 上的每个 canonical contract，
不可达 owner 上的损坏 row 也会使输出 fail closed。裁剪后只发布 retained owner 的 contract table，并输出
`nativeImportsBefore/After/Removed` 以及版本化 `nativeImportManifest`。清单 identity 使用 `symbolId` 和
`callable.contractHash`，owner/predecessor 使用 stable flat function index；library/entry 字符串不参与可达性推断。

静态调用执行到 `LibraryHandle.getContractSymbol(contractIndex)`。runtime 从当前调用链定位
声明函数持有的 canonical contract，校验 library/availability/capability/target ABI，直接
把同一份 `FfiSignature` 降到 libffi。ref-like、owner/resource/GC reference 等不能
直接进入 native ABI 的类型会按 capability/layout facts 在契约构建阶段拒绝；callback 参数必须显式声明生命周期、
线程和异常策略。

## Removed Legacy Lowering

旧实现曾为每个 `%extern` block 生成一套模块局部隐藏缓存：

1. `import("zr.ffi")`
2. `loadLibrary(libraryName)`
3. 对每个 extern function 调用 `library.getSymbol(entryName, signatureDescriptor)`

该 compiler lowering 已不再由 production parser 到达。需要动态 FFI 时必须显式调用
`zr.ffi` API；`%extern` 本身只产生移除诊断。

这里保持两条约束：

- 库和符号是懒解析的，只有代码真的执行到 extern block 时才加载
- function / struct / enum / delegate 的 descriptor 都先在 compiler 里构造成稳定 object，再交给 `zr.ffi`

extern function 的 signature descriptor 包含：

- `returnType`
- `parameters`
- `abi`
- `varargs`

extern struct 的 layout descriptor 包含：

- `kind = "struct"`
- `name`
- `fields`
- `pack`
- `align`

extern enum 的 descriptor 包含：

- `kind = "enum"`
- `name`
- `underlying`
- `members`

extern delegate 的 descriptor 复用 function signature shape，但带 `kind = "function"`，供 callback trampoline 直接消费。

source extern 的指针形参语法仍写成 `pointer<T>`，但 compile-time 兼容检查会把它与 `zr.ffi` helper API 产生的 `Ptr<T>` 指针家族视为同一个 FFI pointer family。

## Compile-Time Projection Boundary

这轮修复里最重要的约束是：

- `compileTimeTypeEnv` 代表“编译期可见的签名和类型信息”
- 它不再等价于“允许被 compile-time executor 直接执行的调用目标”

`compile_expression.c` 的 compile-time projection 现在只会对两类名字生效：

- 真正登记在 `compileTimeVariables` / `compileTimeFunctions` 里的 compile-time declarations
- `Assert` / `FatalError` / `import` 这类内建 compile-time entry

这避免了 extern function 因为“签名已在 compile-time env 可见”而被误判成 compile-time callable，进而触发 `Compile-time evaluation failed`。

## Runtime Expectations

`zr.ffi` 继续负责：

- 动态库句柄缓存
- 符号解析
- ABI / marshalling
- callback trampoline
- `FfiError` 分类

`native extern` 静态路径消费 canonical contract；显式动态 FFI API 仍消费 descriptor object。
两条路径最终都进入同一套 libffi marshaller、pin/callback
边界和 `FfiError` 分类，没有裸调用帧分支，也不允许跨函数持有裸栈指针。

当前已覆盖的错误分类包括：

- `LoadError`
- `SymbolError`
- `AbiMismatch`
- `MarshalError`
- `CallbackThreadError`
- `NativeCallError`

## Validation

当前回归覆盖的重点：

- `tests/parser/test_parser.c`
  - `native extern("x") fn Foo(): u64;`
  - block 形式 `native extern`
  - extern delegate / struct / decorator 解析
- `tests/parser/test_type_inference.c`
  - extern function 返回类型推断
  - extern enum member 推断
  - extern struct 构造推断
- `tests/ffi/test_ffi_module.c`
  - source extern function 调用
  - source extern delegate callback
  - source extern pointer parameter lowering
  - callconv decorator
  - struct pack / offset overlay
  - runtime error classification
- `tests/ffi/test_native_extern_contract.c`
  - scalar、aggregate/union、`in/ref/out`、callback 与拒绝类型契约
  - ABI signature hash 与 callable hash 独立，方向交叉约束不可被重新哈希绕过
  - 类型名无关的 blittable admission 与 canonical `SZrTypeLayout` hash
  - availability/capability、target ABI、error/cleanup 与 corrupt contract
  - 当前语法静态符号实际调用
  - schema v4 callable vector 的 `.zro` roundtrip 与截断输入拒绝
  - C/LLVM AOT canonical table 生成、共享库加载及 VM/libffi 共用 golden vectors

如果后续继续扩展 extern `class`、extern `interface` 或 source-level version predicates，优先保持同一个原则：

- declaration metadata 先注册
- 编译期可见性与 compile-time executable 分离
- 静态声明持久化 canonical contract，动态 API 才使用 descriptor-driven marshaller
