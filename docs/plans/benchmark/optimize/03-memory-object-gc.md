---
related_code:
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/src/zr_vm_core/string.c
  - zr_vm_core/include/zr_vm_core/string_builder.h
  - zr_vm_core/src/zr_vm_core/string_builder.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/include/zr_vm_core/string_builder.h
  - zr_vm_core/src/zr_vm_core/string_builder.c
plan_sources:
  - user: 2026-08-29 VM performance optimization toward mainstream runtimes
  - docs/plans/benchmark/optimize/00-audit-and-baseline.md
tests:
  - tests/core/test_hash_set_dense_paths.c
  - tests/core/test_execution_member_access_fast_paths.c
  - tests/gc/gc_tests.c
  - tests/benchmarks/cases/array_index_dense
  - tests/benchmarks/cases/object_field_hot
  - tests/benchmarks/cases/gc_fragment_baseline
  - tests/benchmarks/cases/gc_fragment_stress
  - tests/core/test_string_builder.c
  - tests/acceptance/2026-08-30-string-builder.md
doc_type: milestone-detail
---

# 内存、对象与 GC 优化计划

> **Goal:** 降低容器和对象热路径的间接访问、48 字节值搬运、重复分配和 GC 扫描成本，同时保持所有权、弱引用和并发 GC 语义。
>
> **Architecture:** typed container 和 shape slot 使用单一 canonical storage；通用对象/value 只在动态接口边界物化。GC 以分配速率、扫描字节、停顿和 survivor 为证据调整，不靠减少安全检查换吞吐。
>
> **Tech Stack:** C11、现有 object/hash/GC runtime、profile counters、Valgrind Massif/Callgrind、Unity、benchmark registry。

## 已有能力与真实缺口

整数数组已经有 `superArrayRawIntData`、length/capacity/dirty 状态，不应再计划“添加连续整数数组”。缺口是原始侧存储和通用 node map 的同步、物化与生命周期边界。对象成员路径已有 member slot/cache 快路，缺口是 shape 稳定性、cache 失效成本和未命中回退。GC 已有并发/分代相关路径，缺口必须用 allocation/mark/rewrite/pause 指标重新排序。

## Task 1：为内存行为增加可决策指标

**Files:**

- Modify: `zr_vm_core/include/zr_vm_core/profile.h`
- Modify: `zr_vm_core/src/zr_vm_core/profile.c`
- Modify: `zr_vm_core/src/zr_vm_core/object/object.c`
- Modify: `zr_vm_core/src/zr_vm_core/gc/gc_cycle.c`
- Modify: `tests/benchmarks/scripts/hotspot_summary.py`
- Test: `tests/core/test_value_construction_profile.c`

**Steps:**

1. 记录分配次数/字节、value copy 字节、write barrier、minor/full collection、mark/rewrite 对象数、promoted bytes、最大/总停顿。
2. 记录 raw-int hit、node-map materialization、raw/node synchronization 和 member cache hit/miss/invalidation。
3. profile 关闭时所有计数分支必须被 `ZR_UNLIKELY` 保护且不分配；用 numeric 基准验证开关关闭回退小于 1%。
4. 为 array/object/string/gc case 输出每操作分配字节和每操作扫描字节。

## Task 2：数组 canonical storage

当前实现状态：canonical raw-int storage、显式 materialization/generation 边界、
通用 API 物化和 raw buffer 生命周期已实现。聚焦 canonical-storage 测试在
GCC、Clang、MSVC 均通过 7/7；container runtime 49/49、GC 67/67，Valgrind
记录 3,609 alloc/free、0 bytes in use、0 errors。ext4 GCC Release 的
`array_index_dense` core checksum 为 `723012102`，C 和 ZR interp 一致。
Clang 14 ASan Debug 在 `detect_leaks=1` 下通过 canonical-storage 7/7 和完整
GC 67/67；验证中修复了 `RELEASED` 对象在 shutdown 清扫时跳过 raw free 的泄漏。
process scope 的 20 样本仍不稳定：C median `4.095 ms`、CV `7.07%`，ZR interp
median `205.810 ms`、CV `17.18%`，两者均 `gate_eligible=false`。persistent
steady 协议当前不支持该 case，因此不跨 scope 比较，`+20%` 性能门禁保持开放。

**Files:**

- Modify: `zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h`
- Modify: `zr_vm_core/src/zr_vm_core/object/object.c`
- Modify: `zr_vm_core/src/zr_vm_core/gc/gc_object.c`
- Test: `tests/core/test_inline_struct_array_layout.c`
- Add: `tests/core/test_super_array_raw_int_canonical_storage.c`

**Steps:**

1. 写测试覆盖 raw-int append/get/set、通用迭代、clone、GC move、类型漂移、反射访问和从 int 切换到 object。
2. 对纯 int 数组把 raw buffer 定为 canonical；普通 index/get/set 不创建 node-map pair。
3. 只有进入要求通用 `SZrTypeValue`/pair identity 的 API 时按边界物化 node map，并记录 generation。
4. 通用写入使 raw view 失效或执行一次可证明的类型检查后同步；不得维护无 generation 的双向脏状态。
5. 接受条件：`array_index_dense` 至少提升 20%，每元素分配接近 0，GC clone/rewrite 测试无回归。

## Task 3：对象 shape 与 member slot

