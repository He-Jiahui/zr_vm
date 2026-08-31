---
related_code:
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/performance/perf_runner.c
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/function.c
  - tests/core/test_frame_slot_layout_lookup.c
plan_sources:
  - user: 2026-08-29 VM performance audit and mainstream-runtime optimization
tests:
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/acceptance/2026-08-29-frame-slot-layout-dense-lookup.md
  - tests/benchmarks/cases/numeric_loops/zr/benchmark_numeric_loops.zrp
  - tests/benchmarks/cases/dispatch_loops/zr/benchmark_dispatch_loops.zrp
doc_type: milestone-detail
---

# VM 性能审查与基线

> **Goal:** 固化 2026-08-29 审查的可复现实证，使后续优化从同一边界和同一结果起步。
>
> **Architecture:** 端到端进程基准用于启动/加载体验，长工作负载用于降低启动占比，指令计数与 Callgrind 用于归因；三类结果不相互替代。
>
> **Tech Stack:** AMD Ryzen 7 5800H、Ubuntu 22.04 on WSL、GCC 11.4、Valgrind 3.18.1、Lua 5.4.4、QuickJS 2025-09-13、.NET SDK 8.0.407。

## 构建与环境

- CPU：AMD Ryzen 7 5800H，8 核 16 线程。
- ZR：GCC 11.4 Release，`-O3 -DNDEBUG -mtune=generic -march=x86-64`，共享库，binary 运行前在计时区外生成 `.zro`。
- 亲和性：所有记录到本计划的子进程使用 `taskset -c 2`。
- 参考运行时：Lua 5.4.4、QuickJS 2025-09-13、.NET 8.0.407。
- 诊断工具：Valgrind/Callgrind 3.18.1，counting mode 关闭 cache/branch simulation。
- `perf` 在当前 WSL 中未安装，因此本轮没有硬件分支、cache miss 或 IPC 数据；计划不得伪造这些结论。

GCC x64 编译探针确认：`sizeof(SZrTypeValue) == 48`、`sizeof(SZrTypeValueOnStack) == 64`、`sizeof(TZrInstruction) == 8`。

## 测量边界

现有 `perf_runner` 的每次 warmup 和 measured iteration 都创建新子进程。它意味着：

- ZR interp 包含 CLI、项目加载、解析/编译、运行时建立和执行；
- ZR binary 排除一次性 `--compile`，但包含 CLI、`.zro` 加载、运行时建立和执行；
- Lua/QuickJS/.NET 包含各自进程启动；
- .NET 的 warmup 进程退出后，JIT 状态不会传递到 measured 进程。

因此 core 结果是“进程级端到端”，不是稳态 VM 吞吐。stress 工作量降低了启动占比，但仍没有同进程多次调用的 JIT 稳态合同。

## 基线结果

### 数值循环

| 模式 | 档位 | 样本 | 中位数 | 说明 |
|---|---|---:|---:|---|
| ZR interp，修复前 | core | 3 | 3,729.668 ms | 包含源码加载/编译 |
| ZR binary，修复前 | core | 5 | 2,941.107 ms | 编译在计时外 |
| ZR binary，稠密查找修复后 | core | 5 | 1,889.751 ms | 比修复前快 35.75% |
| Lua | core | 7 | 858.494 ms | 进程级，方差较高 |
| QuickJS | core | 7 | 625.170 ms | 进程级 |
| .NET | core | 7 | 2,014.411 ms | snap 进程冷启动/JIT，不能代表稳态 |
| ZR interp，修复前 | stress | 5 | 41,454.358 ms | 源码模式 |
| ZR binary，稠密查找修复后 | stress | 3 | 30,591.891 ms | 当前长期执行基线 |
| Lua | stress | 5 | 3,129.862 ms | ZR 当前为 9.77 倍 |
| QuickJS | stress | 5 | 4,665.153 ms | ZR 当前为 6.56 倍 |
| .NET | stress | 5 | 1,579.317 ms | ZR 当前为 19.37 倍 |

### 调度循环

| 模式 | 档位 | 样本 | 中位数 | 与修复后 ZR 的关系 |
|---|---|---:|---:|---:|
| ZR interp，修复前 | core | 3 | 17,534.026 ms | 源码加载不是主要成本 |
| ZR binary，修复前 | core | 5 | 17,963.016 ms | 基线 |
| ZR binary，稠密查找修复后 | core | 3 | 8,798.310 ms | 提升 51.02% |
| Lua | core | 5 | 791.566 ms | ZR 为 11.11 倍 |
| QuickJS | core | 5 | 444.775 ms | ZR 为 19.78 倍 |
| .NET | core | 5 | 2,593.807 ms | ZR 为 3.39 倍；.NET 方差很高 |

## 热点证据

修复前的 binary scale-1 `numeric_loops`：

- 字节码计数：17,313,521 条；慢路径只有个位数；
- 高频指令为 `MOD_SIGNED_CONST`、比较跳转、`JUMP` 和常量整数算术；
- Callgrind 总指令读取 774,972,233；
- `ZrCore_Function_FindFrameSlotLayout` 独占 348,952,794，比例 45.03%。

稠密索引修复后：

- Callgrind 总指令读取 487,764,858，减少 37.06%；
- 线性查找退出第一热点；
- `ZrCore_Stack_MakeFramePlace` 成为第一独占热点，127,606,726，比例 26.16%。

结论是帧布局/地址解析，而非慢路径 miss 或源码编译，是当前数值和调度执行的首要结构成本。

## 已实现而不应重复规划的能力

- GCC/Clang/Linux 下已有 computed-goto 快速分派表。
- 已有大量 signed arithmetic、plain destination、load-stack 和 fused 指令。
- 已有指令、helper、slowpath、quickening probe 计数。
- 已有调用点缓存和多种 known-call 快路径。
- 整数数组已有 `superArrayRawIntData` 原始侧存储。
- CLI 已能发出 AOT C；`zr_vm_aot/` 保留历史 AOT 工具链。

后续计划必须针对这些机制的剩余成本做剖析，不得把“增加 computed goto”“增加整数数组”或“增加 profiler”列成缺失功能。

## 基线限制

1. core 默认只测 1 次，历史报告中的 stddev 没有统计意义。
2. 本轮部分短 C/Lua/.NET 进程样本方差超过 10%，不能作为 3% 级优化门禁。
3. 固定实现顺序可能受热状态、后台负载和 CPU 频率漂移影响。
4. 算法 checksum 相同不自动证明容器表示、字符串语义和内存策略等价。
5. 当前 suite 没有 LuaJIT，也没有 CoreCLR 同进程稳态结果。

这些限制由 [01-measurement-and-gates.md](01-measurement-and-gates.md) 先解决；在此之前只接受大于 10% 且被 Callgrind/指令计数同时解释的变化。
