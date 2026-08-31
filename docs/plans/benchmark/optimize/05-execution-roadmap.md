---
related_code:
  - tests/performance/perf_runner.c
  - tests/cmake/run_performance_suite.cmake
  - tests/benchmarks/registry.cmake
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_precall_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_value_slot_fast.h
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame_copy_fast.h
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot
implementation_files:
  - tests/performance/perf_runner.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_value_slot_fast.h
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame_copy_fast.h
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
plan_sources:
  - user: 2026-08-29 audit, test, and optimize VM performance
  - docs/plans/benchmark/optimize/index.md
tests:
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/core/test_precall_frame_slot_reset.c
  - tests/acceptance/2026-08-30-mixed-service-loop-frame-fast-paths.md
  - tests/acceptance/2026-08-31-dispatch-frame-value-slot-inline.md
  - tests/acceptance/2026-08-31-direct-value-copy-probe-skip.md
  - tests/acceptance/2026-08-31-generated-frame-slot-count-summary.md
  - tests/acceptance/2026-08-31-direct-frame-drop-span-and-no-owner-skip.md
  - tests/acceptance/2026-08-31-direct-value-parameter-in-place-copy.md
  - tests/acceptance/2026-08-31-dispatch-direct-value-copy-probe-bypass.md
  - tests/acceptance/2026-08-31-packed-direct-value-frame-summary.md
  - tests/acceptance/2026-08-31-packed-direct-value-drop-owner-batching.md
  - tests/acceptance/2026-08-31-packed-direct-prepared-precall-fusion.md
  - tests/benchmarks/test_benchmark_registry.c
  - tests/gc/gc_tests.c
  - tests/parser/test_aot_c_typed_scalar.c
doc_type: milestone-detail
---

# 性能优化执行路线图

> **Goal:** 以依赖顺序执行性能工作，确保每一阶段都有可重复基线、失败测试、收益门禁和安全回归。
>
> **Architecture:** M0 修正测量，M1 完成解释器帧/标量热路径，M2 优化对象/内存/GC，M3 推进 AOT parity，M4 依据门禁决定 JIT。
>
> **Tech Stack:** WSL GCC/Clang Debug/Release、CMake/Ninja、CTest、Unity、Callgrind、benchmark JSON/CSV。

## M0：基线与已落地 P0

**Status:** 稠密 frame-slot O(1) 查找与 M1 测量协议已实现并完成聚焦验收；完整 WSL GCC 构建证据仍需在干净构建树补齐。

## 当前执行状态（2026-08-31）

