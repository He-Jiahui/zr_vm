---
related_code:
  - zr_vm_core/include/zr_vm_core/global.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/string.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/include/zr_vm_core/execution.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/session_checkpoint.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/core-runtime/index.md
  - docs/core-runtime/state-lifecycle.md
tests:
  - tests/core/test_session_checkpoint.c
  - tests/core/test_precall_frame_slot_reset.c
  - tests/core/test_call_binding_runtime.c
  - tests/core/test_gc_concurrent_major.c
  - tests/exceptions/test_exceptions.c
doc_type: api-reference
---

# Core C API

## Global/state 生命周期

```c
SZrGlobalState *global = ZrCore_GlobalState_New(allocator, userArgs, 1u, &callbacks);
if (global == ZR_NULL) { return; }
/* GlobalState_New creates and launches global->mainThreadState. */
SZrState *state = global->mainThreadState;
EZrThreadStatus status = ZrCore_State_DoRun(state, entryName);
ZrCore_State_Exit(state);
ZrCore_GlobalState_Free(global);
```

`ZrCore_GlobalState_New` 建立 allocator、GC、主线程 state，并在返回前通过
`ZrCore_State_MainThreadLaunch` 初始化 string table、基础原型和 global registry；因此普通宿主
应直接使用 `global->mainThreadState`，不要再次手动调用 `State_MainThreadLaunch`。需要
AttachedDomain worker 时才调用 `ZrCore_State_New`，然后用
`State_MutatorLaunch/State_MutatorExit` 配对；不要在 secondary state 重复初始化 global registry。
当前 `ZrCore_State_Exit` 是兼容性钩子，不释放 state storage；secondary state 应以
`ZrCore_State_Free(global, state)` 释放，主 state 随 `ZrCore_GlobalState_Free` 释放。
`State_ResetThread` 可在捕获异常后清空执行上下文并保留 global。

loader/编译器由 `ZrCore_GlobalState_SetNativeModuleLoader`、`SetAotModuleLoader`、
`SetCompileSource`、`SetProviderModuleNameResolver` 注入。失败诊断通过
`ZrCore_GlobalState_GetModuleLoadDiagnostic` 读取。

## 值、字符串和对象

`SZrTypeValue` 包含 runtime `type`、`TZrPureValue`、GC/native 标记和 ownership control。
使用：

```c
ZrCore_Value_InitAsInt(state, &v, 42);
ZrCore_Value_InitAsBool(state, &flag, ZR_TRUE);
ZrCore_Value_InitAsFloat(state, &f, 3.5);
ZrCore_Value_Copy(state, &dst, &v);
ZrCore_Ownership_ReleaseValue(state, &dst);
```

字符串由 `ZrCore_String_Create` 创建，`ZrCore_String_GetNativeString` 只借用 UTF-8 指针；
短字符串内嵌，长字符串另有 GC storage。对象/原型 API 包括
`ZrCore_ObjectPrototype_New`、`ZrCore_StructPrototype_New`、`SetSuper`、`AddMeta`、
`AddManagedField`、`AddProtocol`、`ImplementsProtocol`、`ZrCore_Object_New`、
`ZrCore_Object_GetMember/SetMember`、`InvokeMember` 和 `GetByIndex/SetByIndex`。原型 mutation
会递增版本并使 member cache/call binding 失效。

## 执行和调用

`ZrCore_Execute(state, callInfo)` 驱动 bytecode；`ZrCore_Execution_Add` 把函数加入执行窗口，
`ZrCore_Execution_ToObject/ToStruct` 完成返回值物化。已知调用可通过
`ZrCore_CallBinding_PrepareKnownCall`，动态调用通过 `ZrCore_Object_InvokeMember`；成功后
cache contract 携带 target/signature/module/layout generation，失败必须重新 resolve，不能
盲跳旧指针。

## GC 和异常

GC API：`ZrCore_GarbageCollector_GcFull`、`GcStep`、`CheckGc`、`ZrCore_Gc_SafePoint`、
`ZrCore_Gc_WriteBarrier`、`ZrCore_Gc_NativeCallPinObject/Value`、`ZrCore_Gc_NativeCallUnpin`。
native callback 期间使用 pin/root，不要直接冻结整个 collector。`IgnoreObject` 只用于明确
的宿主生命周期对象，取消 ignore 前必须仍然可访问。

异常 API：`ZrCore_Exception_TryRun`、`Throw`、`TryStop`、`MarkError`、`ClearCurrent`、
`NormalizeThrownValue/Status`、`RaiseNamedRuntimeError`、`CatchMatchesTypeName`。捕获的
`EZrThreadStatus` 和 `currentException` 必须一起处理；仅清 status 会留下 stale exception。

## 模块、快照和诊断

`ZrCore_Module_Create`、`SetInfo`、`AddPubExport/AddProExport`、`GetPubExport/GetProExport`、
`AddToCache/GetFromCache/RemoveFromCache` 管理模块。`ZrCore_SessionCheckpoint_Create`、
`Rollback`、`Free` 只保存 parser/REPL 允许的 session 绑定，不是任意 heap snapshot；回滚后
旧 function/closure handle 必须丢弃并重新查 generation。
