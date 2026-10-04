---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_test.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
plan_sources:
  - user: 2026-04-04 回到实现侧，把 lambda 路径也显式纳入 childFunctions，补一个更强的不变量检查
  - user: 2026-04-04 拆分边界“final function assembly + invariant validation”独立出去
tests:
  - tests/parser/test_compiler_regressions.c
  - tests/parser/test_escape_metadata_pipeline.c
doc_type: module-detail
---

# Compiler Final Function Assembly

## 目标

`compiler.c` 负责 parser 前端总编排，但“把 `SZrCompilerState` 装配成最终 `SZrFunction`”已经形成独立职责：

- 从编译状态复制 instruction / constant / local / closure / child graph
- 把导出表和源码行范围写回最终函数
- 对 `CREATE_CLOSURE -> childFunctions` 图做最终不变量校验

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
- 先做 `optimize_instructions(...)`
- 调 `compiler_assemble_final_function(...)`
- 继续做 `SemIR` 构建与 `ExecBC` quickening
- 不再维护特殊 test function 指针表

### `compiler_function_assembly.c`

只处理最终装配与装配期验证：

- `compiler_assemble_final_function(...)`
- 保留 `compiler_attach_detached_function_prototype_context(...)` helper；当前 `CompileTest` 编排路径不调用它
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

原特殊测试编译入口已随 `%test` 破坏性切换删除。普通脚本和顶层函数声明仍共享同一套 final assembly 逻辑；后续测试发现只能由普通函数 metadata 和 typed `TestManifest` 接入。

## 不变量

最终装配阶段会调用 core 层的：

- `ZrCore_Function_RebindConstantFunctionValuesToChildren(...)`
- `ZrCore_Function_ValidateCreateClosureTargetsInChildGraph(...)`

含义是：

- function-valued constants 先尽量重绑到最终 child function tree
- 每个 `CREATE_CLOSURE` 指向的函数常量都必须能从当前函数的 `childFunctionList` 图递归找到

因此 AOT / runtime 不再允许依赖“constant graph 还能把漏掉的 child function 找回来”这种结构偶然性。

## 测试函数边界

当前 `CompileTest` 以 `emitTestManifest=true` 进入同一编译编排；测试是带 `#zr.testing.test#` 静态 metadata 的普通 `fn`，不使用旧 `%test` 指针表。`compiler_test_finalize_manifest` 对未开启或空 entries 返回成功且不生成清单；非空 entries 经 Encode 后把字节挂到 `cs.currentFunction`。因此“compiler 尚不生成清单”已不适用于这条现行路径；此局部发布不宣称项目测试执行或完整 Gate14 已验收。

## 编译调用的阶段与失败边界

编译状态借用 AST、module key 与 submission context；临时状态释放不等于销毁成功返回的 VM 函数。模式入口读取 provider phase，再切到 TEST 或 RUNTIME，active 返回函数或 NULL 后恢复原值；实际字段为 `state.nativeProviderPhase`，getter 对非法值回退 RUNTIME。`TODO:` 这段恢复只覆盖普通返回，需沿 VM 非局部异常入口核实阶段恢复，不能把 NULL 失败处理等同异常安全保证。

当前顺序是 final assembly、SemIR/ExecBC 后调用 `FinalizeCurrentSourceModule`，随后才完成 call bindings 与 submission result 发布。后两步仍可失败并释放函数。`TODO:` 沿 summary 发布与失败清理核实是否需要回滚，不能由“函数未返回”推断已经发布的模块摘要自动撤销。


## 当前验证

以下为原拆分时记录的验证；本次调用契约同步没有追加构建或运行收据：

```powershell
wsl.exe bash -lc "cd /mnt/d/Git/Github/zr_vm_mig/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_compiler_regressions_test zr_vm_semir_pipeline_test -j8 && export LD_LIBRARY_PATH=$PWD/build/codex-wsl-gcc-debug/lib && ./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_regressions_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_semir_pipeline_test"
wsl.exe bash -lc "cd /mnt/d/Git/Github/zr_vm_mig/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_compiler_regressions_test zr_vm_semir_pipeline_test -j8 && export LD_LIBRARY_PATH=$PWD/build/codex-wsl-clang-debug/lib && ./build/codex-wsl-clang-debug/bin/zr_vm_compiler_regressions_test && ./build/codex-wsl-clang-debug/bin/zr_vm_semir_pipeline_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake -S . -B build\codex-msvc-ninja-cli-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-msvc-ninja-cli-debug --target zr_vm_cli_executable --parallel 8
.\build\codex-msvc-ninja-cli-debug\bin\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

验收点：

- nested lambda child graph regression 通过
- SemIR/最终装配主链路在 interp/binary 入口下不回退
- gcc / clang 结果一致
- Windows MSVC `cl + Ninja` smoke 仍能输出 `hello world`
