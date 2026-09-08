---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_emitter.c
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c
  - zr_vm_aot/tests/parser/test_execbc_aot_manual_opcode_sync.c
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_emitter.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
tests:
  - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c
  - zr_vm_aot/tests/parser/test_execbc_aot_manual_opcode_sync.c
  - zr_vm_aot/tests/parser/test_known_call_pipeline.c
  - zr_vm_aot/tests/parser/test_meta_call_pipeline.c
  - zr_vm_aot/tests/parser/test_tail_call_pipeline.c
  - tests/parser/test_aot_c_frame_setup_contracts.c
  - tests/parser/test_aot_c_metadata_binding_loader.c
  - tests/parser/test_call_binding_aot_projection.c
  - docs/plans/aot/syntax-contract-traceability.md
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/aot/index.md
  - docs/plans/aot/02-typed-value-and-layout.md
  - docs/plans/aot/04-semir-and-c-backend.md
doc_type: module-detail
---

# AOT C/LLVM 后端

**状态：`experimental`（已有可运行 baseline；是否可称 full-AOT 由具体 fixture/选项决定）。**

## AOT 的边界

AOT 不重新定义 ZR 语法、ownership、module、Task 或 property。输入必须是已经通过 parser/compiler 验证的 Canonical Type、TypeLayout、Place/CFG facts、Semantic/Exec IR 和 versioned artifact。backend 不得从 AST token、旧 `%xxx`、runtime object shape 或具体类型名推断语义。

## 后端组成

| 层 | 作用 |
|---|---|
| ExecIR/ExecBC projection | 把 Semantic IR 的值、Place、控制流和 effect 投影为 backend-neutral instruction。 |
| C emitter | 生成 C frame、typed scalar/aggregate helpers、native imports、metadata tables 和 entry thunk。 |
| LLVM emitter | 生成 LLVM IR/module text、typed calls、aggregate layout、exception/ownership control。 |
| AOT runtime | 对动态访问、异常、GC、module load、reflection、ownership 和 deopt 提供共享 C ABI。 |
| reachability/stripping | 从 entry、exports、reflection/native/test roots 计算可达 function/type/layout。 |

## Frame 与 ABI

`SZrAotMethodInfo` 记录 function index、metadata function、register frame bytes、GC root map、signature、generic dictionary、reflection invoker 和 observation policy。`SZrAotSignature` 记录 parameter count、return/parameter `SZrAotSignatureType`、varargs 和 passing mode。inline aggregate 不强行 box；frame layout 直接以 byte offset/alignment 传递。

```c
typedef TZrInt64 (*FZrAotEntryThunk)(SZrState *state);

typedef struct SZrAotGcRootMap {
    TZrUInt32 rootCount;
    const SZrAotGcRootSlot *roots;
} SZrAotGcRootMap;
```

进入 AOT function 时 push `SZrAotGcRootFrame`；返回、异常和 deopt 路径必须 pop。GC root location 可以是 frame byte offset 或 local address，layout id/field offset 由 metadata 验证。

## C 与 LLVM lowering

两后端都覆盖 typed scalar arithmetic、logical/branch、value construction/copy/drop、member/index、property meta get/set、direct/typed/dynamic calls、iterators、exceptions、ownership transitions、module exports 和 reflection invoker。C backend 使用生成的 helper/thunk；LLVM backend 生成相应 IR 并调用相同 runtime contract。

不能静态证明的动态 call/value access/iterator/reflection 由 AOT writer 标记 fallback warning；`requireFullAot` 打开时，任何未 lower 的 capability 应直接失败，而不是静默回退解释器。

## Generic dictionary 与代码裁剪

`SZrAotGenericDictionary` 的 slot 可解析 TypeLayout、prototype、method、box type 或 `sizeof`。generic sharing 以 canonical instantiation identity 区分 layout-dependent specialization 和 representation-independent sharing。stripping 必须保留：

- entry/export function；
- reflected constructor/method/field；
- native import/callback；
- generic dictionary/type layout；
- property accessor/resource Drop/test manifest roots。

缺失 root 在 link/metadata load 时报告，而不是运行时随机找不到函数。

## Module registration

生成 module 以 `SZrAotCodeRegistration` 暴露 function pointers、method infos/tokens、member remaps、manifest exports、invokers、type layouts/tokens、GC descriptors、native import contracts、call-binding rows 和 target function indices。`ZrAotCompiledModule.abiVersion` 必须匹配 `ZR_VM_AOT_ABI_VERSION`（当前头文件值为 `16`）。

持久 artifact 只保存 table index/token/hash；动态 thunk/function pointer 在 load 时由 registration 绑定。module reload 会使 function graph generation 前进并清理旧 witness。

## AOT runtime helper 类别

`zr_vm_library/aot_runtime.h` 当前提供以下函数族：

| 函数族 | 代表入口 | 责任 |
|---|---|---|
| frame/locals | `FrameInit`、`Sync*Local` | 建立 generated frame、读写 typed locals。 |
| calls | `PrepareDirectCall`、`PrepareMetaCall`、`FinishDirectCall` | 建立 call-info、参数复制、返回/异常收尾。 |
| values | `CreateObject`、`CreateArray`、`CreateInlineArray`、`TypeOf`、`ToObject/ToStruct` | 值/对象构造和 runtime checks。 |
| member/meta | `MetaGet`、`MetaSet`、`MetaGetCached`、`MetaSetStaticCached` | descriptor/accessor dispatch 与 cache guard。 |
| control | `Try`、`Throw`、`Catch`、`EndFinally`、`SetPendingReturn/Break/Continue` | 异常和 cleanup CFG。 |
| ownership | `OwnUnique`、`OwnBorrow`、`OwnShare`、`OwnDegrade`、`OwnWake`、`OwnDrop`、`OwnIntoGcBox` | owner/ref 生命周期和 bridge。 |
| return | `Return`、`ReturnI64`、`ReturnBool`、`ReturnU64`、`ReturnF64`、`ReturnInlineStruct` | typed/aggregate return。 |
| failure | `ReportUnsupportedInstruction`、`FailGeneratedFunction` | fail-closed diagnostics。 |

函数返回 `TZrBool`/`TZrInt64` 只表示 helper 是否完成；真正的异常/线程 status 仍在 `SZrState` 中，调用者必须遵守 header 注释的 resume/return contract。

## parity 与验证

AOT 通过门禁的顺序是 parser/semantic → SemIR/ExecBC → artifact → C → LLVM → project/CLI。对同一 fixture 要比较返回值、异常类型/位置、module exports、GC/drop 计数、call-binding identity 和 artifact hashes。`tests/parser/test_execbc_aot_pipeline.c` 等测试验证 opcode/contract，`tests/fixtures/projects/*` 验证端到端。
