---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/module/module_internal.h
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_internal.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_ffi.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/module/module_internal.h
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_ffi.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
plan_sources:
  - user: 2026-03-31 实现 “M6 强类型推断完整闭环计划”
  - .codex/plans/M6 强类型推断完整闭环计划.md
  - .codex/plans/zr_vm阶段化总计划.md
  - user: 2026-04-06 struct 值类型与 native wrapper 分层方案
  - user: 2026-04-06 新的 source-level wrapper decorator surface 和具体 handle_id lowering runtime完善
tests:
  - tests/parser/test_type_inference.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_prototype.c
  - tests/parser/test_instruction_execution.c
  - tests/ffi/test_ffi_module.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/scripts/test_artifact_golden.c
  - tests/cmake/run_projects_suite.cmake
  - tests/fixtures/projects/import_binary/fixtures/greet_binary_source.zr
  - tests/fixtures/projects/import_binary/bin/greet.zro
  - tests/module/test_module_system.c
doc_type: module-detail
---

# Typed Module Metadata

## Purpose

M6 的目标不是把局部推断再补几条特判，而是把“编译期知道的类型”真正落成模块产物的一部分，让三条链路用同一份事实源工作：

1. 编译器推断和 overload 选择
2. import 时的 compile-time signature / member inference
3. `.zro` 持久化正式 typed metadata，`.zri` 保留 debug / intermediate 镜像给后续运行时与 AOT 验证使用

这轮实现直接升级 `.zro` schema，不保留旧格式双读兼容。

## Related Files

- runtime function metadata
  - `SZrFunction` 在 [zr_vm_core/include/zr_vm_core/function.h](../../zr_vm_core/include/zr_vm_core/function.h) 新增 `typedLocalBindings` 和 `typedExportedSymbols`
  - `function.c` 负责这些数组的生命周期
- binary IO schema
  - `SZrIoFunctionTypedTypeRef`、`SZrIoFunctionTypedLocalBinding`、`SZrIoFunctionTypedExportSymbol` 在 [zr_vm_core/include/zr_vm_core/io.h](../../zr_vm_core/include/zr_vm_core/io.h) 定义
  - `io.c` 负责 `.zro` 读写
- compile-time metadata construction
  - [zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c)
- import normalization
  - [zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c](../../zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c)
  - [zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c](../../zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c)
- opcode selection and conversions
  - [zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c)
  - [zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c)
  - [zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c](../../zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c)
- artifact writers
  - [zr_vm_parser/src/zr_vm_parser/writer.c](../../zr_vm_parser/src/zr_vm_parser/writer.c)
  - [zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c](../../zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c)

## Behavior Model

函数级 typed metadata 统一使用三类记录：

- `SZrFunctionTypedTypeRef`
  - 表示基础值类型、nullable、ownership、数组标记、用户类型名，以及数组元素类型
- `SZrFunctionTypedLocalBinding`
  - 记录局部名、stack slot 和推断出的类型
- `SZrFunctionTypedExportSymbol`
  - 记录导出名、stack slot、access modifier、symbol kind、值类型或返回类型，以及函数参数类型列表
  - 记录导出声明的源码行列范围，供 `.zro` 消费侧做 symbol-level definition / references / document highlight

这里没有把类型信息散落进原有运行时字段。`prototypeData` 仍负责 prototype/member 布局，typed metadata 负责“导出的值和局部绑定在编译期是什么类型”。

## Compile-Time Metadata Build

[compiler_typed_metadata.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c) 把本来只存在于短生命周期 `semanticContext` / `typeEnv` 里的结果汇总回 `SZrFunction`：

- 局部变量优先从 `typeEnv` 查推断类型
- 顶层函数声明如果已经离开原始表达式路径，仍会回填为 `function` / `closure`
- 导出变量从 `typeEnv` 查值类型
- 导出函数从 AST declaration 读取返回类型和参数类型

结果是：script entry function 自身就能携带 import 所需的基础签名信息，而不需要保留完整 AST 或执行模块。

## Import Normalization Flow

`type_inference_import_metadata.c` 把三类 import 统一映射到同一套 compile-time prototype/type metadata：

1. native import
   - 继续从 `__zr_native_module_info` 拿原始声明
   - 再归一化成编译器自己的 prototype/member/type 视图
2. source import
   - 走“只编译不执行”的装载路径
   - 直接读取被导入脚本的 `typedExportedSymbols`、`typedLocalBindings` 和 prototype data
3. binary import
   - 通过 `.zro` 读出 `SZrIoFunctionTyped*` 结构
   - 恢复成和 source import 一致的导出符号与 prototype/member 信息

统一之后，编译器不再关心模块来自 native、source 还是 binary，只看：

