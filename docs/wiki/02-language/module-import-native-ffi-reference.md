---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/ffi_contract.h
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ffi_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
  - zr_vm_library/src/zr_vm_library/project/project_import_resolver.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_contract_validation.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki 的语言规则、用例与 C 接口说明
  - docs/plans/syntax/10-native-ffi-module-package/m1-specifier-foundation.md
  - docs/plans/syntax/10-native-ffi-module-package/m2-artifact-provider-phase.md
  - docs/plans/syntax/10-native-ffi-module-package/m2-v2-manifest-admission.md
tests:
  - tests/library/test_project_import_resolver.c
  - tests/library/test_project_module_specifier.c
  - tests/ffi/test_native_extern_contract.c
  - tests/parser/test_project_import_canonicalization.c
  - tests/fixtures/projects/syntax_reference_v1/src/modules.zr
  - tests/fixtures/projects/syntax_reference_v1/src/native_ffi.zr
doc_type: language-reference
---

# 模块导入与 Native FFI 参考

ZR 的 `import("...")`、`native extern("...")` 和 C native provider 都会接触“模块”一词，
但它们不是同一个机制：import 选择一个 project/provider 的模块位置；`native extern` 描述
外部 ABI；native registry 则登记由宿主直接提供的 ZR module descriptor。本页把三者分开，
给出语法、binding/验证链和 C 侧入口，避免把 source-level declaration 误当作动态库加载 API。

项目 manifest、artifact 和 resolver 的更宽背景见[模块、项目与产物](../07-modules-projects-artifacts.md)，
模块库的已有 API 目录见[模块索引](../03-modules/index.md)。

## 三个边界

| 机制 | ZR 源码入口 | 主产物 | 谁执行实际工作 | 不应承担的责任 |
| --- | --- | --- | --- | --- |
| 静态 import | `import("module.path")` | import expression + module key | Library project resolver/provider loader | 直接描述 C 参数布局。 |
| Native extern | `native extern("library") { ... }` | `SZrExternBlock` + FFI contract | FFI compiler、AOT/runtime linker | 自动注册一个 ZR provider module。 |
| Native provider | `ZrLibrary_NativeRegistry_RegisterModule` | `ZrLibModuleDescriptor` | host/global native registry | 从任意 source string 推断 ABI。 |

一个程序可以同时使用三者：先 `import("zr.task")` 获取官方 provider，再声明
`native extern("codec")` 调用动态库符号，同时由 host 注册自定义 `engine.render` module。
它们共享 diagnostics 和 global/state 生命周期，但 identity、缓存和失败模型不同。

## `import`：静态、字符串字面量的模块请求

```ebnf
import-expression = "import", "(", string-literal, ")" ;
```

```zr
let system = import("zr.system");
let workspace = import("engine.render");
let nativeProvider = import("native:engine.render");
let alias = import("#nativeRender");
let dependency = import("@fixturedep/tool");
let archive = import("artifacts/fixturedep.zrm");
```

`import` 在 lexer/parser 中按 contextual name 处理，但语法形状是严格的：括号内必须为一个
string literal。下列写法应被视为无效或已移除的 source surface：

```zr
// 不要这样写。
let a = import engine.render;
let b = import(pathVariable);
let c = zr.import("engine.render");
```

parser 在 `import("...")` 中建立 import AST，并保留 literal 的 source range；它不读取
磁盘、不展开 alias、不打开 `.zrm`，也不验证 native provider 已存在。这种静态 spelling 是
让 compiler、LSP 和 artifact writer 看到稳定依赖边的前提。

### specifier 与 resolver

Library 将 raw literal 解析为 `SZrLibrary_ModuleSpecifier`，再结合当前 module/project 生成
canonical module key 和 provider location。常见可见形态如下：

