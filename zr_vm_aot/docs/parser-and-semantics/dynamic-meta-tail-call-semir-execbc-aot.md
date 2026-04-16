---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_tail_call.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_tail_call.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
plan_sources:
  - user: 2026-04-03 实现 ZR 三层 IR 与 Ownership/AOT 字节码重构方案，并继续推进 dynamic/meta 指令族、quickening/superinstruction 生成器
  - user: 2026-04-03 继续往后做 AOTIR/LLVM/C backend、dynamic/meta 新指令族、quickening/superinstruction 生成器
tests:
  - tests/parser/test_meta_call_pipeline.c
  - tests/parser/test_tail_call_pipeline.c
  - tests/parser/test_semir_pipeline.c
  - tests/parser/test_execbc_aot_pipeline.c
  - tests/fixtures/projects/hello_world/hello_world.zrp
doc_type: module-detail
---

# Dynamic And Meta Tail Call `SemIR -> ExecBC -> AOT`

## 目标

这一轮把调用站点里的 dynamic/meta 语义继续从“运行时兜底行为”提升成稳定 IR 契约：

- 非 tail position 的 `@call` 仍显式保留为 `META_CALL`
- tail position 上的“未知可调用值”显式保留为 `DYN_TAIL_CALL`
- tail position 上的 `@call` receiver 显式保留为 `META_TAIL_CALL`

核心目的不是新增两个名字，而是避免 `return callable(...)` 这类站点在 `SemIR` / AOT artifact 中退化成普通 `FUNCTION_TAIL_CALL`，从而丢失 dynamic/meta dispatch 信息。

## Source Lowering

`compile_expression_types.c` 现在在调用 lowering 时按这三个维度分流：

1. 是否处于 tail-call context
2. 是否存在显式 `@call` 元方法
3. 是否能在编译期解析为普通函数 / 方法调用

最终规则是：

- 非 tail + `@call` 存在: `META_CALL`
- tail + `@call` 存在: `META_TAIL_CALL`
- 非 tail + 无法静态解析 callable: `DYN_CALL`
- tail + 无法静态解析 callable: `DYN_TAIL_CALL`
- 其余静态可解析路径继续走 `FUNCTION_CALL` / `FUNCTION_TAIL_CALL`

`compile_statement.c` 也同步把 `DYN_TAIL_CALL` / `META_TAIL_CALL` 认作合法 tail-site 返回值来源，因此 `return expr;` 不会再把这些调用误判成普通表达式结果槽。

## SemIR Contract

`compiler_semir.c` 为这四个 dynamic/meta 调用站点都建立稳定映射：

- `DYN_CALL -> ZR_SEMIR_OPCODE_DYN_CALL`
- `DYN_TAIL_CALL -> ZR_SEMIR_OPCODE_DYN_TAIL_CALL`
- `META_CALL -> ZR_SEMIR_OPCODE_META_CALL`
- `META_TAIL_CALL -> ZR_SEMIR_OPCODE_META_TAIL_CALL`

四者统一具备：

- `effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME`
- `needsDeopt = true`

因此：

- `.zri` 能区分普通动态调用与 tail-site 动态调用
- `.zri` 能区分普通 `@call` 与 tail-site `@call`
- AOT backend 可以继续只依赖 `SemIR`，而不是猜测 ExecBC 上的调用语义

## ExecBC Runtime Semantics

`execution_dispatch.c` 为新增 opcode 定义了专门解释路径：

- `DYN_TAIL_CALL`
  - 与 `FUNCTION_TAIL_CALL` 一样进入 tail-call 分派路径
  - 优先尝试 `execution_try_reuse_tail_call_frame(...)`
  - 复用失败时再回退到统一的 `ZrCore_Function_PreCall(...)`
  - 对象值如果带 `@call`，frame-reuse helper 会先原地物化 meta-call target，再复用同一 frame
- `META_TAIL_CALL`
  - 先执行 `execution_prepare_meta_call_target(...)`
  - 然后优先尝试 `execution_try_reuse_tail_call_frame(...)`
  - 只有复用失败时才回退到 `PreCall` 新建下一层 `callInfo`

真正的 frame reuse 现在已经落到 runtime：

