---
related_code:
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/meta.c
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
  - zr_vm_core/src/zr_vm_core/execution/execution_meta_access.c
  - tests/core/test_object_shape_transition_cache.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/meta.c
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
  - zr_vm_core/src/zr_vm_core/execution/execution_meta_access.c
plan_sources:
  - docs/plans/benchmark/optimize/03-memory-object-gc.md
tests:
  - tests/core/test_object_shape_transition_cache.c
doc_type: testing-guide
---

# Object Shape Transition Cache Acceptance

## Scope

本验收记录 Task 3 的运行时合同：prototype 稳定 shape ID、mutation
generation、4 项固定 PIC、generation mismatch fail-closed，以及
mono/poly/mega/meta profile 名称。它同时记录相邻 member/GC 回归和
`object_field_hot` 的确定性指令数证据；墙钟吞吐门禁仍单独判定。

## Test-First Evidence

新增 `test_object_shape_transition_cache.c` 后，在 shape 字段尚不存在时用严格 GCC 语法检查得到预期编译错误；加入字段、generation mutation 和 PIC 写入后，同一测试源通过严格 GCC 语法检查。

## Implementation Checks

- `ZrCore_ObjectPrototype_MarkMutation` 统一推进 shape generation 和 legacy member version。
- descriptor、super、meta 和 module prototype accessor mutation 都调用统一 helper。
- member access、meta access、known VM member dispatch 使用 shape-aware slot matcher。
- `ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY` 固定为 4，slot 不执行动态分配。
- profile metric names include mono/poly/mega/meta fallback categories。
- 精确 receiver object/pair 身份命中继续直接访问同一 pair，即使 prototype
  mutation 已推进 version/generation；命中后同步 slot 的 receiver/owner shape
  元数据。prototype、descriptor、callable 和 meta 路径仍在任何代数失配时
  fail closed。
- 固定容量替换测试先填满 A/B/C/D，再验证 E 替换 A、B 保持命中、A 的后续
  miss 按环形游标替换下一项，避免继续依赖旧的 2-slot 假设。

## Validation

在 2026-08-30，以下聚焦二进制在 ext4 构建树中通过：

- GCC 11.4 Release：shape `3/3`、member access `102/102`、frame slot
  `14/14`、GC `67/67`；
- Clang 14 Debug：shape `3/3`、member access `102/102`、frame slot
  `14/14`、GC `67/67`；
- MSVC 19.44 Debug：shape `3/3`、member access `102/102`、frame slot
  `14/14`、GC `67/67`；
- Clang 14 ASan Debug，`detect_leaks=1`：同一组 `3/3`、`102/102`、
  `14/14`、`67/67` 均通过，无 ASan/LeakSanitizer 报告。

ASan shape 测试曾在与其他进程输出重定向串联时出现一次无 sanitizer
诊断的进程崩溃；隔离复跑稳定通过 `3/3`，因此不把该 harness 现象解释为
运行时通过之外的额外结论。

## Performance Evidence

相同 ext4 GCC 11.4 Release build、相同 scale-1 profile 输入和 checksum
`623146080` 的手工 Callgrind 对比：

| Metric | Before | Dispatch helper | Final | Decision |
|---|---:|---:|---:|---|
| total Ir | 205,647,828 | 159,970,049 | 126,716,379 | -38.38% total |
| frame VALUE-slot getter/helper exclusive Ir | 79,975,168 | 31,418,398 | folded below top-level symbol report | first stage -60.71% |
| direct / checked frame helper count | 1,686,066 / 1 | 1,686,066 / 1 | 1,686,066 / 1 | unchanged |

第一阶段由 dispatch 使用已缓存的 profile runtime 和已验证 direct VALUE-slot
元数据；第二阶段在 cached-name inline-member probe 之前证明 receiver 是 direct
VALUE，从而跳过只适用于 inline struct/union 的名称解析和 frame-place 探测。
两项都不把 shape/PIC 自身的收益与相邻 frame 热点混为一谈。最终 process run
的 ZR interp median 为 `136.382 ms`，相对最初 `199.464 ms` 的诊断变化为
`-31.62%`，但 17 样本 CV 为 `16.52%`、`gate_eligible=false`，不能用于
接受 `object_field_hot +20%` 墙钟门禁。

## Deferred Gates

`object_field_hot +20%` 墙钟门禁仍开放。`mixed_service_loop` 已完成 profile，
但其 `-52.85% Ir` 属于后续 frame/call-boundary 优化，不作为 shape/PIC 的
独立收益；该 case 的墙钟门禁仍开放。
后续还需补齐 GC move、跨模块对象和动态 meta fallback 的独立 binary tests，
并把固定 4-slot 容量纳入 RSS/每 call site 内存报告。在这些证据齐全前，
不报告 Task 3 性能门禁完成。