| 输入形式 | 典型语义 | 解析的下一步 |
| --- | --- | --- |
| `zr.system` | 官方 native module | 在官方 inventory/registry 中解析。 |
| `native:engine.render` | 注册 native provider | 按 native domain 查 descriptor。 |
| `engine.render` | workspace module | 由 `.zrp` source/binary 路径定位。 |
| `./child`、`../shared` | 相对 workspace 模块 | 用 importing module 的 identity 规范化。 |
| `#nativeRender` | manifest alias | alias 展开后重新分类。 |
| `@fixturedep/tool` | package import | dependency/export 表解析。 |
| `artifacts/fixturedep.zrm` / `file:...` | artifact/file 定位 | 进入 archive/file policy 和 provider location。 |

相同显示文本不保证相同 identity。尤其 `native:engine.render` 和 workspace `engine.render`
可同时存在，不能通过省略 domain 或比较字符串将它们合并。resolver 的 public C 入口包括
`ZrLibrary_ModuleSpecifier_Parse`、`ZrLibrary_Project_ResolveImportModuleKey`、
`ZrLibrary_Project_ResolveImportProviderLocation` 和 AOT load request 解析函数；具体 owner/错误
buffer 规则见[项目、文件与 ZRM API](../05-interop/project-file-zrm-api.md)。

## `native extern` 块

最常用的 block form：

```zr
native extern("syntax_reference_native") {
    #zr.ffi.entry("syntax_reference_render_value")#
    #zr.ffi.callingConvention("c")#
    pub fn renderValue(): i32;
}

fn checksum(): i32 {
    return renderValue();
}
```

生产 parser 的摘要：

```ebnf
extern-block       = "native", "extern", "(", string-literal, ")",
                     ( "{", { extern-member }, "}" | extern-member ) ;
extern-member      = { decorator }, [ access ],
                     ( extern-function | extern-delegate | struct-declaration
                     | enum-declaration ) ;
extern-function    = "fn", identifier, "(", parameters, ")",
                     [ ":", TypeRef ], ";" ;
extern-delegate    = "delegate", identifier, "(", parameters, ")",
                     [ ":", TypeRef ], ";" ;
decorator           = "#", expression, "#" ;
access              = "pub" | "pri" | "pro" ;
```

unbraced的单成员形式当前由 parser 接受，但文档、代码生成和审查中应优先使用 braced form，
因为它清楚划定 library spec 与全部声明的范围。普通函数在 extern block 内必须以 `fn` 开头；
裸 `Name(...)` 会得到明确的 parser 错误。

`native extern` 仅声明：函数没有 ZR body，末尾必须有 `;`。若需要执行 ZR 实现，请声明普通
`fn`；若需要把已有 C callback 暴露成 ZR module API，请使用 descriptor registry，而不是用
extern declaration 伪装回调实现。

## 声明 ABI 的核心元素

### 函数、direction 与 symbol

```zr
native extern("fixture") {
    #zr.ffi.entry("zr_ffi_increment_i32")#
    pub fn increment(value: ref i32): i32;

    #zr.ffi.entry("zr_ffi_initialize_i32")#
    pub fn initialize(value: out i32): i32;
}
```

`ref` 与 `out` 在 extern 参数中不是装饰性的类型文字。FFI contract 需要把 source direction、
native pointer/writeback 行为、layout 和 call-site marker 固化成可验证 signature。调用方也要
使用匹配的 `ref value` 或 `out value` marker。把一个 mutable pointer 伪装成 by-value `i32`
会在 contract/binding 阶段失败，而不是静默地由 ABI “猜测”。

没有 `#zr.ffi.entry(...)#` 时，contract 可采用声明名或配置的默认规则；在要求稳定符号名的
跨语言 API 中应显式声明 entry，避免重命名 ZR API 时意外改变 native symbol 查找。

### struct、union 和 delegate

```zr
native extern("fixture") {
    struct Point {
        var x: i32;
        var y: i32;
    }

    #zr.ffi.kind("union")#
    struct NumberView {
        #zr.ffi.offset(0)# var integer: i32;
        #zr.ffi.offset(0)# var scalar: f32;
    }

    delegate Unary(value: f64): f64;
    #zr.ffi.entry("zr_ffi_apply_callback")#
    #zr.ffi.callbackLifetime("call")#
    fn apply(value: f64, callback: Unary): f64;
}
```