- `ZrCore_Function_TryReuseTailVmCall(...)` 会把 callee 和参数整体搬回当前 frame base
- 当前 frame 的 open upvalues 会先关闭，再覆盖旧局部槽位
- 最终返回值目标会显式保留，不再依赖“当前 `functionBase` 就是原返回槽”这个旧假设

这样 tail-site 语义不仅在 ExecBC 与 SemIR 上显式化，而且在安全场景里已经变成真正的 VM frame recycling。

## Frame Reuse Guards

为了不破坏异常与 deterministic cleanup 语义，只有这些条件同时满足时才允许复用当前 `callInfo`：

- 当前 frame 是 VM frame，而不是 native frame
- 当前 frame 没有活动中的异常处理器
- 当前 frame 没有待执行的 `to-be-closed` / `%using` cleanup 注册
- callee 最终能解析成 VM function / closure，或能先通过 `@call` 物化成 VM callee

不满足时仍回退到旧的 `PreCall -> next callInfo` 路径，因此：

- `return g()` 落在 `try/catch/finally` 保护区内时不会错误跳过当前 frame 的异常语义
- `%using` / close-meta 仍按原来的单次 cleanup 顺序执行
- native callable 继续走旧路径，不混入这轮 VM-only frame reuse

## AOT Boundary

`backend_aot.c` 继续只从 `SemIR` 构造文本化 AOTIR：

- `DYN_TAIL_CALL` / `META_TAIL_CALL` 都会写入 AOT instruction listing
- 二者都声明共享的 `FUNCTION_PRECALL` runtime contract
- LLVM 与 C 后端都不会窥探 quickened ExecBC

这保持了两个边界：

- tail-site dynamic/meta 语义在 AOT artifact 中可见
- AOT 后端仍然不重新发明 callable 解析语义，只依赖 `ZrCore_Function_PreCall(...)`

## 当前边界

这一轮已经把三个层次都打通，但边界仍明确保留：

- AOT artifact 仍只保留稳定 semantic opcode，不暴露 ExecBC 的 frame reuse 细节
- native callable 仍走统一 `PreCall` 慢路径，不参加 VM frame reuse
- 有活动异常处理器或 `%using` cleanup 的 frame 仍显式回退，不做激进折叠

## 验证证据

本轮实际跑过的验证：

```powershell
wsl bash -lc "cmake --build build/codex-wsl-gcc-debug --target zr_vm_tail_call_pipeline_test zr_vm_meta_call_pipeline_test zr_vm_dynamic_iteration_pipeline_test zr_vm_semir_pipeline_test zr_vm_execbc_aot_pipeline_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_tail_call_pipeline_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_meta_call_pipeline_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_dynamic_iteration_pipeline_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_semir_pipeline_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_execbc_aot_pipeline_test"
wsl bash -lc "cmake --build build/codex-wsl-clang-debug --target zr_vm_tail_call_pipeline_test zr_vm_meta_call_pipeline_test zr_vm_dynamic_iteration_pipeline_test zr_vm_semir_pipeline_test zr_vm_execbc_aot_pipeline_test -j 8 && ./build/codex-wsl-clang-debug/bin/zr_vm_tail_call_pipeline_test && ./build/codex-wsl-clang-debug/bin/zr_vm_meta_call_pipeline_test && ./build/codex-wsl-clang-debug/bin/zr_vm_dynamic_iteration_pipeline_test && ./build/codex-wsl-clang-debug/bin/zr_vm_semir_pipeline_test && ./build/codex-wsl-clang-debug/bin/zr_vm_execbc_aot_pipeline_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8
.\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

验收点：

- `test_tail_call_pipeline.c` 证明 `DYN_TAIL_CALL` / `META_TAIL_CALL` 会进入 `.zri`、SemIR 和 AOT artifact，并且深尾递归后 `callInfo` 链保持有界
- `test_meta_call_pipeline.c` 继续约束非 tail `@call` 仍使用 `META_CALL`
- `test_dynamic_iteration_pipeline.c` / `test_semir_pipeline.c` / `test_execbc_aot_pipeline.c` 继续证明现有 dynamic iteration、SemIR 和 AOT 边界没有被 tail-call 扩展破坏
- Windows MSVC CLI smoke 仍必须保持 `hello world`
