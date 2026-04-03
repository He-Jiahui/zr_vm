---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/execution_dispatch.c
  - zr_vm_parser/src/zr_vm_parser/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot.c
  - zr_vm_parser/src/zr_vm_parser/writer_intermediate.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/src/zr_vm_core/execution_dispatch.c
  - zr_vm_parser/src/zr_vm_parser/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot.c
  - zr_vm_parser/src/zr_vm_parser/writer_intermediate.c
plan_sources:
  - user: 2026-04-03 实现 ZR 三层 IR 与 Ownership/AOT 字节码重构方案，并继续推进 dynamic/meta 指令族、quickening/superinstruction 生成器
  - user: 2026-04-03 继续当前动态迭代 superinstruction slice，保持 SemIR/AOT 契约稳定
tests:
  - tests/parser/test_dynamic_iteration_pipeline.c
  - tests/parser/test_semir_pipeline.c
  - tests/parser/test_execbc_aot_pipeline.c
  - tests/fixtures/projects/hello_world/hello_world.zrp
doc_type: module-detail
---

# Dynamic Iteration `SemIR -> ExecBC -> AOT`

## 目标

这条链路把 `foreach` 的动态对象回退路径拆成三层职责：

- `SemIR`
  - 保留稳定语义契约
  - 明确记录 `DYN_ITER_INIT` / `DYN_ITER_MOVE_NEXT`
  - 标记 `DYNAMIC_RUNTIME` effect 和 deopt requirement
- `ExecBC`
  - 允许做解释执行导向的 quickening
  - 当前只融合一条窄模式 superinstruction
- `AOT`
  - 只消费 `SemIR`
  - 不读取 quickened ExecBC
  - 继续通过 runtime contract 调用 `ZrCore_Object_IterInit` / `ZrCore_Object_IterMoveNext`

这个边界的目的不是把动态语义“编回旧 helper call”，而是让稳定语义、解释执行快路径、AOT 后端各自拥有独立演进面。

## Foreach Lowering

当 `foreach_should_use_dynamic_iterator_ops(...)` 发现 iterable 推断结果退回到“无静态布局名、无 element type 的 object fallback”时，`compile_statement_flow.c` 会发出：

1. `DYN_ITER_INIT`
   - `operandExtra = iteratorSlot`
   - `operand1[0] = iterableSlot`
2. `DYN_ITER_MOVE_NEXT`
   - `operandExtra = moveNextSlot`
   - `operand1[0] = iteratorSlot`
3. `JUMP_IF`
   - `operandExtra = moveNextSlot`
   - `operand2[0] = break-target relative offset`
4. `ITER_CURRENT`
   - 从 iterator 读取当前元素并绑定到 foreach pattern

因此 `moveNext` 结果既是 runtime iterator contract 的产物，也是循环退出判定值。

## SemIR Contract

`compiler_semir.c` 对这两个动态迭代 opcode 的映射保持稳定：

- `DYN_ITER_INIT -> ZR_SEMIR_OPCODE_DYN_ITER_INIT`
- `DYN_ITER_MOVE_NEXT -> ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT`

并且二者都固定为：

- `effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME`
- `needsDeopt = ZR_TRUE`

这意味着：

- `.zri` 中必须还能看到原始动态迭代语义
- `.zro` roundtrip 和后续 AOT backend 不会因为 ExecBC quickening 丢失动态站点信息
- 解释器快路径可以升级，但不能改写 SemIR 对 runtime contract 的表达

## ExecBC Quickening

`compiler_quickening.c` 当前只允许一个窄融合模式：

```text
DYN_ITER_MOVE_NEXT dst=moveNextSlot, iter=iteratorSlot
JUMP_IF cond=moveNextSlot, offset=break
```

满足以下条件时才融合：

- 第一条必须是 `DYN_ITER_MOVE_NEXT`
- 第二条必须是 `JUMP_IF`
- `JUMP_IF.operandExtra == DYN_ITER_MOVE_NEXT.operandExtra`
- `JUMP_IF` 的相对跳转偏移能装入 signed 16-bit