- P0 frame-slot dense lookup：已验收。
- M2 frame-place Task 1：validated direct VALUE-slot 安全切片、dispatch cached-profile/direct-getter inline follow-up、direct VALUE-to-VALUE copy-probe skip 与 strict-summary call-site bypass、direct VALUE inline-member negative guard，以及 mixed-service 的 direct place/init/intersection/reverse/drop、direct VALUE-only frame-drop summary、strict packed-frame initialization/copy/drop loops、packed drop no-profile/owner batching、once-per-frame drop span/no-owner skip 与 direct 参数同址 copy follow-up 已实现；`.zro` 不序列化派生信任位，运行期按当前 frame base 直接寻址，其他布局 fail-closed 回退 checked place。GCC Release、Clang Debug、MSVC Debug 与 Clang ASan/UBSan/LeakSanitizer 的 frame `36/36`、precall `17/17`、postcall `3/3`、member `102/102`、shape `3/3`、GC `67/67` 均通过。最新 owner-batching slice 对 numeric/object 为 `+0.020328%/-0.015146%`，通过 `1%` 代表集门禁；`mixed_service_loop` exact pair 为 `255,021,394 -> 245,339,382 Ir`（`-3.796549%`），相对最初 `868,860,510 Ir` 累计下降 `71.763087%`。墙钟样本仍不稳定或尚无合格 paired series，相关墙钟门禁保持开放。
- M1 Task 1-4：测量范围、persistent protocol、calibration/statistics/randomization、environment identity/isolation/cache/publishing 均已完成聚焦验收；完整 WSL GCC configure/build smoke 超时，未宣称通过。
- M2 Task 1：profile memory metrics 已作为观测 slice 验收，pause/allocation 范围限制见 `tests/acceptance/2026-08-30-profile-memory-metrics.md`。
- M2 Task 2：super-array canonical storage 已完成源码实现；GCC/Clang/MSVC focused binary 均为 7/7，container runtime 49/49、GC 67/67，Valgrind 为 0 errors/0 bytes in use，Clang ASan/LeakSanitizer 也通过 7/7 与完整 GC 67/67。`array_index_dense` checksum 一致，但 20-sample CV 为 7.07%/17.18%，收益门禁仍开放，见 `tests/acceptance/2026-08-30-super-array-canonical-storage.md`。
- M2 Task 3：对象 shape ID/generation、4-slot PIC、shape-aware invalidation 与精确 receiver pair generation 刷新已实现；GCC/Clang/MSVC 的 shape 3/3、member 102/102 和 GC 67/67 通过，Clang ASan/LeakSanitizer 无报告。`object_field_hot` Callgrind 总 Ir 累计降低 38.38%，但墙钟 median 诊断改善 31.62%、CV 16.52%，门禁不可接受。`mixed_service_loop` 已完成 profile，但其 `-52.85% Ir` 来自相邻 frame/call-boundary follow-up，不作为 shape/PIC 的独立收益声明。
- Interpreter Task 4 partial：VM precall 与 tail reuse 现在把实际 frame storage slot count 以 24 位编码保存在 `SZrCallInfo` 既有 ABI padding，零值或超限保留 legacy/native 元数据扫描回退。已发布 `SZrFunction` 还保存不可变、非序列化的 direct VALUE 参数数量/layout 前缀与 direct VALUE-only frame-drop 摘要；loader/compiler finalize 重建，任何未发布或非规范布局回退原 full scan/checked place。参数摘要精确配对为 `409,431,558 -> 396,430,578 Ir`（`-3.18%`）；frame-drop 摘要为 `396,142,221 -> 378,649,763 Ir`（`-4.42%`）；初始化复用为 `378,637,009 -> 365,295,917 Ir`（`-3.52%`）；dispatch direct getter inline 为 `365,326,562 -> 349,179,948 Ir`（`-4.42%`）；direct-to-direct copy probe skip 为 `349,179,948 -> 325,175,994 Ir`（`-6.87%`）。precall `16/16`、tail reuse `4/4`、frame/loader/摘要/inline probe 契约 `28/28` 在 GCC Release、Clang Debug、Clang ASan/UBSan/LeakSanitizer 与全新 MSVC Debug 构建通过。callee identity、返回目的地专化和 generation/token 失效尚未实现，Task 4 总门禁开放。
- Interpreter generated-slot follow-up：`SZrFunction` 尾部新增不可变、非序列化的 `generatedFrameSlotCountPlusOne`，零值保持动态全指令流扫描；loader 在完整复制后重建，quickening 在全部重写后刷新。精确配对把 `mixed_service_loop` 从 `325,175,994` 降到 `314,490,481 Ir`（`-3.286%`），numeric/object 分别 `-0.023%/-0.012%`。GCC Release、Clang Debug、Clang ASan/UBSan/LSan 与全新 MSVC Debug 的 13 项矩阵均通过；precall 更新为 `17/17`，quickening 为 `20/20`。
- Interpreter frame-drop follow-up：strict direct summary 现在先对完整 `frameByteSize` 做一次栈 span 预检，并对 byte/dense 两份 mirror 都为 `NONE` ownership 的槽跳过无效释放。span-only 阶段为 `-2.447%`，与 no-owner skip 合并后 `314,490,481 -> 296,648,172 Ir`（`-5.673%`）；drop 函数 exclusive 降 `51.52%`，owner helper 几乎归零但所有非 `NONE` 路径保持原逻辑。四配置 13 项矩阵通过，frame 更新为 `29/29`。
- Interpreter direct-parameter copy follow-up：strict 参数摘要分支先预检完整 frame，并在 call window 源已等于 dense mirror 时只复制 byte mirror。独立参数源仍同步两份视图，`ZrCore_Value_Copy` 保留 stale/owner 覆盖语义，frame-source/checked 路径不变。精确配对 `296,648,172 -> 282,552,302 Ir`（`-4.752%`），parameter-copy inclusive 降 `66.01%`；四配置 13 项矩阵通过，frame 更新为 `31/31`。
- Interpreter dispatch copy-probe follow-up：普通和 fast stack-copy 调用点在 strict direct VALUE-only frame 摘要与 bounds 同时命中时不再进入 inline-copy helper；摘要缺失、mixed/inline/union/越界场景仍经过原逐槽判定和 helper 内部的双重 fail-closed 回退。逐槽-only 候选 `-2.961%` 被拒绝，最终精确配对 `282,552,302 -> 273,765,184 Ir`（`-3.110%`），numeric/object 为 `-0.0060%/-0.0084%`；四配置 13 项矩阵通过，frame 更新为 `33/33`。
- Interpreter packed direct-frame follow-up：strict frame 摘要现在额外证明 dense layout 数、固定 stride byte mirror、精确 VALUE payload 和 parameter prefix；非 packed direct 槽保留逐槽 proof 但不发布摘要。precall scan、parameter copy、初始化、drop 和 callee return/copyback negative probes 复用该证明，prepared fast guard 的 storage/GC clear 合同不变。scan-only `-0.655611%` 与 packed-loops `-2.839406%` 均未独立接受；最终组合 `273,765,184 -> 255,021,394 Ir`（`-6.846667%`），numeric/object 为 `-0.012560%/+0.010445%`。四配置 13 项矩阵通过，frame 更新为 `35/35`。
- Interpreter packed drop owner-batching follow-up：strict packed teardown 的 dense mirror 改用 no-profile getter，并按四槽合并 byte/dense ownership kind；全 `NONE` 组一次跳过，owner group 与 remainder 保留逐 pair release。no-profile-only `-2.517827%` 未独立接受；最终组合 `255,021,394 -> 245,339,382 Ir`（`-3.796549%`），numeric/object 为 `+0.020328%/-0.015146%`。四配置 13 项矩阵通过，frame 更新为 `36/36`。
- Interpreter packed prepared-precall follow-up：仅对 debug-off、exact args/window、strict packed summary、现有完整栈容量、有界 entry clear 与可复用 call-info 的 steady state 专化；其他路径继续原 precall。padding clear 已初始化全部 byte mirror，参数用标准 ownership-aware copy 从 dense prefix 写入 mirror。前两阶段 `-1.611%/-2.609%` 均未独立接受，最终 `245,339,382 -> 236,125,782 Ir`（`-3.755451%`），numeric/object 为 `-0.003641%/+0.013457%`；四配置 13 项矩阵通过，precall 更新为 `18/18`，累计 mixed 降幅 `72.823511%`。
- M2 Task 4：native string builder 安全切片已实现，focused MSVC regression 4/4；当前没有公开 ZR builder binding，因此 `string_build/zr` 尚未切换，70/20 性能门待后续接线后验证。
- M2 Task 5：plain-layout fail-closed GC scan skip 已实现；Clang ASan Debug focused 3/3、完整 GC 67/67 且 LeakSanitizer 无报告，`RELEASED` shutdown 清扫泄漏已修复。GC throughput、baseline overhead 和 p99 pause 门禁仍待验证。
- M2 Task 3-5 的 JIT 决策门保持关闭。当前 GCC Release native benchmark runner 可正确返回 `array_index_dense` core checksum `723012102`，但 standalone scale-1 native-only 采样 CV 为 0.46-0.95、gate 不可用，因此没有性能提升结论。
- AOT Task 1：BLOCKED。当前 CLI 只支持 `interp`/`binary`，registry/suite 没有 `zr_aot_c` 分支，生成的 AOT C 也没有独立 runner 或运行时 `native_percent`/`deopt_count` 统计；先恢复 archive runner 与报告合同，再纳入默认 registry。

