---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
plan_sources:
  - user: 2026-04-04 回到实现侧，把 lambda 路径也显式纳入 childFunctions，补一个更强的不变量检查
  - user: 2026-04-04 拆分边界“final function assembly + invariant validation”独立出去
tests:
  - tests/parser/test_compiler_regressions.c
  - tests/parser/test_execbc_aot_pipeline.c
doc_type: module-detail
---

# Compiler Final Function Assembly

## 目标

`compiler.c` 负责 parser 前端总编排，但“把 `SZrCompilerState` 装配成最终 `SZrFunction`”已经形成独立职责：

- 从编译状态复制 instruction / constant / local / closure / child graph
- 把导出表和源码行范围写回最终函数
- 对 `CREATE_CLOSURE -> childFunctions` 图做最终不变量校验
- 在 `%test` 编译结果里，把 entry function 的 prototype context 重新挂到 detached test function 上

这一层现在收敛到 `compiler_function_assembly.c`，避免 `compiler.c` 继续同时承担：

- script 编译 orchestration
- prototype 序列化
- final function assembly
- compile-result patching
- invariant validation

## 文件边界

### `compiler.c`

保留顶层流程：

- 初始化 `SZrCompilerState`
- 调 `compile_script(...)`
- 处理编译失败 summary
- 调 `compiler_assemble_final_function(...)`
- 继续做 `optimize_instructions(...)`
- 继续做 `SemIR` 构建与 `ExecBC` quickening
- 在 `CompileWithTests` 场景下复制 test function 指针表

### `compiler_function_assembly.c`

只处理最终装配与装配期验证：

- `compiler_assemble_final_function(...)`
- `compiler_attach_detached_function_prototype_context(...)`
- file-local copy helpers
  - instructions
  - constants
  - locals
  - closures
  - child functions
  - exported variables
- child graph invariant check failure转成统一 compiler error

## 装配规则

`compiler_assemble_final_function(...)` 现在显式区分两种入口：

1. script wrapper
   - 需要从 `SZrCompilerState` 复制当前函数 buffers
   - 需要复制 exception metadata slice
   - 会把 entry signature 固定成 `0` 参数、非 varargs
2. top-level function declaration
   - 不重复覆盖 declaration path 已经装配好的 instruction / constant / local / closure buffers
   - 仍然会补 child graph、export table、源码范围和最终不变量校验
   - 保留 declaration path 已经写好的参数签名

这让 `ZrParser_Compiler_Compile(...)` 和 `ZrParser_Compiler_CompileWithTests(...)` 可以共享同一套 final assembly 逻辑。

## 不变量

最终装配阶段会调用 core 层的：

- `ZrCore_Function_RebindConstantFunctionValuesToChildren(...)`
- `ZrCore_Function_ValidateCreateClosureTargetsInChildGraph(...)`

含义是：

- function-valued constants 先尽量重绑到最终 child function tree
- 每个 `CREATE_CLOSURE` 指向的函数常量都必须能从当前函数的 `childFunctionList` 图递归找到

因此 AOT / runtime 不再允许依赖“constant graph 还能把漏掉的 child function 找回来”这种结构偶然性。

## `%test` 场景

`CompileWithTests` 仍然允许 test function 作为 detached function 存在，但在 final assembly 之后要补一层 prototype context：

- 如果 entry function 带 `prototypeData`
- 且 test function 的 constant pool 里还没有 entry function 引用
- 就把 entry function 追加到 test function 常量池

这样 test body 在运行时仍然能看到脚本入口已经建立好的类型 prototype 上下文。

## 当前验证

本次拆分后实际重跑的验证：

```powershell
wsl.exe bash -lc "cd /mnt/d/Git/Github/zr_vm_mig/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_compiler_regressions_test zr_vm_execbc_aot_pipeline_test -j8 && export LD_LIBRARY_PATH=$PWD/build/codex-wsl-gcc-debug/lib && ./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_regressions_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_execbc_aot_pipeline_test"
wsl.exe bash -lc "cd /mnt/d/Git/Github/zr_vm_mig/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_compiler_regressions_test zr_vm_execbc_aot_pipeline_test -j8 && export LD_LIBRARY_PATH=$PWD/build/codex-wsl-clang-debug/lib && ./build/codex-wsl-clang-debug/bin/zr_vm_compiler_regressions_test && ./build/codex-wsl-clang-debug/bin/zr_vm_execbc_aot_pipeline_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake -S . -B build\codex-msvc-ninja-cli-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-msvc-ninja-cli-debug --target zr_vm_cli_executable --parallel 8
.\build\codex-msvc-ninja-cli-debug\bin\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

验收点：

- nested lambda child graph regression 通过
- AOT child thunk pipeline 不回退
- strict standalone AOT C lowering（无 embedded blob）在 entry/child 含 unsupported 指令时会直接失败；CLI / project `--emit-aot-c` 因为携带 embedded blob，仍会保留 shim fallback
- gcc / clang 结果一致
- Windows MSVC `cl + Ninja` smoke 仍能输出 `hello world`
