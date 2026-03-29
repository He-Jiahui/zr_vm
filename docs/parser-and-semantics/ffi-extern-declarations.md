---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_ast_constants.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
plan_sources:
  - user: 2026-03-29 实现“zr %extern 源级 FFI 声明计划”
  - user: 2026-03-29 extern 语法用于注册外部 ffi
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_type_inference.c
  - tests/ffi/test_ffi_module.c
  - tests/ffi/ffi_fixture.c
doc_type: module-detail
---

# `%extern` Source FFI Declarations

## Scope

`%extern` 是 zr 的源级 FFI 声明面。它建立在 `zr.ffi` runtime API 之上，但不再要求用户先手写 `loadLibrary` / `getSymbol` / signature object，再把返回的动态句柄塞进普通变量。

v1 当前支持：

- extern function
- extern struct
- extern enum
- extern delegate

当前不支持：

- 普通 `extern` 关键字别名
- 非顶层 `%extern`
- extern class / interface
- 显式 `union` 关键字

## Syntax

块级默认库：

```zr
%extern("user32") {
    #zr.ffi.entry("GetTickCount64")#
    GetTickCount64(): u64;

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
%extern("kernel32") Sleep(ms: u32): void;
```

v1 允许的 decorator 面：

- `#zr.ffi.entry("Symbol")#`
- `#zr.ffi.callconv("cdecl"|"stdcall"|"system")#`
- `#zr.ffi.charset("utf8"|"utf16"|"ansi")#`
- `#zr.ffi.pack(n)#`
- `#zr.ffi.align(n)#`
- `#zr.ffi.offset(n)#`
- `#zr.ffi.value(n)#`
- `#zr.ffi.underlying("u32")#`
- 参数级 `#zr.ffi.in#` / `#zr.ffi.out#` / `#zr.ffi.inout#`

`delegate` 是具名 FFI 签名类型，不是 prototype，不参与 `$Type(...)`、`new Type(...)` 或普通调用。

## Parser And AST

parser 把 `%extern` 解析成独立的 `ZR_AST_EXTERN_BLOCK`，内部成员节点细分为：

- `ZR_AST_EXTERN_FUNCTION_DECLARATION`
- `ZR_AST_EXTERN_DELEGATE_DECLARATION`
- `ZR_AST_STRUCT_DECLARATION`
- `ZR_AST_ENUM_DECLARATION`

decorator 不再通过“运行一段 compile-time expression”取值。`compiler.c` 会直接按 AST 结构识别 `zr.ffi.*` decorator 路径，再提取字面量参数：

- string 参数走 `extern_compiler_decorators_get_string_arg(...)`
- int 参数走 `extern_compiler_decorators_get_int_arg(...)`

这条路径的目的很明确：extern 元数据是 declaration metadata，不是 compile-time executable program。

## Semantic And Type Visibility

extern 声明在编译前先经过 `ZrParser_Compiler_PredeclareExternBindings(...)` 预注册：

- extern struct / enum 先注册 runtime prototype metadata
- extern function 先把签名注入普通 type environment
- extern delegate 先把签名名义类型暴露给后续参数和 `zr.ffi.callback(...)`

extern `struct` / `enum` 同时具备“zr 类型可见性”和“FFI layout descriptor 可见性”：

- extern struct 支持 `$Type(...)` 值构造和 `new Type(...)` boxed 构造
- extern enum 支持 `Enum.Member`、`$Enum(value)`、`new Enum(value)`
- extern delegate 只能当签名类型使用

类型推导消费的是已注册 declaration metadata，不再依赖“普通函数返回匿名 object 充当外部类型”。

## Lowering To `zr.ffi`

每个 `%extern` block 在 lowering 时都会生成一套模块局部隐藏缓存：

1. `import("zr.ffi")`
2. `loadLibrary(libraryName)`
3. 对每个 extern function 调用 `library.getSymbol(entryName, signatureDescriptor)`

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

`%extern` 只是把源级声明 lower 到同一套安全 helper。它没有新开裸调用帧分支，也不允许跨函数持有裸栈指针。

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
  - `%extern("x") Foo(): u64;`
  - block 形式 `%extern`
  - extern delegate / struct / decorator 解析
- `tests/parser/test_type_inference.c`
  - extern function 返回类型推断
  - extern enum member 推断
  - extern struct 构造推断
- `tests/ffi/test_ffi_module.c`
  - source extern function 调用
  - source extern delegate callback
  - callconv decorator
  - struct pack / offset overlay
  - runtime error classification

如果后续继续扩展 extern `class`、extern `interface` 或 source-level version predicates，优先保持同一个原则：

- declaration metadata 先注册
- 编译期可见性与 compile-time executable 分离
- runtime 统一走 `zr.ffi` 的 descriptor-driven marshaller
