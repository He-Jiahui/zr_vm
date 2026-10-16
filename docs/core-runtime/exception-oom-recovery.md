---
related_code:
  - zr_vm_core/src/zr_vm_core/exception/exception.c
  - zr_vm_core/src/zr_vm_core/exception/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/exception/exception_internal.h
  - zr_vm_core/src/zr_vm_core/base/memory.c
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/include/zr_vm_core/state.h
  - tests/core/test_core_exception_oom.c
  - tests/cmake/core-exception-oom-tests.cmake
implementation_files:
  - zr_vm_core/src/zr_vm_core/exception/exception.c
  - zr_vm_core/src/zr_vm_core/exception/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/exception/exception_internal.h
plan_sources:
  - user: 2026-10-16 修复现役 Core 支撑缺陷并提供真实分配失败回归
tests:
  - tests/core/test_core_exception_oom.c
  - tests/cmake/core-exception-oom-tests.cmake
doc_type: module-detail
---

# 异常规范化期间的持续 OOM 恢复

## 调用链与边界

`GcMalloc` 首次申请失败后，在全局初始化完成且未处于即时 GC 时执行完整 GC 并重试。最终失败调用 `Throw(MEMORY_ERROR)`。`Throw` 在存在本线程恢复点且没有当前异常时，尝试为原生状态构造 Error。MemoryError 的原型查询键、Error 对象及诊断字段均可能分配；持续 OOM 会从这些分配再次进入 Throw。在实际跳转前反复规范化会耗尽宿主栈。

bootstrap 检查仍然有效：global 或基础 Error 原型尚未建立时，NormalizeStatus 直接返回 false，不查询字符串或构造错误对象。初始化完成后的持续 OOM 由独立的规范化重入保护处理。

## 私有恢复点状态

`exception_try_run.c` 的私有 `SZrExceptionTryRunContext` 保存 `statusNormalizationInProgress`。每次 TryRun 初始化为 false；它与公开 `SZrExceptionLongJump` 的 status 分离，不改变公开结构尺寸或函数签名。公开恢复点仍为私有 context 的首成员。与既有 scope 恢复 helper 相同，Begin/End 只接受活动 TryRun 创建的恢复点；宿主不得自行安装或复制恢复点。

首次 Throw 需要规范化时，Begin 设置 guard。普通返回后 End 清除 guard；若规范化内部再次 Throw，Begin 返回 false，重入直接写线程状态、恢复点状态并沿既有 scope 恢复和非局部跳转路径返回。实际 OOM 返回 MEMORY_ERROR，不伪造 `hasCurrentException`。非局部离开时，拥有 guard 的 TryRun context 随恢复点撤销，无需在被放弃的调用帧上清理。

正常分配条件下，MemoryError 仍构造 Error 对象。已有 currentException 的快速路径保留原对象，不启动新的规范化。嵌套 TryRun 有独立 guard，内层撤销后恢复外层指针；后续受保护调用可继续规范化错误。

status 在真正 Throw 前仍为 FINE，因此 forced-C++ 模式中规范化期间的外来 C++ 异常仍由现有 catch-all 转成 INVALID。此修改不扩大 forced-C++ 析构重入或跨原生线程 longjmp 的保证。旧转发路径仍使用其原有 scope unwind 策略，guard 本身不读取线程私有 GC 快照。

## 回归与证据

回归链接实际 Core，不替换 Throw、TryRun 或 GC。分配器在武装期间拒绝每个非零请求，允许释放，并检查每个旧指针的原始尺寸和类别。OOM 持续到 TryRun 实际返回，避免一次性注入掩盖递归。每案最终释放 GlobalState 并要求没有存活分配或尺寸/类别错误。

| CTest 后缀 | 检查 |
| --- | --- |
| persistent_allocation_oom | GcMalloc 失败返回 MEMORY_ERROR，恢复分配后 RuntimeError 对象语义可继续 |
| runtime_normalization_oom | 原始 RuntimeError 的规范化分配失败改为 MEMORY_ERROR，随后可恢复 |
| normal_memory_error | 普通 MemoryError 保留对象规范化和状态 |
| existing_error_oom | 既有 Error 在持续 OOM 时保留身份，传播不新增分配请求 |
| nested_oom | 内层 OOM 恢复外层恢复点，外层后续 RuntimeError 仍可规范化 |

真实 RED 已记录在 `E:/cargo-targets/zr_vm/reports/review-bugfix-20261016/integrated-support-oom-string-red-r2-2.log`：前两项均输出持续 OOM 标记后 SegFault，没有编译或夹具失败。修复后的五项已在 `integrated-support-batch-green` 实际构建中全部通过；同轮 53 项 CTest、pool 14 项及 CLI hello-world 通过，收据 `changed_inputs=[]`。独立只读复核 ACCEPT，未将代码审读代替动态证据。当前证据来自 Windows clang-cl，不宣称 Linux 或 forced-C++ 动态验证通过。