extern block 可以容纳 struct、enum 和 delegate，使 FFI compiler 能从同一 source region 建立
aggregate/callback contract。`#zr.ffi.kind("union")#` 与每个字段的 `#zr.ffi.offset(...)#`
用于说明显式 overlay；它们不是普通 class field 语义。layout hash、对齐、平台 ABI 与允许的
字段类型最终由 contract validator 决定。

delegate 作为 callback 参数必须有明确 callback lifetime/policy。测试覆盖的 `call` lifetime
表示仅在这次 native 调用内可使用；C 库若把回调保存到调用返回之后，需要声明并实现匹配的
长生命周期策略，否则 contract 应拒绝它。不要把一个 ZR closure 的裸地址缓存到 C 全局变量。

### 受证据覆盖的 FFI decorator 类别

下表列出 FFI contract tests 中实际出现的类别，作为常用接口面而不是“任何字符串都可用”的
承诺。decorator 是否可用于 function、parameter、struct 或 field 由 FFI compiler 单独检查。

| decorator 示例 | 表达的事实 | 典型失败 |
| --- | --- | --- |
| `#zr.ffi.entry("symbol")#` | native entry point | entry length、符号/contract hash 不匹配。 |
| `#zr.ffi.callingConvention("c")#` / `callconv("cdecl")` | 目标调用约定 | 不支持或与 target ABI 不一致。 |
| `#zr.ffi.platform("windows")#` | 平台限定 | 未知/不匹配 platform。 |
| `#zr.ffi.requiredCapabilities(...)#` | FFI capability admission | host/provider 无该 capability。 |
| `#zr.ffi.kind("union")#` | aggregate kind | layout/字段 overlay 不合法。 |
| `#zr.ffi.offset(0)#` | 字段 byte offset | offset、size 或 alignment 不一致。 |
| `#zr.ffi.callbackLifetime("call")#` | callback 可存活范围 | callback policy 缺失或冲突。 |
| `#zr.ffi.errorPolicy("errno" / "returnCode")#` | native failure 信号 | policy 与返回/平台约定不兼容。 |
| `#zr.ffi.cleanup("caller" / "callee")#` | 返回资源清理归属 | allocator/cleanup contract 缺失。 |

不要把 unknown decorator 当成 runtime no-op。它可能在 FFI contract compilation 中产生
`INVALID_POLICY`、`INVALID_ABI`、`INVALID_LAYOUT` 或 `UNSUPPORTED_TYPE` 等状态。

## 从 source 到可调用的 native contract

```text
native extern source
    -> parser: SZrExternBlock + extern function/delegate/aggregate AST
    -> semantic: resolve named TypeRef and parameter directions
    -> FfiContract_Build: SZrNativeImportContract + source-aware diagnostic
    -> FfiContract_Validate: ABI/layout/policy/hash validation
    -> artifact/native registry binding: resolved entry + marshalling descriptor
    -> execution: checked call, result/writeback/error normalization
```

`ZrParser_FfiContract_Build` 接受 semantic context、extern block、具体 declaration、输出
`SZrNativeImportContract` 和 `SZrFfiContractDiagnostic`。diagnostic 不只是 message：它携带
`status`、`parameterIndex` 和 source range，适合 LSP 或 build tool 精确定位。随后
`ZrParser_FfiContract_Validate` 可以验证持久化/反序列化的 contract 是否仍符合当前 ABI。

常见状态应按类别处理，而不是一律重试：

| 状态类别 | 含义 | 建议处理 |
| --- | --- | --- |
| `INVALID_ARGUMENT` / `SCHEMA_MISMATCH` | host 使用 API 或 artifact schema 不对 | 停止，修复调用/版本。 |
| `UNSUPPORTED_TYPE` / `FORBIDDEN_MANAGED_TYPE` | source type 无安全 ABI 表达 | 改为支持的 scalar/blittable aggregate/显式 handle。 |
| `INVALID_DIRECTION` / `INVALID_MARSHALLING` | `ref/out`、pointer 或 writeback 不一致 | 同时检查 declaration、call site 和 C signature。 |
| `CALLBACK_POLICY_REQUIRED` / `INVALID_POLICY` | callback/错误/cleanup 生命周期未声明 | 明确 policy，不能依赖默认猜测。 |
| `INVALID_LAYOUT` / `INVALID_TARGET_ABI` | struct/union/target ABI 事实冲突 | 使用实际 C layout 和目标平台重新生成。 |
| `HASH_MISMATCH` | 源、artifact、动态库或 descriptor 的契约不同 | 重新编译/部署一致的一组，禁止继续执行。 |

