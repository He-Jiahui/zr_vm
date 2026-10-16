---
related_code:
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_common/include/zr_vm_common/zr_object_conf.h
  - zr_vm_core/include/zr_vm_core/raw_object.h
  - zr_vm_core/src/zr_vm_core/gc/gc_roots.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/src/zr_vm_core/gc/gc_internal.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_internal.h
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - tests/core/test_gc_root_leases.c
  - tests/cmake/gc-root-leases-tests.cmake
implementation_files:
  - zr_vm_core/src/zr_vm_core/gc/gc_roots.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
plan_sources:
  - user: 2026-10-16 修复共享 ignored-root 登记与 native-call 临时保活失败契约
  - parent: support-batch-2-red 真实失败后授权支持层修复
tests:
  - tests/core/test_gc_root_leases.c
  - tests/cmake/gc-root-leases-tests.cmake
  - tests/core/test_gc_ignored_root_concurrency.c
  - tests/cmake/gc-ignored-root-concurrency-tests.cmake
doc_type: module-detail
---

# 共享临时根与本地调用 pin

## API 与所有权

`SZrGcRootLease` 是借用对象地址的临时强根凭据，先零初始化，调用
`ZrCore_Gc_RootLeaseAcquire(state, object, &lease)`，最后用同一个 global
调用 `ZrCore_Gc_RootLeaseRelease(global, &lease)`。对象须属于该 global，调用
Acquire 时仍有效；state 必须已附着其域。凭据不可复制、不可跨域释放，必须覆盖
整个使用窗口，异常退出由调用方在受保护调用返回后释放。空对象成功产生空凭据；
非空凭据重复 Acquire 返回 false；Release 清空凭据，重复释放空凭据无效果。

Acquire 不更改对象的 storage、region、escape 或 pin 标志。临时引用计数保存在
对象标记中，登记表对同一对象只存一项。GC 在临时引用计数非零时禁止复制搬迁，
可原址晋升，因此凭据的地址持续有效；释放后恢复普通搬迁资格。

显式 IgnoreObject 是独立的幂等所有权来源，不按调用次数计数。它在临时 lease
存在时同样登记显式来源。UnignoreObject 清除显式来源，但不能撤除仍有 lease
的根；最后一个临时 Release 仅在没有显式来源时删除登记。普通图引用接管旧显式
根的既有 escape 行为保留，但活跃 lease 不会被 escape 路径撤销。

IgnoreObjectIfNeededFast 的检查与登记现在是单次锁内操作；它借用已存在的根时
不取得独立所有权。需要覆盖重叠调用生命周期的新调用方应使用 RootLease。
IsObjectIgnoredFast 同样走同步查询，Fast 名称不表示可无锁读取数组或对象索引。

## Native pin 的事务

NativeCallPinObject/Value 共用临时根引用计数，并另记 native pin 引用数。
第一个 native pin 才应用 pinned storage/region 和 escape 信息；后续重叠 pin
仅增加计数。最后一个 native token 释放时，只清除这组临时调用实际取得的 native
pin 位。先前已有的 pin，或期间通过公开 PinObject 取得的持久 pin，不由临时
释放清除。token 中两个 AddedByCaller 字段保留为本次取得时的观察值；释放依据
共享计数，不以最早取得者决定根生命周期。所有 token 都必须一次取得、一次释放。

先完成 ignoredObjects 容量预留，再变更对象计数与 pin 信息。登记表容量及字节
乘法检查溢出；原生 ARRAY 分配失败保留旧表、count、capacity 和所有对象索引。
pinned region 获取失败时先撤销本次临时根，不改变原对象 storage/region/escape
或 pin；获取新 region 成功后才释放旧 region 记账。公开 void PinObject 失败会在
释放同步保护后 Throw MEMORY_ERROR。正常 Unpin 保留成功晋升形成的 storage、region
与 escape 记录，不伪造回滚。

资源析构 DropProtected 和 pending-control 值复制原先手工设置、清除 native 位，
现改用同一套 NativeCallPin token，避免覆盖其他重叠调用的保活窗口。获取失败先
恢复 pending-control / AOT root frame，再返回或抛 MEMORY_ERROR。

## 同步与 GC 顺序

根表的查找、插入、删除、扩容、对象索引更新和根计数均由域的递归 mutationLock
保护。全局查询与 Release/Unignore 不分配，不轮询，不启动 GC；带 state 的取得
与 escape/pin 使用 SharedMutationBegin，所有收集阶段都持锁，复用已有 mutationDepth
记账，因此 TryRun 的 scope 恢复可释放异常跨越的锁层级。

进入时先在 coordinationLock 下检查暂停边界，释放协调锁后才等待 mutationLock；
持 mutationLock 时仅短暂取得 coordinationLock 更新深度。GC 先完成 STW 握手再
取得 mutationLock；释放 mutationLock 后才结束 STW。调用方不得在持有 mutationLock
时启动 GC 或等待 STW。嵌套 escape 传播没有分配、GC 或 Poll，MutationEnd 只在
释放最外层锁后处理延迟 Poll。GC 扫描和重写登记表继承回收驱动器持有的 mutationLock。

公开 stats snapshot 读取根计数也使用同一把锁。其他 region 分配路径和压力遥测
仍有既有并发审计范围，不能据此声称整份 stats snapshot 已无数据竞争。

## 回归与证据

真实 support-batch-2-red 中，explicit_during_pin、same_object_threads、reserve_oom
及旧 ignored-root 两个 Unity 场景失败；生产修改前输入 changed_inputs=[]。
主代理随后在当前对象标记布局上实际统一重编，integrated-support-batch-3-red 中本模块七项全部通过，输入哈希稳定。

owned target `zr_vm_gc_root_leases_test` 每个 case 独立进程、TIMEOUT 30：

| case | 断言 |
| --- | --- |
| explicit_before_pin | 原显式根不被 Unpin 移除 |
| explicit_during_pin | pin 期间新增的显式根保留 |
| same_object_threads | 两线程重叠 pin，先释放者退出后仍有根及 native 位 |
| reserve_oom | 精确 ARRAY 尺寸故障，登记表和对象全部关键字段不变，撤故障后重试 |
| region_reserve_oom | 实际 live native pins 填满 region 表，预先保留目标 lease/显式根，只拒 region 元数据 ARRAY 增长；对象完整标记、旧 region 记账/表及根所有权不变，关闭故障后重试 |
| temporary_roots | 重复同对象、显式来源独立、空/重复释放、真实完整 GC 后凭据仍有效 |
| distinct_root_threads | 四线程各 96 根、12 轮同时增长/查询/交错删除，最终索引与计数回零 |

allocator tracker 核对所有非零分配类别/尺寸、释放归属与最终零残留；线程夹具在
detach 或访问结果前 join 全部已启动线程，启动失败会取消等待。旧 foreign repro
源码保持原状，不纳入本任务交付和最终通过信用。Core 的 CONFIGURE_DEPENDS 源扫描发现新增
`gc_roots.c`；统一验证需要重新配置并重编译所有受对象标记布局影响的模块。

region_reserve_oom 是独立评审后补强的定向覆盖，旧实现未运行此项 RED。该新夹具已在上述统一批次通过；不得据此追记历史红测。对象池、所有 live pin 及故障状态均在退出前清理，最终由 allocator tracker 核对零残留。
