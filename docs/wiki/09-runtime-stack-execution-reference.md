---
related_code:
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/execution.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_budget.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
tests:
  - tests/core/test_precall_frame_slot_reset.c
  - tests/core/test_call_binding_runtime.c
  - tests/instructions/test_instructions.c
  - tests/parser/test_instruction_execution.c
  - tests/core/test_gc_concurrent_major.c
  - tests/exceptions/test_exceptions.c
plan_sources:
  - user: 2026-09-10 持续扩充 ZrVm Wiki 的语言实现与 C API 说明
  - docs/core-runtime/index.md
  - docs/core-runtime/state-lifecycle.md
  - docs/core-runtime/gc-domain-concurrent-major.md
doc_type: implementation-reference
---

# VM 执行栈、调用帧与解释器参考

**状态：`current`。** 本页从实现视角说明 `SZrState` 如何承载执行栈、call-info 链、解释器、
cleanup、GC safepoint 和 native/AOT 边界。它补充 [VM 运行时](09-vm-runtime.md) 的总览；
宿主创建/关闭 VM 的顺序见[嵌入式宿主生命周期](05-interop/embedding-lifecycle.md)。

## 1. 运行时对象边界

ZrVm 将长寿命配置/共享资源与实际执行上下文分开：

```text
SZrGlobalState
  allocator, module/native registry, loader, GC-wide state, caches
       |
       +-- SZrState (one mutator/execution context)
              stackBase / stackTop / stackTail
              callInfoList chain
              thread status, pending control, exception handlers
              debug hooks, execution budget, GC domain attachment
```

`SZrGlobalState` 管理 VM 范围的 owner；`SZrState` 只能在附着的 global/GC domain 内执行。
主 state 由 `ZrCore_State_MainThreadLaunch` 启动。需要并发执行时，先在同一 global 下创建 state，
再使用 `ZrCore_State_MutatorLaunch` / `ZrCore_State_MutatorExit` 附着或退出 mutator；这不是创建
第二个 global，也不能把一个 state 搬到另一个 global。

## 2. 栈的物理表示与逻辑 slot

栈元素是：

```c
struct SZrTypeValueOnStack {
    SZrTypeValue value;
    TZrUInt32 toBeClosedValueOffset;
};
```

`TZrStackValuePointer` 是该槽类型的指针。`stackBase`、`stackTop`、`stackTail` 分别表示已分配
区域起点、当前使用上界和容量上界。`SZrTypeValue` 保存 tagged value/managed pointer；需要 inline
aggregate 的 frame slot 则通过 `SZrTypeLayout` 和 byte offset/alignment 表示，不能假定每个语言值
只占一个 C 指针。

### 2.1 栈扩容会使裸指针失效

`ZrCore_Stack_GrowTo`、`ZrCore_Stack_Grow` 与 `ZrCore_Stack_CheckFullAndGrow` 可以重分配栈。
因此，任何会分配、调用、触发 debug hook、进入 native 或触发 safepoint 的路径之前，不能把
`TZrStackValuePointer` 当作稳定地址保存。

可重定位指针使用 offset 或 function stack anchor：

```c
TZrMemoryOffset saved = ZrCore_Stack_SavePointerAsOffset(state, slot);

if (!ZrCore_Stack_Grow(state, extraSlots, ZR_TRUE)) {
    return ZR_FALSE;
}

slot = ZrCore_Stack_LoadOffsetToPointer(state, saved);
```

上例表达的规则比代码更重要：**每次可能移动栈的操作之后，重新获取 pointer。** 内部实现也在
closure、debug、numeric execution、exception cleanup 等路径中以 offset/anchor 恢复位置。
对扩展作者而言，优先使用公开的 root/anchor helper；不要复制 state 内部字段的保存策略。

### 2.2 Inline storage 和 frame place

inline struct、固定布局数组或别名 slot 以 frame byte layout 存储。`SZrStackFramePlace` 包含
`address`、`byteOffset`、`byteSize`、`byteAlign`；`ZrCore_Stack_MakeFramePlace` 将 frame base 和
layout offset 解析为一个有效 place。复制 inline 值时应使用：

| API | 适用情况 |
| --- | --- |
| `ZrCore_Stack_CopyInline` | 已有 source/destination 的稳定 stack offset。 |
| `ZrCore_Stack_CopyInlinePlace` | 已计算为 `SZrStackFramePlace` 的两个位置。 |
| `ZrCore_Stack_CopyValue` | 普通 `SZrTypeValue` 的 value-level copy。 |
| `ZrCore_Stack_SetRawObjectValue` | 写入受 GC 管理的 raw object 值，走 runtime 的正确表示。 |

不能用 `memcpy` 替代这些 helper：inline layout 可能需要类型特定 copy/drop/GC scan 语义，且
字节对齐、alias 和 managed edge 都由 layout contract 决定。

## 3. 调用帧和 `SZrCallInfo`