- 导出了哪些变量 / 函数
- 函数参数与返回类型是什么
- prototype 上有哪些字段 / 方法
- 成员调用返回什么类型

## Native FFI Wrapper Metadata

native module `types[]` entry 现在还会额外公开一组 FFI wrapper 字段，用来描述“这个 native type 在 FFI 边界该怎么 lowering”：

- `ffiLoweringKind`
- `ffiViewTypeName`
- `ffiUnderlyingTypeName`
- `ffiOwnerMode`
- `ffiReleaseHook`

这组字段在两个层面同时存在：

- module info object 的公开 metadata，供 `%type` / LSP / 调试器 / 测试读取
- runtime prototype 上的隐藏字段 `__zr_ffi*`，供 marshalling helper 直接读取

当前仓库里已经接入的 built-in wrapper 例子：

- `PointerHandle`
  - `ffiLoweringKind = "pointer"`
  - `ffiOwnerMode = "borrowed"`
- `BufferHandle`
  - `ffiLoweringKind = "pointer"`
  - `ffiOwnerMode = "owned"`

因此 typed metadata 现在不只描述“一个 type 叫什么、有哪些成员”，还会描述“它是不是一个带 native lowering 语义的 wrapper type”。

这条信息目前主要服务两条链路：

- `zr.ffi` runtime 在参数 marshalling 时判定 wrapper 是否允许直接降低为 native pointer
- parser/type inference 在 `%extern` 函数 overload 选择时，只对 FFI/native boundary 放宽 wrapper-compatible 参数检查

同一套 metadata 现在也支持 source-level wrapper class，而不是只覆盖 native registry 内建类型。

## Source Wrapper Metadata Bridge

source module 里的 wrapper class 现在可以直接声明：

```zr
#zr.ffi.lowering("handle_id")#
#zr.ffi.viewType("ModeHandleView")#
#zr.ffi.underlying("i32")#
#zr.ffi.ownerMode("borrowed")#
#zr.ffi.releaseHook("close_mode_handle")#
class ModeHandle {
    var handleId: i32;
}
```

这组 decorator 在 metadata 管道里的流向已经打通成一条闭环：

1. `compiler_class.c`
   - 把 `zr.ffi.*` wrapper decorators 编译成 type decorator metadata object
   - 同时把 decorator 名称写进 prototype decorator name list
2. `prototypeData`
   - 通过 `SZrCompiledPrototypeInfo.hasDecoratorMetadata` / `decoratorMetadataConstantIndex` 持久化
3. `%type(...)` / reflection
   - source type reflection 的 `metadata` 和 `decorators[]` 可以直接读到 `ffiLoweringKind` / `ffiViewTypeName` / `ffiUnderlyingTypeName` / `ffiOwnerMode` / `ffiReleaseHook`
4. `module_prototype.c`
   - 在 runtime materialize prototype 时，再把这组公开 metadata 桥接成隐藏字段：
     - `__zr_ffiLoweringKind`
     - `__zr_ffiViewTypeName`
     - `__zr_ffiUnderlyingTypeName`
     - `__zr_ffiOwnerMode`
     - `__zr_ffiReleaseHook`

这样 source wrapper 和 native descriptor wrapper 在 runtime 上共享同一套 hidden metadata contract。

## `handle_id` In Typed Metadata

`handle_id` 是这轮补全的第三种 lowering kind（除了已有的 `pointer` 与未来的 `value`）：

- compile-time/type inference 层：
  - 当 extern/native boundary 目标参数是整数标量，且 wrapper metadata 的
    - `ffiLoweringKind == "handle_id"`
    - `ffiUnderlyingTypeName` 与目标整数类型同名
    时，overload 选择和参数检查会把 wrapper 视为边界兼容
  - source-level wrapper decorator 当前只接受固定宽度整数名
    `i8/u8/i16/u16/i32/u32/i64/u64` 作为 `ffiUnderlyingTypeName`
  - 若声明了 `ffiViewTypeName`，source wrapper decorator 现在要求它指向同一 source file 中的
    `%extern struct`
  - 普通 zr 函数调用仍然不兼容，不会隐式转成整数
- runtime marshalling 层：
  - `ffi_runtime_callback.c` 在构造 scalar argument 时读取 prototype hidden metadata
  - 若 lowering kind 为 `handle_id` 且目标 ABI 标量类型匹配 underlying 名称，则从对象字段
    - `__zr_ffi_handleId`
    - 或 `handleId`
    取值并降为 ABI 整数

这让 typed metadata 不再只是“类型说明书”，而是 source/native wrapper 共享的 lowering contract。

## Typed Opcode Selection

M6 的 codegen 行为是“先推断，再转化，再选 opcode”。

当前已收口的路径包括：

