---
related_code:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_transport.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_wrappers.c
  - tests/thread/test_thread_transport_facade_oom.c
  - tests/cmake/thread-transport-facade-oom-tests.cmake
implementation_files:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_transport.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_wrappers.c
plan_sources:
  - "user: 2026-10-16 支撑层现役 BUG 修复与真实回归"
tests:
  - tests/thread/test_thread_transport_facade_oom.c
  - tests/cmake/thread-transport-facade-oom-tests.cmake
doc_type: failure-contract
---

# Thread transport decode façade 失败引用平衡

## 当前诊断

本组已取得四种 façade 的实际 RED，随后修复 decode 事务边界；GREEN 待 root 统一验证。Shared、WeakShared、UniqueMutex 和 SharedMutex 的实际 decode 分支先增加 native cell 引用，然后调用 make_value 创建 VM façade。原先普通 false 或抛异常都没有对应回滚；现有 Core/Library Throw 只能让失败可见，不会替 native 计数释放引用。

本组直接调用实际 `zr_vm_task_transport_decode_value`，不启动线程、不构造调度器。内部 header 的 cell 布局用于提供合法 caller-owned 输入：Shared cell 有一份 caller strong、一份 caller weak、ALIVE 状态和整数 payload；Mutex cell 有一份 caller 引用、未加锁状态、实际同步原语及整数 payload。calloc/init 仅为 fixture 输入准备，不复制 decode、retain 或 release 实现，也不据此取得业务 constructor 或 GC 自动析构的测试信用。

## Decode 事务实现

四个 decode 分支共用 runtime_transport.c 的私有 helper。它先增加对应 strong、weak 或 Mutex 引用，再用局部 TryRun 调用实际 make_value。上下文由 TryRun 调用者持有，callback 的 created 标志不处于被 longjmp 放弃的函数局部状态内。TryRun 捕获状态为 FINE 且 created=true 时，该引用交给 façade；不因入口已有残留 threadStatus 而改变成功语义。失败时归还本次预留的一份，普通 false 保持 false；只有 false 后实际 threadStatus 相比入口发生变化才采用该次新状态。TryRun 捕获的错误优先，清理后按原状态重新 Throw。保留已有 currentException，Core Throw 已有 Error 时不重新规范化，不清除或替换原 Error。

该 helper 没有公开接口、生产故障钩子或新内存分配。make_value 保持原来的最后 finish 发布结果方式；本组没有增加输出槽回滚或字段多步原子性的公共保证。普通 false 的分支也执行同一引用回滚，但当前动态注入只取得 Throw 路径信用。wrappers 的 clone/downgrade/upgrade 及并发释放保持独立候选，不据此宣称已修。

## 精确故障

测试先 Register/Import 真实 zr.thread 描述符及类型，预热 MemoryError，创建有独立 GC lease 的结果 sentinel，再启用真实 VM allocator 故障。仅拒绝 old=NULL、oldSize=0、OBJECT 类别、`sizeof(SZrObject)` 字节的实际对象申请；持续拒绝包括 GcMalloc 的 GC 后重试，避免单次故障被重试吞掉。

第一次拒绝时读取实际 cell 计数，要求只有当前 decode 对应的一项已经比原始 baseline 多一，证明故障命中 retain 之后。外层真实 TryRun 要捕获 MEMORY_ERROR，回调不能正常返回，输出仍为 sentinel，恢复点与显式 ignored roots 数量平衡。随后直接对比原始 baseline，不能把失败后的当前计数当成新基线。

精确 OBJECT 申请失败测试覆盖 Throw 路径；没有通过非法 state、空 gcDomain 或 out=NULL 人为制造 false。这些不等于有效 decode retain 后的正常返回失败。make_value false 分支的回滚仍是静态诊断，需后续独立合法失败边界才能声明动态覆盖。

## 重试和成功 façade 清理

撤障并清除异常状态后，用同一 cell 和同一借用输入 envelope 重试。成功返回应恰好多一份对应引用；实际 try_get_cell 必须取回原 cell，并给出准确 weak 标记或 Mutex kind。测试只在真实成功 façade 上清空 native-cell 字段，再调用实际 strong/weak/mutex release 一次，归还这次成功交付的引用；之后计数必须回到最初 baseline，整数 payload 和存活/锁状态保持。

WeakShared 和 Mutex 缺少公开 owner release/drop 方法，本组明确使用内部支持层释放 API，不把 VM 对象 GC 当成 native cell 析构。最后只释放原 caller 的 strong/weak 或 Mutex 引用，不按计数循环释放，也不补还失败时泄漏的引用。借用输入 envelope 不调用 transport_clear，避免重复归还 caller 引用。

RED 时遗留 native 引用可能让 native cell 活到进程退出，这是未修行为的证据；夹具不直接 free 活 cell 来掩盖错误。native libc cell 不纳入 VM allocator 总块数，不声明全进程 malloc 零泄漏。GlobalState 销毁后的零存活断言仅覆盖真实 VM allocator 的分配类别、尺寸与释放配对。

## 注册与证据边界

目标 `zr_vm_thread_transport_facade_oom_test`，CTest 前缀 `thread_transport_facade_oom_`。四个 OOM 进程为 shared、weak_shared、unique_mutex、shared_mutex；normal_controls 在一个进程依次执行四种无故障 control。每项 30 秒超时，Windows 禁止崩溃对话框。独立片段检查全部八个真实静态链接目标，不修改顶层注册。

Root 实际 exception-thread-red-r2 configure/build 均为 0，1935 inputs 的 changed_inputs=[]。四个 OOM 用例均观测失败后的对应计数多留一份，撤障重试后仍高于原始 baseline；normal_controls 通过。生产随后添加上述事务 helper。agent 未运行 native；修复后 GREEN 由 root 统一实际构建执行。测试不覆盖 Shared.clone/downgrade/upgrade 的独立调用失败、字段写入中途失败、业务构造器、GC 析构和并发释放竞争，也不为这些问题宣称修复或动态证据。

## Root acceptance

Actual ClangCL configure/build0, 168/168 CTest, pool14, exceptions15 and CLI hello0; 1944 input hashes unchanged; independent review ACCEPT. Windows only. Evidence: `reports/review-bugfix-20261016/integrated-support-batch-6-green-{0..5}.log` and receipt under the configured external artifact root.