`SZrCallInfo` 是执行中的调用链节点。关键字段分为四组：

| 组 | 字段/概念 | 作用 |
| --- | --- | --- |
| frame | `functionBase`、`functionTop` | 当前 closure 与 frame 的逻辑范围。 |
| 链路 | `previous`、`next` | 连接调用者和被调用者。 |
| 执行上下文 | program counter、trap、variable argument count | 解释器继续执行与 debug 观察。 |
| 返回/参数 | return destination、argument source frame/base/start slot | 跨帧传参与返回值物化。 |
| 运行时状态 | call status、native continuation、yield/tail 状态、debug generation | native、generator、tail call 和 debugger 的行为边界。 |

`EZrCallStatus` 以位标志区分 VM call、native call、create-frame、debug hook、yield、tail call、
deconstructor/close call 等状态。`ZR_CALL_INFO_IS_VM(callInfo)` 与
`ZrCore_CallInfo_IsNative(callInfo)` 是检查所处 lane 的正确方式；不要仅凭 metadata function 是否为
空推断调用类型。

### 3.1 返回目标不是固定栈槽

新版 call-info 可以带显式 `returnDestination` 和可重定位 offset，`hasReturnDestination` 用来
区分它与兼容旧语义的回退位置。AOT、native callback 和 normal bytecode call 都必须让返回值沿
调用约定指定的位置物化，不能假定“返回值总在 functionBase”。

### 3.2 Native continuation

native call-info 使用 `SZrCallInfoNativeContext` 保存 continuation function、上一个 error
function 和 continuation arguments。callback 返回并不意味着 VM 可随意跳过 cleanup；控制权回到
调用边界后仍要观察 state 的 thread status、异常和 pending control。

## 4. 解释器执行模型

`ZrCore_Execute(state, callInfo)` 是 core 的字节码执行入口。正常的宿主启动路径应通过
project/function 入口或 `ZrCore_State_DoRun` 建好 call frame；外部代码没有一个已初始化的
`SZrCallInfo` 时，不应直接调用 `ZrCore_Execute`。

一次指令执行的抽象步骤：

1. 读取当前 program counter 和 instruction/opcode。
2. 解析当前 frame、slot、常量、layout/call binding。
3. 在 numeric、value、member/index、call、control、ownership、iterator 或 meta lane 中执行。
4. 若发生分配、native boundary、取消或 GC 请求，进入相应的 safepoint/check。
5. 若发生 throw/return/break/continue/yield，转交统一 pending-control/cleanup 路径。
6. 更新 program counter 或切换到 next call-info。

`ZrCore_Execution_Add`、`ZrCore_Execution_ToObject`、`ZrCore_Execution_ToStruct` 是对特定语义
操作的 core helper；它们仍要求来自当前 state/call-info 的正确 value 和 destination。禁止让
native library 伪造任意 `SZrCallInfo` 或偷用另一个 state 的 slot。

### Checked path 与 quickening

解释器可以为已验证的 call/member/iterator 路径缓存或 quicken，但这一优化不会改变语言语义：

- cache guard miss 必须回到可验证的通用路径；
- type/range/layout/ownership check 不能因热点而被删除；
- dynamic/meta/property 调用必须保留 descriptor/prototype contract；
- profile/debug 的观测点不能被优化成“不可见”。

这也是 SemIR、call binding 与 AOT runtime 使用统一 identity/fact 的原因。

## 5. 控制转移、异常和 cleanup

运行时不会为 `return`、`throw`、`break`、`continue`、resource Drop 和 `finally` 分别维护互不
理解的展开逻辑。`SZrState` 的 pending control 与 exception-handler state 统一表示尚未完成的
控制转移；execution control 先关闭需要关闭的值/资源，再恢复目标 PC 或向外层 handler 继续展开。

```text
instruction raises control
  -> record pending return/exception/break/continue
  -> close values above cleanup boundary
  -> execute finally / cleanup handler when present
  -> resume target, catch target, or caller frame
```

因此：

- `using`/owner 的关闭必须走 cleanup scope，而不是 native code 自行在每个 return 分支释放；
- catch 不能吞掉尚未完成的 finally；
- tail call、yield 和 native continuation 都必须保持 frame/cleanup 的一致性；
- callback 遇到 type error 时应使用 library error helper，让 VM 进入其标准异常路径。

异常 API、try frame 和取消/终止边界详见[Core GC、异常与执行安全 API](05-interop/core-gc-exception-api.md)。

## 6. GC、safepoint 与 native 边界

GC 支持 minor、major、full collection，并暴露 idle、minor mark/evacuate、concurrent major mark、
remark、sweep、compact 等 phase。根来源包括 state stack、call-info、closures、全局表、native
pin/root 和 AOT root frame。

### 6.1 哪些操作可能改变可观察内存

以下行为都应按“可能进入 safepoint 或改变 managed object 可达性”处理：