融合后第一条改写成：

```text
SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE
```

编码约定：

- `operandExtra`
  - `moveNextSlot`
- `operand1[0]`
  - `iteratorSlot`
- `operand1[1]`
  - signed 16-bit `jump_if_false_offset`

当前实现保留跟随的原始 `JUMP_IF` 指令槽位，但执行时通过 `DONE_SKIP(2)` 跨过它。这样可以在不重排指令流和不重写 label patch 逻辑的前提下，稳定引入 superinstruction。

## Runtime Dispatch Semantics

`execution_dispatch.c` 为 superinstruction 定义了专门解释路径：

1. 对 `iteratorSlot` 调用 `ZrCore_Object_IterMoveNext`
2. 把结果写入 `moveNextSlot`
3. 复用与 `JUMP_IF` 相同的 truthiness 规则
4. 条件为假时把缓存的相对偏移直接加到 `programCounter`
5. `DONE_SKIP(2)` 跳过后继原始 `JUMP_IF`

为避免再次在大 dispatch switch 里复制一段条件求真逻辑，这一轮把 `JUMP_IF` 与 superinstruction 统一收敛到 `execution_is_truthy(...)` helper。

## AOT Boundary

`backend_aot.c` 继续从 `SemIR` 降低，不读取 quickened ExecBC，因此：

- AOT C 文本仍声明 `DYN_ITER_INIT` / `DYN_ITER_MOVE_NEXT`
- AOT LLVM 文本仍声明 `@ZrCore_Object_IterInit` / `@ZrCore_Object_IterMoveNext`
- quickening 只影响解释器热路径，不影响 AOT artifact 的稳定契约

这条规则也限制了后续 superinstruction 扩展方向：

- 可以继续在 ExecBC 增加更细的 adaptive/superinstruction 形式
- 不能让 AOT backend 直接依赖这些 quickened opcode

## 当前限制

本轮故意没有做这些事：

- 不恢复旧的名字驱动 dynamic access quickening
- 不把 `GET_MEMBER` / `GET_BY_INDEX` / `GET_GLOBAL` 回写成兼容 opcode
- 不让 `SemIR` 输出 superinstruction
- 不让 AOT backend 感知 `SUPER_*` opcode

原因是仓库现有测试已经把“显式访问语义保留在产物中”当成硬约束，而 dynamic foreach guard fusion 目前是唯一低风险且能证明 `SemIR` 与 `ExecBC` 解耦的 quickening 切片。

## 验证证据

本轮实际跑过的验证：

```powershell
wsl bash -lc "cmake --build build/codex-wsl-gcc-debug --target zr_vm_dynamic_iteration_pipeline_test zr_vm_semir_pipeline_test zr_vm_execbc_aot_pipeline_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_dynamic_iteration_pipeline_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_semir_pipeline_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_execbc_aot_pipeline_test"
wsl bash -lc "cmake --build build/codex-wsl-clang-debug --target zr_vm_dynamic_iteration_pipeline_test zr_vm_semir_pipeline_test zr_vm_execbc_aot_pipeline_test -j 8 && ./build/codex-wsl-clang-debug/bin/zr_vm_dynamic_iteration_pipeline_test && ./build/codex-wsl-clang-debug/bin/zr_vm_semir_pipeline_test && ./build/codex-wsl-clang-debug/bin/zr_vm_execbc_aot_pipeline_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8
.\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

验收点：

- dynamic foreach 专项测试通过
- `.zri` 同时保留 `DYN_ITER_MOVE_NEXT` 的 SemIR 文本和 `SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE` 的 ExecBC 文本
- `test_semir_pipeline.c` 通过，说明 SemIR 表和 roundtrip 没被 quickening 破坏
- `test_execbc_aot_pipeline.c` 通过，说明 AOT artifact 仍只表达 SemIR 稳定契约
- Windows MSVC CLI smoke 仍能输出 `hello world`
