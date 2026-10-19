---
related_code:
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_full.c
  - zr_vm_core/src/zr_vm_core/gc/gc_internal.h
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c
  - zr_vm_core/src/zr_vm_core/execution/execution_budget.c
  - zr_vm_core/include/zr_vm_core/raw_object.h
  - tests/core/test_gc_scan_exception_cleanup.c
  - tests/cmake/gc-scan-exception-cleanup-tests.cmake
implementation_files:
  - zr_vm_core/src/zr_vm_core/gc/gc_full.c
  - zr_vm_core/src/zr_vm_core/gc/gc_internal.h
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - tests/core/test_gc_scan_exception_cleanup.c
  - tests/cmake/gc-scan-exception-cleanup-tests.cmake
plan_sources:
  - user: 2026-10-16 active BUG repair with actual RED before production
  - initial-bug-ledger: FRawObjectScanMarkGc protected full-GC exception path
tests:
  - tests/core/test_gc_scan_exception_cleanup.c
  - tests/cmake/gc-scan-exception-cleanup-tests.cmake
doc_type: regression-contract
---

# FullGC 扫描异常的清理边界

根代理统一执行的 `integrated-gc-fs-support-red` 已取得真实 RED：configure/build
均为 0，1952 个输入 `changed_inputs=[]`；本组两种模式的 Throw 用例均失败，真实
观测为锁未释放、pauseDepth=1、预算 gcDepth=1、immediate=1。两个正常对照通过。
已据此实现修复，尚未取得修复后的 GREEN；根代理负责后续统一实际构建与验证。

FullGC 取得暂停和原始 mutation lock 后，会经过对象扫描回调。旧路径没有本地
异常边界，回调的非局部退出跳过 mutation unlock、StopTheWorldEnd 和 GcEnd。
既有 TryRun 只恢复登记的 execution/native/mutation scope 深度；原始锁层、暂停
层和 GC 预算深度并不在该快照内。因此仅验证 TryRun 返回错误不能证明 GC 已收尾。

## 生产边界与重试

完整回收职责已从超过千行的 `gc.c` 提取到 `gc_full.c`。原成功算法仍由同一增量/
分代驱动器执行，继续使用同一遥测函数和共享私有暂停超时值；没有修改 Core 的
GC budget/major/compact 标量契约。模块源收集规则会纳入新增 `.c`，不新增顶层接线。

FullGC 在获得暂停和 mutation lock 后记录 generation/immediate 入口值，再建立
局部 TryRun。可变 context 属于 TryRun 调用者，不处于 setjmp 所在函数的自动
局部变量作用域，因此非局部退出后仍可读取完成标志。失败后仍持锁和暂停，先修复
collector，再恢复 immediate 并配对释放本次 lock、STW 和预算层；不强行清空外层
暂停的 collectorState/pauseDepth，也不释放其他调用者的递归锁层。

本组根住的 NATIVE_DATA 扫描异常发生在 atomic 翻转 generation 之前。仅当 generation
未变且 running status 为 PAUSED、FLAG_PROPAGATION、BEFORE_ATOMIC 或 ATOMIC，失败
分支才废弃标记：取消并发标记状态，调用无分配/无回调的 prepare_major_collection，
重置全对象链的活对象颜色、gcList 和 forwarding 元数据；再清各工作链和 sweeper。
这避免只清灰队列却保留 referenced 标记，导致重试漏扫尚未完成的子图。保留对象、
区域、根和 ownership 的真实所有权，不复原已被覆盖的旧灰链。

失败后的 phase/快照为 IDLE，running status 为 PAUSED，保留 FULL 重试请求和运行
意图；不清未满足的 explicit 请求，不抵扣债务，也不记录完成一次 collection。
后续回收重新扫描全部根。catch 内不会调用 RestartCollection，以免立即再次进入
尚未撤障的扫描回调。

