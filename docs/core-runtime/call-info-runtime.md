---
related_code:
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/src/zr_vm_core/call_info.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_precall_internal.h
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/src/zr_vm_core/closure_close_meta_guard.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/debug_evaluation_context.c
  - zr_vm_core/src/zr_vm_core/task_frame_runtime.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/src/zr_vm_core/call_info.c
plan_sources:
  - user: 2026-10-05 全仓调用意图与公开契约注释审查，CallInfo 有限集成
  - docs/code-review/comment-standard.md
tests:
  - tests/core/test_precall_frame_slot_reset.c
  - tests/core/test_tail_reuse_callinfo_reset.c
  - tests/gc/gc_native_base_frame_tests.inc
  - tests/core/test_ssa_dispatch_vm_call_minor_gc.inc
  - tests/core/test_ssa_dispatch_native_callback.inc
  - tests/parser/ssa_execbc_vm_multiply_cases.inc
doc_type: module-detail
---

# 调用帧记录与复用链

## 用途与边界

`SZrCallInfo` 描述一次调用的栈窗口、活动前驱、返回策略和执行上下文。它让执行、异常、GC 与调试读取同一激活记录；它不是独立拥有栈值的任务快照。native/VM 视图由 `callStatus` 的 `NATIVE_CALL` 位判别，不能因为元数据指针非空就把 native 帧当作 VM 帧。

## 所有权与复用

state 内嵌 `baseCallInfo`。入口初始化借用有效且可写的基础函数槽地址，将该槽中的值置空，建立 native 异常边界；空的是值，而非传入地址。`EntryNativeInit` 清零整个记录，不分配、不自动发布活动帧，也不替调用方释放旧资源。复用含后继的边界前，调用方需要保存并恢复缓存链接，处理旧资源；close 边界现有代码执行这一过程。

`Extend` 只在有效当前帧的缓存链尾分配，要求 `next == NULL`。成功后连接 `next/previous` 并增加长度，state 最终释放这些扩展记录。分配层可能执行 GC 或抛内存异常；返回 NULL 的分支不发布链或长度。`previous` 是 GC/异常/调试回溯的活动链，`next` 是复用缓存。native 快路径还使用 C 栈上的临时记录，需恢复原缓存链接，不能把所有 CallInfo 都当作 Extend 拥有的堆块。

## 栈地址与返回策略

`functionBase/functionTop` 借用可移动 VM 栈窗口。栈重定位将活动链地址转成偏移，再从新基址恢复；跨分配或扩容边界应按所属栈重新取地址。显式返回目标由 `hasReturnDestination` 选择；为假时使用 `functionBase`，不能以指针是否 NULL 替代标志。单结果快路径还要求相应返回数量与状态资格。

`argumentSourceFrameBase/argumentSourceStartSlot` 描述借用的实参源窗口，`hasArgumentSourceFrame` 决定是否有效及是否参与重定位。它们不冻结调用者存储。异步 TaskFrame 使用自己的根与值复制协议；持有 CallInfo 或裸栈指针本身不会建立该所有权。

## GC、调试与字段约束

类型泛型上下文和方法泛型上下文是两个独立的受管值槽。复用会清除旧上下文，GC 沿活动 `previous` 链标记并更新；帧退出后不再提供长期根，跨帧保留对象需要另建根。元数据缓存服务布局查询，不能替代帧视图判别。

三字节 `frameStorageSlotCountPlusOne` 缓存按低字节在前编码槽数加一，零要求回退元数据查询；它不是零槽数。调试激活代数还须结合活动链成员身份校验，复用同一地址不表示同一次激活。VM `trap` 的 volatile 限定不提供跨线程同步。

native 续体字段与 VM PC/trap 存储重叠。初始化将 `continuationFunction` 置空，native 复用清空联合体；close 丢弃判定会拒绝非空续体，因此不能声称字段完全无人读取。当前查到的代码没有生产续体回调调用；函数指针类型/测试赋值不构成回调执行证据。

## 具体待核契约

保留状态位须对照外部 ABI 和生成消费者确认；旧 yield/return 联合体预留字段须核对协议用途；native 续体须核对历史或外部 ABI 后决定保留或接线；入口同一槽两次 ResetAsNull 会重复记录 profile helper，是否保留取决于历史计数契约。本轮没有完整合法链证明将这些疑点提升为新 BUG。

## 静态验证范围

完整 73 项包括所有枚举成员、类型、字段、别名、宏、函数/声明及有意义的块。4 个 included `.inc` 的 7 个实际表达式使用已沿父文件和 CMake 入口定位；宏嵌套测试表达式与生成 C 模板和生产调用分别记录。这里只描述静态源代码及构建注册，不声称执行测试、native 回调、GC、AOT ABI 或运行时验证。

源候选继承完整源码审查与有限 peer 闭合；本次集成只更新注释、文档与台账数字引用，代码、布局、签名和值不变。
