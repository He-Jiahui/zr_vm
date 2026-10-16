---
related_code:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_internal.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_transport.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - tests/thread/test_thread_transport_string.c
  - tests/cmake/thread-transport-string-tests.cmake
implementation_files:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_internal.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_transport.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
plan_sources:
  - "user: 2026-10-16 支撑层现役 BUG 修复与真实回归"
tests:
  - tests/thread/test_thread_transport_string.c
  - tests/cmake/thread-transport-string-tests.cmake
doc_type: module-detail
---

# Thread native transport 字符串字节保真

## 契约和根因

Channel、Shared/Mutex payload、Transfer 和旧 direct-worker 使用私有 `ZrVmTaskTransportValue`。VM 字符串本身有显式字节长度，允许中间 NUL；先前 encode 和 clone 使用 strlen，decode 使用 C 字符串 setter，因此第一处 NUL 后的字节会丢失。本修复只处理此 native envelope，不修改隔离调度器的 Core OwnershipTransfer 协议。

## 长度与所有权

STRING envelope 的 `stringByteLength` 保存有效字节数，`as.stringValue` 独占 libc malloc 缓冲区；缓冲区另有一个终止 NUL，但该终止符不属于有效长度。encode 从 Core GetByteLength 取得长度，复制准确字节数；clone 复制相同长度到独立缓冲区。分配 helper 拒绝 length+1 溢出及非零长度的空源；零长度允许空源，仍分配独立终止符。

decode 使用 Core String_Create 的显式长度入口，再交给 Library StringObject setter；创建失败沿用既有 MemoryError 抛出语义，不把失败变成 true。NULL 指针、零长度仍表示空字符串，NULL 指针、非零长度返回 false。clear 继续 free 字符串并清零整个 envelope，长度也随之清零。嵌套 clone/clear 和队列的整个结构移动天然携带该长度。

全模块检索还找到旧 worker fault 的手工 STRING producer；它的输入本来是 C 诊断字符串，故在成功 strdup 后记录 strlen，避免迁移后错误诊断变为空串。没有修改 runtime.c 或其他 native 引用计数业务。私有 header 的已有外部文档修改保留，交付 diff 只选择新增长度字段的 hunk。此私有结构布局改变需要所有 Thread 翻译单元重新编译。

## 实际回归和限制

`zr_vm_thread_transport_string_test` 链接实际 Core/Library/Thread 生产库。三个独立进程用例执行 encode → clone → clear 原 envelope → decode，比较 VM byteLength 与全部有效字节，检查 clone 缓冲区独立；覆盖含两个中间 NUL 的五字节串、空串、普通串。测试清理两个 envelope 和输入 GC lease，销毁 global 后验证受 VM allocator 跟踪的存活块为零；libc envelope malloc 不在该计数内，不据此声称已动态验证 native malloc 零泄漏。

Root 的 `meta-thread-file-red-r2` 实际 configure/build 均为 0，1918 inputs 未变化；embedded_nul FAIL，empty/plain PASS。修复后 root 的 support-batch-5-green 实际重编译了 Thread，三项全部通过；1929 个输入哈希无变化。worker fault、嵌套 Transfer 及 Channel 端到端行为未新增动态专测。façade OOM retain、Shared 并发释放等独立候选仍未修复，不属于本组验收。
