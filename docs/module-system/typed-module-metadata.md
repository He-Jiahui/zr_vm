---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/module_loader.c
  - zr_vm_core/src/zr_vm_core/execution_dispatch.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer_intermediate.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/module_loader.c
  - zr_vm_core/src/zr_vm_core/execution_dispatch.c
  - zr_vm_parser/src/zr_vm_parser/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer_intermediate.c
plan_sources:
  - user: 2026-03-31 实现 “M6 强类型推断完整闭环计划”
  - .codex/plans/M6 强类型推断完整闭环计划.md
  - .codex/plans/zr_vm阶段化总计划.md
tests:
  - tests/parser/test_type_inference.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_instruction_execution.c
  - tests/scripts/test_artifact_golden.c
  - tests/projects/CMakeLists.txt
  - tests/fixtures/projects/import_binary/fixtures/greet_binary_source.zr
  - tests/fixtures/projects/import_binary/bin/greet.zro
doc_type: module-detail
---

# Typed Module Metadata

## Purpose

M6 的目标不是把局部推断再补几条特判，而是把“编译期知道的类型”真正落成模块产物的一部分，让三条链路用同一份事实源工作：

1. 编译器推断和 overload 选择
2. import 时的 compile-time signature / member inference
3. `.zri` / `.zro` 持久化给后续运行时与 AOT 使用

这轮实现直接升级 `.zro` schema，不保留旧格式双读兼容。

## Related Files

- runtime function metadata
  - `SZrFunction` 在 [zr_vm_core/include/zr_vm_core/function.h](../../zr_vm_core/include/zr_vm_core/function.h) 新增 `typedLocalBindings` 和 `typedExportedSymbols`
  - `function.c` 负责这些数组的生命周期
- binary IO schema
  - `SZrIoFunctionTypedTypeRef`、`SZrIoFunctionTypedLocalBinding`、`SZrIoFunctionTypedExportSymbol` 在 [zr_vm_core/include/zr_vm_core/io.h](../../zr_vm_core/include/zr_vm_core/io.h) 定义
  - `io.c` 负责 `.zro` 读写
- compile-time metadata construction
  - [zr_vm_parser/src/zr_vm_parser/compiler_typed_metadata.c](../../zr_vm_parser/src/zr_vm_parser/compiler_typed_metadata.c)
- import normalization
  - [zr_vm_parser/src/zr_vm_parser/type_inference_import_metadata.c](../../zr_vm_parser/src/zr_vm_parser/type_inference_import_metadata.c)
  - [zr_vm_parser/src/zr_vm_parser/type_inference_native.c](../../zr_vm_parser/src/zr_vm_parser/type_inference_native.c)
- opcode selection and conversions
  - [zr_vm_parser/src/zr_vm_parser/compile_expression.c](../../zr_vm_parser/src/zr_vm_parser/compile_expression.c)
  - [zr_vm_parser/src/zr_vm_parser/compile_expression_types.c](../../zr_vm_parser/src/zr_vm_parser/compile_expression_types.c)
  - [zr_vm_core/src/zr_vm_core/execution_dispatch.c](../../zr_vm_core/src/zr_vm_core/execution_dispatch.c)
- artifact writers
  - [zr_vm_parser/src/zr_vm_parser/writer.c](../../zr_vm_parser/src/zr_vm_parser/writer.c)
  - [zr_vm_parser/src/zr_vm_parser/writer_intermediate.c](../../zr_vm_parser/src/zr_vm_parser/writer_intermediate.c)

## Behavior Model

函数级 typed metadata 统一使用三类记录：

- `SZrFunctionTypedTypeRef`
  - 表示基础值类型、nullable、ownership、数组标记、用户类型名，以及数组元素类型
- `SZrFunctionTypedLocalBinding`
  - 记录局部名、stack slot 和推断出的类型
- `SZrFunctionTypedExportSymbol`
  - 记录导出名、stack slot、access modifier、symbol kind、值类型或返回类型，以及函数参数类型列表

这里没有把类型信息散落进原有运行时字段。`prototypeData` 仍负责 prototype/member 布局，typed metadata 负责“导出的值和局部绑定在编译期是什么类型”。

## Compile-Time Metadata Build

[compiler_typed_metadata.c](../../zr_vm_parser/src/zr_vm_parser/compiler_typed_metadata.c) 把本来只存在于短生命周期 `semanticContext` / `typeEnv` 里的结果汇总回 `SZrFunction`：

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

`.zri` 和 `.zro` 现在共享同一份 metadata 源。

`.zri` 通过 [writer_intermediate.c](../../zr_vm_parser/src/zr_vm_parser/writer_intermediate.c) 输出：

- `TYPE_METADATA`
- `LOCAL_BINDINGS`
- `EXPORTED_SYMBOLS`
- 函数签名文本，例如 `fn add(int, int): int`

`.zro` 通过 `SZrIoFunctionTypedTypeRef` / `SZrIoFunctionTypedLocalBinding` / `SZrIoFunctionTypedExportSymbol` 序列化同样的信息。

这条约束很重要：writer 和 binary schema 不能各自维护一套“看起来差不多”的类型来源。任何 schema 漂移都必须同步更新：

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
- [tests/projects/CMakeLists.txt](../../tests/projects/CMakeLists.txt)
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