**Acceptance commands:**

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_frame_slot_layout_lookup_test -j2
ctest --test-dir build/codex-wsl-gcc-debug -R '^frame_slot_layout_lookup$' --output-on-failure
```

**Recorded acceptance:** core binary numeric 35.75% 提升，dispatch 51.02% 提升，Callgrind instruction reads 减少 37.06%。

## M1：可置信 steady-state 基准

1. 执行 [01-measurement-and-gates.md](01-measurement-and-gates.md) Task 1-4。
2. 补齐 numeric/dispatch 的 ZR/Lua/QuickJS/.NET persistent runners。
3. 运行：

```bash
export ZR_VM_TEST_TIER=core
export ZR_VM_PERF_SCOPE=steady
export ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops
export ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_binary,lua,qjs,dotnet
ctest --test-dir build/benchmark-gcc-release -R '^performance_report$' --output-on-failure
```

4. 只有全部 case CV 小于 5%、checksum 一致、environment fingerprint 完整时进入 M2。

## M2：frame place 与 typed scalar

1. 执行 [02-interpreter-hot-path.md](02-interpreter-hot-path.md) Task 1。
2. 用 scale-1 numeric Callgrind 验证 `ZrCore_Stack_MakeFramePlace < 5%`。
3. 执行 typed scalar lane Task 2，先支持 signed integer，再逐类扩展 unsigned/f64/bool。
4. 每个提交运行：

```bash
ctest --test-dir build/codex-wsl-gcc-debug -R 'frame_slot|inline_frame|numeric_fast|stack_relocation|precall|postcall|tail_reuse' --output-on-failure
ctest --test-dir build/codex-wsl-gcc-debug -R 'gc|ownership|weak|closure|exception' --output-on-failure
```

5. Release benchmark：

```bash
export ZR_VM_TEST_TIER=stress
export ZR_VM_PERF_SCOPE=steady
export ZR_VM_PERF_ONLY_CASES=numeric_loops,branch_jump_dense,dispatch_loops,call_chain_polymorphic
export ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,lua,qjs,dotnet
ctest --test-dir build/benchmark-gcc-release -R '^performance_report$' --output-on-failure
```

**Exit gate:** interpreter geomean `ZR/Lua <= 3.0`，无正确性/内存回归；未达到则继续 Task 3/4，不进入容器微调。

## M3：对象、数组、字符串与 GC

1. 先完成 [03-memory-object-gc.md](03-memory-object-gc.md) Task 1 指标。
2. 按 profile 排名选择数组、对象或字符串；不得同时修改三个表示层。
3. 每个表示层完成后运行对应 core tests、GC tests、ASan 和 Valgrind。
4. 运行：

```bash
export ZR_VM_TEST_TIER=stress
export ZR_VM_PERF_SCOPE=steady
export ZR_VM_PERF_ONLY_CASES=array_index_dense,object_field_hot,string_build,map_object_access,gc_fragment_baseline,gc_fragment_stress
export ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_binary,lua,qjs,dotnet
cmake --build build/benchmark-gcc-release --target run_performance_suite -j2
```

**Exit gate:** interpreter 完整代表集 geomean `ZR/Lua <= 2.0`，单 case `<= 5.0`，RSS 无未说明的 5% 以上增长。

## M4：AOT C parity

1. 执行 [04-aot-jit-parity.md](04-aot-jit-parity.md) Task 1，将 `zr_aot_c` 作为独立 implementation。
2. 从 numeric/control 开始补 native coverage，再推进 call/object；每步输出 `native_percent` 和 `deopt_count`。
3. 运行：

```bash
export ZR_VM_TEST_TIER=stress
export ZR_VM_PERF_SCOPE=steady
export ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_aot_c,lua,qjs,dotnet
ctest --test-dir build/benchmark-gcc-release -R '^performance_report$' --output-on-failure
```

**Exit gate:** native coverage 大于 90%，AOT geomean `ZR/Lua <= 1.25`、`ZR/QuickJS <= 1.25`、`ZR/.NET <= 1.5`。

## M5：JIT 决策

M4 达不到目标时，先按 case 归因。如果差距仍由解释器 fallback、容器或 GC 主导，返回对应里程碑；只有满足 [04-aot-jit-parity.md](04-aot-jit-parity.md) 的四项 JIT 决策门时创建独立设计计划。JIT 不是本路线图的默认已批准实现。

## 每个优化提交的固定流程

1. 保存 before JSON、commit、环境指纹和 Callgrind/profile 证据。
2. 写一个因缺失优化而失败的测试并记录 RED。
3. 实现最小改动，运行目标测试和受影响模块回归。
4. 运行同 scope before/after benchmark；收益小于 3% 或 CI 跨 0 时回退。
5. 运行 GCC Debug/Release；涉及内存、GC、所有权或 native 时追加 ASan/Valgrind。
6. 更新 `docs/core-runtime/` 或对应模块文档、验收文件和本计划状态。

## 最终验收输出

- `benchmark_report.json/md`：cold-start 与 steady-state 分开。
- `comparison_report.json/md`：只比较相同 scope/environment/algorithm contract。
- `instruction_report.json/md`：bytecode/helper/slowpath/deopt/native coverage。
- `hotspot_report.json/md`：Callgrind 及可用时的硬件 counter。
- `gc_overhead_report.json/md`：allocation、pause、survivor、scan/rewrite。
- 一份按 case 列出的 before/after、置信区间、RSS、正确性和回退决定表。