## C library、native provider 与 callback 的正确连接方式

### 调用由 `native extern` 声明的 C 符号

1. 在 ZR 中写严格的 extern block，显式标注 entry、direction、layout 和 policy。
2. 编译时建立并验证 `SZrNativeImportContract`；将它与 artifact 一起保存或在 runtime 重新验证。
3. loader 按 library locator 打开目标，解析 entry，比较 ABI/contract hash。
4. runtime 根据 contract marshal 参数、执行 C 调用、写回 `out/ref`、转换错误结果。

此路径的 C 函数签名由具体 contract 决定。不能从页面上的 `i32` 例子推导所有 pointer、struct
return、varargs 或 callback 的平台 ABI；针对真实函数应逐项查看[FFI 契约](../05-interop/ffi-contract.md)
和[Native callback 配方](../05-interop/native-callback-recipes.md)。

### 由宿主注册一个 ZR native module

宿主提供的模块走 native registry，而不是 `native extern`：

```text
create Global/State
    -> ZrLibrary_NativeRegistry_Attach(global)
    -> validate descriptor
    -> ZrLibrary_NativeRegistry_RegisterModule(global, descriptor)
    -> import("provider.module") / resolve call binding
    -> callback receives ZrLibCallContext
```

注册前可调用 `ZrLibrary_NativeRegistry_ValidateModuleDescriptor`，失败后读取
`ZrLibrary_NativeRegistry_GetLastErrorCode` 和 `GetLastErrorMessage`。成功的 descriptor 通常
仍由插件/宿主拥有或为静态存储；不要在注册后立即 free。调用 binding 应通过
`ZrLibrary_NativeRegistry_ResolveCallBinding` 和 `ZrLibrary_NativeCallBinding_GetDescriptorContract`
验证 identity，而不是只用 module/function 文本名称查表。

callback 内使用 `ZrLibCallContext`：先 `ZrLib_CallContext_CheckArity`，再使用
`ReadInt`、`ReadString`、`ReadObject` 等 typed accessor；需要写回 `ref/out` 时调用
`ZrLib_CallContext_WriteBackArgument`。值和临时 managed object 跨越可能 safepoint 的调用时，
用 `ZrLib_TempValueRoot_Begin/End` 或 context 版本 root helper 保活。完整模板见
[原生模块编写](../05-interop/native-module-authoring.md)。

## 常见错误与选择指南

| 目标 | 应使用的机制 | 不应使用的机制 |
| --- | --- | --- |
| 读取 `zr.system`、workspace 或 package 模块 | `import("...")` + project resolver | 手写 `dlopen` 绕过 manifest。 |
| 调一个已有 C 导出符号 | `native extern` + FFI contract | 将函数地址塞进普通 `object` 并强转调用。 |
| 提供新的 ZR module 给多个工程 | native descriptor + registry | 只写 extern block，期待它产生实现。 |
| 回调到 ZR closure | delegate + 明确 callback lifetime | 缓存未 rooted 的 closure/raw pointer。 |
| 将 `out` 结果带回 ZR | `out` declaration + call marker + writeback | 按值传副本后修改 C 局部。 |
| 平台差异处理 | target-aware ABI/decorator + artifact validation | 假设 Windows、Unix、不同编译器共享所有 ABI 细节。 |

当问题来自 import 解析时，先看 project/module-key diagnostic；当问题来自 ABI 时，先看
`SZrFfiContractDiagnostic.status` 和 source range；当问题来自已注册 callback 时，查看 native
registry error 与 call binding identity。这三类失败的修复入口不同，混在一起通常会掩盖真正的
契约问题。