- 分配 object/string/array，或把值写入 managed object；
- 进入/返回 native callback，或嵌套执行一个 callable；
- 显式 `ZrCore_GarbageCollector_GcStep` / `GcFull`；
- 模块加载、AOT helper、debug hook、执行预算/取消检查；
- 可触发 stack grow 的任何 frame/scratch slot 分配。

在这些边界跨越期间保存 managed object 时，使用正确的 value root/pin/GC API。不要保存未 rooted
对象裸指针，也不要把 stack slot 地址带出 callback。面向 native 作者的实际规则见
[Native Call Context 与回调 C API](05-interop/native-call-context-reference.md)。

### 6.2 写屏障不是可选优化

对象、array、pair 或 provider 写入一个 managed edge 时必须使用相应 core/library setter，确保
write barrier 和 remembered-set 更新发生。直接写 object internal field 可能在 young/old 代或
并发 major collection 下产生漏标。GC 统计和 domain 迁移不是替代写屏障的机制。

## 7. 执行预算、取消与线程状态

execution budget（当前为 `experimental`）可约束 instruction、heap、GC pause、native call 或
cancellation token。检查点出现在指令/调用/GC 边界；中止不是事务回滚，已经完成的 I/O、native
side effect 和 heap mutation 不会自动撤销。

callback、AOT helper 和宿主运行循环应把 `TZrBool` 失败与 `state->threadStatus` 一起看待。一个
helper 返回 false 可能表示已建立的语言异常、取消、资源限制或内部失败；不能继续使用同一批
临时 pointer 假装调用仍处于成功状态。

## 8. C API 生命周期和安全模式

### 最小 state 生命周期

```c
/* 具体构造参数以 global.h/state.h 为准。 */
ZrCore_State_Init(state, global);
ZrCore_State_MainThreadLaunch(state, arguments);

status = ZrCore_State_DoRun(state, "main");

ZrCore_State_Exit(state);
ZrCore_State_Free(global, state);
```

实际嵌入程序还要先构造/配置 global、注册 provider/loader、在退出前释放 project 和 native
resources。不要把此片段理解为全部 owner 的创建代码。

### 栈工具 API 的正确定位

| API | 合法用途 | 不应做什么 |
| --- | --- | --- |
| `ZrCore_Stack_Construct/Deconstruct` | core/testing 中初始化/销毁指定 stack storage。 | 绕过 state 生命周期为生产宿主自行拼 runtime。 |
| `ZrCore_Stack_Grow*` | core 扩容或内部扩展。 | 增长后继续使用旧 stack pointer。 |
| `SavePointerAsOffset` / `LoadOffsetToPointer` | 将 slot pointer 跨可能 reallocate 的小段工作转换为相对位置。 | 跨 state、跨 frame 或跨 state teardown 保存 offset。 |
| `MakeFramePlace` | 依据受验证的 layout 获取 inline storage。 | 手算 byte offset 或跨 frame 复用 place。 |
| `CopyInline*` | 执行 layout-aware inline copy。 | 用 raw `memcpy` 忽略 ownership/GC scan。 |

## 9. 与 Native、AOT 和调试器的交接

| 边界 | VM 负责 | 调用方负责 |
| --- | --- | --- |
| native callback | 建立 `ZrLibCallContext`、选 dispatch lane、同步重定位后的 self/argument。 | 只在 callback 生命周期内使用借用 view，给 result 赋值或走语言错误。 |
| AOT function | 建立 generated frame、GC root map、观测点、return/exception bridge。 | generated code 只调用声明的 runtime helper，不复制解释器内部状态机。 |
| debugger/LSP | 用 debug generation、source map、snapshot 避免过时 frame。 | 不缓存失效 frame slot 或旧 revision 的 pointer。 |
| attached mutator | 管理同一 domain 的 safepoint 协作。 | 先 launch/exit，禁止跨 global 传递对象。 |

关于 AOT 路径的实际 helper 分工，见[AOT Lowering、运行时 Helper 与注册参考](10-aot-lowering-registration-reference.md)。

## 10. 排查清单

发生随机崩溃、GC 后对象错误、callback 返回后 slot 错位或异常跳错 handler 时，优先依次检查：

1. 是否把 `TZrStackValuePointer`、inline address 或 `ZrLibInlineArgumentView` 跨过了分配/safepoint；
2. 是否在写入 managed object 时绕开 setter/write barrier；
3. 是否在 return/throw/break 路径跳过了 resource cleanup；
4. 是否用另一个 `SZrState` 或已退出 mutator 访问对象；
5. 是否把 `false` 返回当成普通空结果、继续执行；
6. 是否在 AOT 代码中跳过 root frame push/pop 或 dynamic deopt bridge。

这类问题应从 stack offset、call-info chain、thread status 和 root visibility 开始取证，而不是先
在业务 callback 中添加重试或额外 `GcFull`。相关回归测试位于本页 Front Matter 列出的 core、
instruction、exception 和 GC 测试目录。
