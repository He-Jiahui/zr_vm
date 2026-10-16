---
related_code:
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/src/zr_vm_core/gc/gc_debt.c
  - zr_vm_core/src/zr_vm_core/base/memory.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_sweep.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c
  - zr_vm_lib_system/src/zr_vm_lib_system/vm/vm.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/gc/gc_debt.c
  - zr_vm_core/src/zr_vm_core/base/memory.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_sweep.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c
  - zr_vm_lib_system/src/zr_vm_lib_system/vm/vm.c
plan_sources:
  - .codex/plans/20261016-review-bugfix.md
  - user: 根据会话 01a13463 的已确认 BUG 修复现役代码，允许必要接口迁移
  - docs/code-review/coverage/zr_vm_core_memory.tsv
tests:
  - tests/core/test_gc_allocation_debt.c
  - tests/cmake/gc-allocation-debt-tests.cmake
doc_type: module-detail
---

# GC 分配债务的并发与边界

GC 债务表示安全点尚需偿还的分配工作量，不等于当前存活内存。成功的 GcMalloc 计入分配字节；GcReallocate 只计入成功增长的差额，失败和释放不增加债务。原生 RawFree 不单独抵扣此工作量。回收和 GC 步骤通过同一个调整入口抵扣，结果始终处于零与 ZR_MAX_MEMORY_OFFSET 之间。

## 唯一读写协议

`gc_debt.c` 提供 AddDebtSpace 与 GetDebtSpace。管理器构造后、尚未发布给其他线程前可以直接初始化 gcDebtSize 为零；发布后的生产访问全部经这两个函数。分配器、调度器、预算、清扫、步骤观测和 system.vm 统计使用同一协议，避免原子写与普通读混用。

Windows 按宿主指针宽度使用 InterlockedCompareExchange 或 InterlockedCompareExchange64；其他支持编译器使用 __atomic 内建操作。现有 ptrdiff_t 字段的布局保持不变。GetDebtSpace 是单个原子快照，不能代替 region 表、根表和其余遥测字段所需的域同步。

AddDebtSpace 在 CAS 循环中从最新值计算结果，竞争失败重新计算，防止成功分配的增量被覆盖。正增量先比较 MAX - delta 再加；负增量与非负旧值的相反数比较，避免对最小负数取反。无符号分配字节转为有符号增量前先夹到可表示上限。清扫抵扣同样先限制对象字节数；预算统计直接读取非负债务并转为 Int64，避免 32 位宿主把 INT64_MAX 窄化后误判。

## 真实复现与覆盖

`core_gc_allocation_debt_saturation` 先把债务加到上限，再执行真实的 1 字节分配与 1→8 字节扩容，要求仍为上限；随后用最小有符号增量扣减，要求为零。`core_gc_allocation_debt_concurrent_allocations` 创建八个已登记 mutator，每个执行 20,000 次 64 字节真实分配，全部 join 后要求债务恰为 10,240,000。夹具仅在所有 worker 停在 barrier 时直接复位基线，不与工作线程并发读写测试字段。

稳定输入的 `integrated-support-batch-2-red` 记录了实际失败：饱和后的分配变成 -9223372036854775808，扩容后仍为负；并发分配得到 10,238,592，少于预期。配置与编译成功，故障属于生产行为，收据 changed_inputs=[]。修复后 integrated-support-batch-3-red 在当前对象布局上实际重新编译，以上两项全部通过，源码输入哈希稳定。该批名称记录整个批次含其他模块红测，不影响这两项的逐项通过证据。

当前执行环境为 Windows x64 / ClangCL。Windows 32 位和 POSIX 原子分支仅静态检查，WSL 权限返回 E_ACCESSDENIED；没有 Linux 动态通过信用。并发 debt 回归不宣称证明整个 GC 或所有遥测字段线程安全。