- 二元算术
- 比较
- 赋值
- 自由函数调用
- imported module 成员调用

具体规则：

- 若两侧都是已知 `int`，生成 `ADD_INT` / 对应整数比较指令
- 若目标签名要求 `float`，先插 `TO_FLOAT`，再生成 typed call
- 若类型未知、动态成员访问、或当前路径无法给出稳定签名，退回现有 generic opcode / meta fallback

这意味着 typed path 只在“真的知道类型”时变精确，不会为了追求闭环而引入兼容分支。

## Artifact Model

LSP / compiler 侧正式消费的 typed metadata 载体固定为 `.zro`。

`.zro` 通过 `SZrIoFunctionTypedTypeRef` / `SZrIoFunctionTypedLocalBinding` / `SZrIoFunctionTypedExportSymbol` 序列化 typed metadata，
其中 `typedExportedSymbols` 额外持久化 declaration line/column span，供 binary metadata declaration resolver 直接命中导出符号，而不是只能回退到 module entry。

`.zri` 继续由 [writer_intermediate.c](../../zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c) 输出，保留：

- `TYPE_METADATA`
- `LOCAL_BINDINGS`
- `EXPORTED_SYMBOLS`
- 函数签名文本，例如 `fn add(int, int): int`

但 `.zri` 的职责已经收窄为 debug / intermediate artifact：

- 便于人读和 golden 回归
- 可承载与 `.zro` 对齐的 debug 线列范围信息
- 不再作为 language server / type inference / import metadata 的正式事实源

这条约束很重要：writer artifact 和 binary schema 可以表达相近信息，但语义入口只能有一条正式机器路径。任何 `.zro` schema 漂移都必须同步更新：

- `tests/golden/intermediate/*.zri`
- `tests/golden/binary/*.zro`
- `tests/fixtures/projects/import_binary/bin/greet.zro`

## Runtime Constraint For Typed Calls

typed opcode 只在运行时也守住调用栈边界时才成立。

本轮暴露出的底层约束是：`TO_INT` / `TO_UINT` / `TO_FLOAT` 走 meta conversion 时，scratch 区不能从 `state->stackTop` 直接复用。因为 `stackTop` 可能仍落在当前帧的 live temp 区间之前，错误复用会覆盖掉 imported callable 所在槽位。

当前修复方式与 `TO_BOOL` 保持一致：

- meta call scratch 上界使用 `callInfo->functionTop.valuePointer`
- 标记使用安全 scratch 区域

这样 `math.add(1, 2.5)` 这类 imported typed call 的 `TO_FLOAT` 不会再把 callable 自己覆盖成 `float`。

## Edge Cases And Constraints

- 没有可恢复类型的导出变量仍会降级为 `object`
- 数组类型元数据当前只记录统一元素类型视图，符合 M6 现阶段的数组元素推断需求
- `typedExportedSymbols` 只描述模块对外可见的符号，不替代 prototypeData
- 本轮不实现 typed IR / AOT backend，只保证元数据稳定落盘并可被 import / writer / runtime 验证
- 旧 `.zro` 不兼容，本轮直接重生成仓库内 fixture

## Test Coverage

M6 的验证不是“编译成功”级别，而是直接断言 opcode、签名和运行结果。

- [tests/parser/test_type_inference.c](../../tests/parser/test_type_inference.c)
  - source import 签名 pass / fail
  - binary import 签名 pass / fail
  - imported member chain 返回类型
  - imported array assignment 类型拒绝
- [tests/parser/test_compiler_features.c](../../tests/parser/test_compiler_features.c)
  - mixed-type call 的 `TO_FLOAT`
  - `.zri` `TYPE_METADATA` section
  - opcode 精确选择
- [tests/parser/test_instruction_execution.c](../../tests/parser/test_instruction_execution.c)
  - source import typed call 真实执行结果
  - binary import typed call 真实执行结果
- [tests/scripts/test_artifact_golden.c](../../tests/scripts/test_artifact_golden.c)
  - `.zrs` / `.zri` / `.zro` golden 回归
- [tests/cmake/run_projects_suite.cmake](../../tests/cmake/run_projects_suite.cmake)
  - `hello_world`
  - `import_basic`
  - `import_binary`
  - `import_pub_function`

## Plan Sources

本实现直接对应：

- `M6 强类型推断完整闭环计划`
- `zr_vm阶段化总计划` 中关于强类型推断、import 和产物闭环的阶段目标

## Open Follow-Up

- AOT / typed IR 仍未消费这批元数据
- 更细粒度的 generic / union / 多元素数组类型表达还没有进入 `.zro` schema
- `import_pub_function` fixture 仍带有与本轮无关的旧语法诊断噪音，当前不影响结果验证，但后续应独立清理
