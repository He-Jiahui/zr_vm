---
related_code:
  - zr_vm_core/include/zr_vm_core/global.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/include/zr_vm_core/execution.h
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
tests:
  - tests/core/test_precall_frame_slot_reset.c
  - tests/instructions/test_instructions.c
  - tests/parser/test_instruction_execution.c
  - tests/core/test_object_shape_transition_cache.c
  - tests/core/test_gc_concurrent_major.c
  - tests/exceptions/test_exceptions.c
  - tests/core/test_string_layout.c
  - tests/core/test_call_binding_runtime.c
  - tests/core/test_session_checkpoint.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/core-runtime/index.md
  - docs/core-runtime/state-lifecycle.md
  - docs/core-runtime/gc-domain-concurrent-major.md
doc_type: module-detail
---

# VM 运行时

**状态：`current`**

## Global 与 State

`SZrGlobalState` 是进程/VM 实例级 owner：allocator、全局 registry、module cache、native/AOT loader、GC、字符串 hash/cache、配置和 module-load diagnostic 都在这里。`SZrState` 是执行线程/mutator：stack、call-info chain、exception state、debug hooks、runtime checks、GC domain 和 execution budget 在这里。

一个 global 可以附着多个 state，但 state 不能跨 global 或 GC domain 任意移动。主线程通过 `ZrCore_State_MainThreadLaunch` 启动；附加 mutator 使用 `ZrCore_State_MutatorLaunch/Exit`，不会创建第二套 global registry。

## Stack 与 frame

栈是 `SZrTypeValueOnStack` 槽数组（指针别名为 `TZrStackValuePointer`），`stackBase`/`stackTop`/`stackTail` 记录边界。扩容或 compaction 后，所有保存的 stack pointer 必须通过 `SZrFunctionStackAnchor`/relative offset 重定位。inline struct slots 由 TypeLayout 决定 byte size/alignment；普通 `SZrTypeValue` 槽保存 tagged value/managed pointer。

call-info 链把当前 function、参数源 frame、返回目标、PC、native continuation、yield/tail 状态连接起来。debug frame generation 让 LSP/debugger 在 frame 重建后拒绝过时变量句柄。

## 指令执行

`ZrCore_Execute(state, callInfo)` 是主解释器入口。dispatch 根据 instruction opcode 选择 numeric、value、member/index、call, control, ownership, iterator 或 meta lane。每条 lane 先验证 stack/layout/contract，再执行；开启的 bounds/type/range checks 在运行时保留。

典型访问分流：

```text
receiver.member       -> GET_MEMBER / SET_MEMBER
receiver[index]       -> GET_BY_INDEX / SET_BY_INDEX
unknown callable      -> DYN_CALL / META_CALL
property              -> META_GET / META_SET
typed function value  -> CALL_TYPED
```

ExecBC quickening 只改执行缓存，不改变 SemIR/AOT 语义；cache guard miss 回到 checked path。

## Object 与 prototype

`SZrObject` 保存 raw object header、prototype、fields/storage 和 internal type；`SZrObjectPrototype` 保存 type identity、member descriptors、meta methods、layout/generation、interface dispatch entries 和 native extension data。字段、method、property、static member 是不同 descriptor kind。

对象访问先按 descriptor/prototype chain 查找，再按 member kind 选择 field load、method bind、property accessor 或 static target。动态 map/key access 只有对象明确实现 index contract 时有效；不能靠 `getIterator` 等字段名字伪造 protocol conformance。

GC write barrier 由 object setters、pair/array helpers 和 native adapters 在 managed edge 写入后调用。inline arrays 可直接存 raw canonical elements，也可在 node canonical mode 物化；两种 storage 由 super-array contract 约束。

## 字符串、数组和 hash

字符串是 UTF-8 managed object，hash-set/string concat pair cache 在 global 中复用；字符串拼接的 semantic type rule 在 parser 层优先于数值 common type。数组支持通用 object array 和 inline typed array；`ZrCore_Object_NewInlineArray`、element offset/GC visit/drop helper 负责布局与元素生命周期。hash/set/map 由 core hash/hash_set 与 library container provider 组合。

## GC 与 safepoint

collector 支持 `GcStep`、full collection、incremental/concurrent major、remembered objects、write barrier、native call pin、AOT root map 和 GC statistics。mutator 在分配、call boundary、native enter/leave 和显式 `ZrCore_Gc_SafePoint` 协作；native callback 若长时间运行必须按 provider contract poll/cancel。

## 异常和 pending control

`SZrState.pendingControl` 可保存 exception、return、break、continue 以及目标 instruction/value slot。异常 handler 先运行 cleanup，再恢复 pending control；因此 `finally`、Drop、using 和 tail-call 不能各自维护另一套展开逻辑。

## Execution budget（`experimental`）

`SZrExecutionBudget` 可按 instruction、heap、GC pause、native call 和 cancellation token 限制一次调用。检查点在 bytecode fetch、native boundary 和 GC safepoint；终止不会回滚已经发生的 global/heap mutation。Rust binding 文档给出完整语义和 usage 结构。

## Session checkpoint（`experimental`）

project session checkpoint 保存可恢复的 session-level 状态，但不是任意 heap snapshot。调用前须完成同一 session 的所有 active calls；rollback 失败不会伪装成成功。当前公共入口位于 `session_checkpoint.h`，由 Rust binding 和测试 fixture 使用。

## 核心 C 生命周期

```c
SZrGlobalState *global = ZrCore_GlobalState_New(allocator, userData, ...);
/* attach loaders/registry and create a state */
SZrState *state = ZrCore_State_New(global);
ZrCore_State_MainThreadLaunch(state, arguments);
EZrThreadStatus status = ZrCore_State_DoRun(state, "main");
ZrCore_State_Exit(state);
ZrCore_State_Free(global, state);
ZrCore_GlobalState_Free(global);
```

实际 `ZrCore_GlobalState_New` 参数以 `global.h` 为准；library 的 `ZrLibrary_CommonState_CommonGlobalState_New` 可为常见 `.zrp` 场景提供默认 allocator/loader。所有 pointer 的销毁顺序必须遵守 owner 关系，详见 [嵌入式宿主生命周期](05-interop/embedding-lifecycle.md)。
