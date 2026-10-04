---
related_code:
  - tests/parser/ssa_eis6_fault_binding_rows.c
  - tests/parser/ssa_eis6_fault_exec_ir.c
  - tests/parser/ssa_eis6_fault_scalar_read.c
  - tests/parser/ssa_eis6_fault_allocator.c
  - tests/parser/ssa_deopt_aggregate_fault_allocator.c
  - tests/parser/ssa_deopt_aggregate_fault_allocator.h
  - tests/parser/ssa_cfg_effects_fault_core.c
  - tests/parser/ssa_cfg_effects_fault_allocator.h
  - tests/parser/ssa_oracle_resume_fault_allocator.c
  - tests/parser/ssa_oracle_resume_fault_allocator.h
  - tests/cmake/ssa-tests.cmake
  - tests/cmake/ssa-cfg-effects-faults.cmake
  - tests/cmake/ssa-cleanup-tests.cmake
implementation_files:
  - tests/parser/ssa_eis6_fault_binding_rows.c
  - tests/parser/ssa_eis6_fault_exec_ir.c
  - tests/parser/ssa_eis6_fault_scalar_read.c
  - tests/parser/ssa_deopt_aggregate_fault_allocator.c
  - tests/parser/ssa_deopt_aggregate_fault_allocator.h
  - tests/parser/ssa_cfg_effects_fault_core.c
  - tests/parser/ssa_cfg_effects_fault_allocator.h
  - tests/parser/ssa_oracle_resume_fault_allocator.c
  - tests/parser/ssa_oracle_resume_fault_allocator.h
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_ssa_binding_rows_artifact.c
  - tests/parser/test_ssa_deopt_aggregates.c
  - tests/parser/test_ssa_cfg_effects_faults.c
  - tests/parser/test_ssa_oracle_resume.c
  - tests/parser/test_ssa_conditional_cleanup.c
doc_type: module-detail
---

# SSA 测试分配故障的翻译单元边界

这些包装器通过局部宏重编译生产模块，让现有测试在真实准备、增长和清理路径中选择一次分配失败。生产算法和导出的函数名保留；拦截范围是被重编译的翻译单元，不能由此推断整个 Core、进程或其他模块的所有分配都已覆盖。

## 选择规则

| 场景 | 被拦截的调用 | 序号 | 关闭或重置方式 | 状态消费 |
| --- | --- | --- | --- | --- |
| 静态 EIS6 解码 | 三份生产模块中的 `malloc/calloc/realloc/free` | 从 0 开始 | `ArmFailure(SIZE_MAX)` 建立不选故障的计数基线；`DisableFailure` 保留计数并取消所选故障 | 解码后观察尝试数、跟踪溢出和存活分配，再检查清理或重试 |
| aggregate recipe 准备 | 准备模块的 `malloc` | 从 1 开始 | `ssa_deopt_aggregate_fail_allocation(0)` | 操作返回后、下一次设置之前读取 `allocation_failed` |
| CFG 效果池增长 | 被包含 `exec_ir.c` 的全部 `realloc` | 从 1 开始 | `ssa_cfg_effects_fault_fail_reallocation(0)` | 操作返回后先读取命中标志和尝试数，再重置、重试 |
| oracle 恢复与 conditional cleanup | oracle 消费模块的 `malloc/calloc`，共享序列 | 从 1 开始 | `ssa_oracle_resume_fail_allocation(0)` | 操作返回后读取命中标志，再关闭注入并检查旧状态 |

三组本地 ordinal/hit 钩子都使用进程内静态状态，测试须串行设置、执行和观察。重新设置序号会清空尝试数与命中标志。命中标志只记录所选的合成失败；自然 `malloc/realloc/calloc` 返回 NULL 不会设置该标志。读取 getter 不清空状态。

所选序号只失败一次。生产代码在命中后仍可能尝试其他分配；例如 oracle 准备缓冲按需执行多次分配之后才统一检查结果。不能把故障选择理解成“此后所有分配失败”，也不能将尝试数理解成成功分配数。

## 为什么先声明 libc 再重定向

三组本地 ordinal/hit 包装器先包含原系统声明，并在定义分配器宏之前定义钩子函数。钩子中的普通分配调用因此仍委托给 libc。三份 EIS6 包装器先包含私有钩子声明与 libc 声明，再定义局部宏；它们调用独立 `ssa_eis6_fault_allocator.c` 中的钩子。下方包含的生产模块经过局部重定向；已有内存与失败清理继续使用匹配的 libc 释放路径。

EIS6 同时拦截释放以追踪图和临时缓冲的存活量。aggregate 的普通 storage 模块与 oracle 的 `ResultFree` 模块在各自另一翻译单元处理释放。CFG 的普通 `free` 留在被重编译的 `exec_ir.c` 中，未被重定向；oracle 同一翻译单元内临时 owner witness 的 `free` 也保持普通 libc 调用。

## 构建与实际使用

EIS6 三份包装器仅在 `BUILD_STATIC_LIB AND NOT BUILD_SHARED_LIB` 的 binding-rows artifact 测试分支中加入，同时定义 `ZR_TEST_EIS6_FAULT_INJECTION`。共享构建不能被当成已经执行该故障扫描的证据。三个模块共用一份 EIS6 跟踪器；其容量和其它 API 的完整审查属于独立范围。

CFG fault target 先从 builder 源列表移除普通 `exec_ir.c`，再加入故障版本，避免同时提供两份 Core 实现。当前 fixture 实际命中 phiIncoming 与 memoryToken 两次增长，失败后检查逻辑计数与已发布图，允许先前增长的 capacity 保留；宏本身仍影响该 TU 的全部 realloc。

aggregate fixture 在两个 recipe 数组分配处分别注入，第三个序号作为未命中的成功对照。它检查原 materialized target、roots 与 aggregate 字段保持；这不表示所有 materialize 分配点都由本钩子覆盖。

conditional-cleanup target 继承 oracle-resume target 的源列表，因此也复用 oracle 分配包装器。拦截包括异常边的临时 owner witness，而不止 values、events 与 ownerStates 三份准备数组。

## 本批审查与验证范围

本批审查范围是六份 C 包装器和三份私有 API 头的 59 个显式或独立契约单元。被包含的生产函数仍归原物理源文件，不因包装器重编译而重复授予函数完成信用。

TODO: 本批原始与注释版本的构建和现有测试比较尚未执行。后续入口为 `ssa_binding_rows_artifact`、`ssa_deopt_aggregates`、`ssa_cfg_effects_faults`、`ssa_oracle_resume`、`ssa_conditional_cleanup`。非注释 token、补丁与台账检查仅提供静态证据，不表示运行通过或故障注入已经执行。
