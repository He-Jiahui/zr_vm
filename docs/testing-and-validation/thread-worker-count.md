---
related_code:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
implementation_files:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - tests/thread/test_thread_worker_count.c
  - tests/cmake/thread-worker-count-tests.cmake
plan_sources:
  - "user: 2026-10-16 支撑层现役 BUG 修复与真实回归"
tests:
  - tests/thread/test_thread_worker_count.c
  - tests/cmake/thread-worker-count-tests.cmake
doc_type: testing-guide
---

# ThreadScheduler workerCount 输入边界

夹具登记真实 task/thread 模块并导入注册 ThreadScheduler 原型，从公开模块描述符查找 CONSTRUCTOR 回调；使用实际公开 CallContext、一个 INT64 实参和已 lease 的真实接收对象调用，不复制构造实现。输入 4294967296、4294967297、INT64_MAX 应依既有 descriptor 验证合同拒绝：回调普通返回 false，TryRun 为 FINE，threadStatus 与 currentExceptionStatus 为 EXCEPTION_ERROR（不是 Throw(RuntimeError)），并在接收对象发布 queue、计数、策略或原生 provider 前退出。零和负一保持既有拒绝控制；一保持正常成功控制并检查返回对象及保存计数。

真实构造器及 attached/isolated init 都只记录 workerLimit、创建 provider 同步状态并注册 await hook。OS worker 启动延后到 schedule；本夹具从不调用 schedule，因此错误接受 INT64_MAX 也不会创建大量线程。没有在途 worker，无需伪造 join，也不授予执行、调度或关闭协议覆盖。

每个 CTest 使用独立进程、30 秒超时及 Windows 无窗口断言诊断。正常配置创建的原生 provider 存在独立的既有缺析构问题；夹具不增加私有手工释放，不宣称 native provider 零泄漏，最终进程退出回收该进程资源。Core GlobalState 正常销毁；本组不使用其分配器统计替代线程或原生资源回收证据。

目标 `zr_vm_thread_worker_count_test`，前缀 `thread_worker_count_`，六项分别为 wrap_zero、wrap_one、int64_max、normal_one、zero、negative。独立片段检查所有八个实际链接目标。稳定 RED r5 的 1934 个输入哈希未变：三个溢出失败，三个对照通过。生产在正数检查后、任何对象或 provider 发布前拒绝大于 UINT32_MAX 的输入，沿用既有错误合同；等待统一 GREEN。不覆盖脚本构造 lowering、uint32 范围内的资源上限或任何并发行为。

## Root acceptance

Actual ClangCL configure/build0, 168/168 CTest, pool14, exceptions15 and CLI hello0; 1944 input hashes unchanged; independent review ACCEPT. Windows only. Evidence: `reports/review-bugfix-20261016/integrated-support-batch-6-green-{0..5}.log` and receipt under the configured external artifact root.