当前正确性状态：prototype shape ID/generation、固定 4-slot PIC、shape-aware
member/meta/dispatch 校验和精确 receiver pair 的安全 generation 刷新已实现。
GCC Release、Clang Debug、MSVC Debug 依次通过 shape `3/3`、member
`102/102`、frame `14/14`、GC `67/67`；Clang ASan/LeakSanitizer 在
`detect_leaks=1` 下也通过。相同 checksum `623146080` 的 ext4 GCC Release
Callgrind 总 Ir 从 `205,647,828` 经 `159,970,049` 降至 `126,716,379`
（累计 `-38.38%`）。墙钟诊断 median 改善 `31.62%`，但最终 CV
`16.52%`、`gate_eligible=false`，
因此 `object_field_hot +20%` 仍开放。`mixed_service_loop` 已完成 profile，
但其 `-52.85% Ir` 属于 frame/call-boundary follow-up，不作为 shape/PIC 的
独立收益；该 case 的墙钟门禁仍未验收。

**Files:**

- Modify: `zr_vm_core/include/zr_vm_core/object.h`
- Modify: `zr_vm_core/src/zr_vm_core/object/object.c`
- Modify: `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- Modify: `zr_vm_core/src/zr_vm_core/execution/execution_internal.h`
- Test: `tests/core/test_execution_member_access_fast_paths.c`
- Add: `tests/core/test_object_shape_transition_cache.c`

**Steps:**

1. 为 prototype/shape 增加稳定 id 与 generation 测试，覆盖字段新增、prototype mutation、meta fallback、GC move 和跨模块对象。
2. 单态 cache 保存 shape id、generation 和 slot index；双态/小型多态站点使用固定 4 项 PIC，不做热路径分配。
3. member write 改变 shape 时只失效相关 generation；动态 meta/proxy 对象直接走 slow path。
4. profile 分开记录 monomorphic、polymorphic、megamorphic 和 meta fallback。
5. 接受条件：`object_field_hot` 至少提升 20%，`mixed_service_loop` 至少提升 10%，cache 内存每 call site 有硬上限。

## Task 4：字符串构建与临时对象

**Files:**

- Modify: `zr_vm_core/src/zr_vm_core/string.c`
- Add: `zr_vm_core/include/zr_vm_core/string_builder.h`
- Add: `zr_vm_core/src/zr_vm_core/string_builder.c`
- Modify: `tests/benchmarks/cases/string_build/zr/src/main.zr`
- Modify: `tests/benchmarks/cases/string_build/c/benchmark_case.c`
- Modify: `tests/benchmarks/cases/string_build/dotnet/benchmark_case.cs`
- Modify: `tests/benchmarks/common/lua/benchmark_runner.lua`
- Modify: `tests/benchmarks/common/qjs/benchmark_runner.js`
- Modify: `tests/benchmarks/common/node/benchmark_runner.js`
- Modify: `tests/benchmarks/common/python/benchmark_runner.py`
- Add: `tests/core/test_string_builder.c`
- Test: `tests/benchmarks/cases/string_build`

**Steps:**

1. 先用 allocation profile 确认每次 append 的对象数和复制字节，区分 VM 字符串与库 builder 成本。
2. 提供容量增长的 mutable builder，最终一次冻结为 immutable string；普通 string 仍保持不可变与 interning 合同。
3. 不引入通用 rope，除非 trace 表明大字符串 concat 的复制字节仍占总成本 20% 以上。
4. 为 UTF-8 边界、embedded NUL、hash cache、GC move 和异常中止增加测试。
5. 接受条件：`string_build` 分配字节减少至少 70%，wall time 提升至少 20%。

## Task 5：GC 参数与布局优化

当前正确性状态：已验证的无 GC/ownership layout 跳过扫描路径在 Clang 14
ASan Debug 下通过 3/3，完整 GC 通过 67/67，且 LeakSanitizer 无泄漏。清扫
现在将 `RELEASED` 解释为“终结完成、等待存储回收”，只跳过重复 finalizer，
仍执行 region/registry/type/raw allocation 清理。GC throughput、baseline overhead
和 p99 pause 门禁尚未测得可接受结果，不能据此宣称 Task 5 完成。

**Files:**

- Modify: `zr_vm_core/src/zr_vm_core/gc/gc_cycle.c`
- Modify: `zr_vm_core/src/zr_vm_core/gc/gc_mark.c`
- Modify: `zr_vm_core/src/zr_vm_core/gc/gc_object.c`
- Modify: `tests/benchmarks/cases/gc_fragment_baseline`
- Modify: `tests/benchmarks/cases/gc_fragment_stress`
- Test: `tests/gc/gc_tests.c`
- Test: `tests/core/test_gc_concurrent_major.c`

**Steps:**

1. 将 GC 基准分成 allocation-throughput、short-lived、high-survivor、large-object、weak/owner 和 explicit-collect 六类；Lua/.NET 只比较语义等价子集。
2. 用 bytes allocated per collection、survival rate 和 pause target 校准 nursery/major trigger，不硬编码只对当前 case 有利的阈值。
3. 对无 GC/ownership 字段的 layout 使用已验证 bitmap/descriptor 跳过逐 value 扫描；任何未知 layout fail closed。
4. 所有 write barrier 合并必须证明 destination generation 和 source liveness，不允许用 benchmark 绕过 barrier。
5. 接受条件：baseline GC overhead 小于 10%，stress throughput 提升至少 15%，p99 pause 不回退超过 10%，Valgrind/ASan 无泄漏或 UAF。

## 内存门禁

- RSS 提升超过 5% 的优化必须给出吞吐收益与可配置上限，否则拒绝。
- 每个 cache/sidecar 都有 owner、free、clone、serialization 和 GC rewrite 测试。
- 任一 owner/borrowed/weak 语义失败均立即回退，无“性能模式”例外。