GcEnd 会 Poll，预算到期可能清 hasCurrentException 并发布 EXECUTION_TERMINATED。
异常路径在收尾前保存 Error 的存在标志和 currentExceptionStatus，GcEnd 后恢复，
再 Throw 原状态；currentException 的 ownership 槽本身不复制、不释放。预算终止原因
仍锁存，后续正常 Poll 可观察它。正常 FullGC 的预算 Poll 行为保持原样。当前四例
预算没有启用 deadline/cancel/时间上限，因此该交错和嵌套暂停层数仅有代码审查信用。

generation 已翻转或已进入 sweep/finalizer/compaction 时不执行上述重置。尤其 sweep
会在析构前摘链，异常时已失去链上对象或已释放部分存储，重置标记不能回滚这些操作。
外围锁/暂停/预算仍会配对释放，但不得从本组测试推出这些晚期失败可重试或无泄漏；
晚期析构失败必须独立设计释放事务和回归，不能将其冒充本次 mark abort 信用。

## 夹具与预期

目标 `zr_vm_gc_scan_exception_cleanup_test` 使用实际 Core，并通过私有域头读取
锁和作用域状态。片段 `gc-scan-exception-cleanup-tests.cmake` 只声明目标和四个
独立进程，不改变共享测试入口。CTest 前缀是 `core_gc_scan_exception_cleanup_`：

| case | 模式 | 扫描回调 |
| --- | --- | --- |
| incremental_throw | incremental | 抛出原 RuntimeError |
| generational_throw | generational | 抛出原 RuntimeError |
| incremental_control | incremental | 正常返回 |
| generational_control | generational | 正常返回 |

每个进程先通过真实 Throw/TryRun 构造 currentException，并创建 Error 根句柄。
随后用 RawObject_New 创建合法 NATIVE_DATA、完成载荷初始化、安装专属扫描回调并
建立真实根句柄。回调仅在目标 FullGC 调用期间 arm；构造和根登记期间不会注入
异常。扫描 Throw 时已有真实 Error，不需要在 GC 内重新规范化或分配 Error。
Error 使用可解析根句柄比对，允许分代 GC 正常转发对象地址。

测试记录入口 scopes、活动根数、暂停状态、pauseDepth、预算 gcDepth 和 immediate
标记。受保护 FullGC 返回后须恢复这些状态，保留原 Error 与异常状态；正常调用
须真正到达返回位置，异常调用须跳过它。callback 命中计数避免暂停或短路返回被
误算为已执行扫描。

独立 worker 对真实 mutation lock 做一次非阻塞 try-lock，成功时立即 unlock，
随后自然 join。worker 不附着 VM、不借用 state、不等待 safepoint；测试不使用
同线程递归获取锁作为释放证据。所有首次清理断言均在 join 之后。Windows join
异常时直接结束进程，绝不让栈上探测上下文离开作用域后仍被线程使用。

在完整清理检查通过后，夹具取消注入并再次执行 FullGC，验证同一 collector 可
继续扫描并正常返回；释放两个根、清理 Error 并销毁真实 global。宿主分配器按
所有类别跟踪地址、大小和标志，要求正常销毁后所有 VM 分配归零。

## 信用边界与失败退出

这只验证台账指定的“扫描异常已到达受保护 FullGC”条件合同。回调由本夹具拥有，
不声称任意 foreign callback 可以执行任意 VM 操作或 Throw；公开回调双阶段契约
仍是独立议题。测试只在标记阶段注入，不为 sweep 摘链后析构失败、所有资源回调、
GcStep minor 疏散 OOM 或并发遥测竞态提供信用。

若 RED 遗留锁、暂停或不平衡预算，夹具先输出真实观测再返回失败，不调用危险的
VM 析构，也不手工补锁/暂停/预算使断言通过。操作系统结束进程只用于隔离失败，
不能称为 VM 零泄漏。产物与后续 RED/GREEN 收据由 E 根专属 child manifest 管理。
