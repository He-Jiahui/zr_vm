---
related_code:
  - tests/benchmarks/registry.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/performance/perf_runner.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/include/zr_vm_core/value.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/function.c
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/CMakeLists.txt
plan_sources:
  - user: 2026-08-29 audit the VM, benchmark it, optimize it, and pursue Lua/C# performance
  - docs/benchmarks/index.md
  - docs/superpowers/plans/2026-04-14-steady-state-performance-convergence.md
tests:
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/acceptance/2026-08-29-frame-slot-layout-dense-lookup.md
  - tests/benchmarks/registry.cmake
doc_type: milestone-detail
---

# VM 性能审查与优化计划

> **Goal:** 用可复现证据消除当前 VM 的主要性能缺陷，并把解释器、AOT/优化执行层分别推进到可与 Lua、QuickJS 和 .NET 稳态结果比较的水平。
>
> **Architecture:** 先修正测量边界，再按 Callgrind/指令计数排序优化帧访问、值表示、调用、容器和 GC；解释器承担低延迟与动态语义，AOT/JIT 承担接近 C# 优化运行时的长期吞吐。
>
> **Tech Stack:** C11、CMake/Ninja、GCC/Clang、WSL、Unity、Callgrind、ZR benchmark runner、Lua 5.4、QuickJS、.NET 8。

## 审查结论

当前实现不是“没有做解释器优化”。已经存在 computed-goto 分派、quickened/fused 指令、调用点缓存、整数数组原始侧存储、指令/辅助函数/慢路径计数和 Callgrind 报告。继续重复这些项目不会产生主要收益。

本轮实测发现的首要缺陷是帧槽布局查找：每个逻辑槽访问都在线性扫描 `frameSlotLayouts`。该问题已按测试先行方式修复，`numeric_loops` 字节码中位数提升 35.75%，`dispatch_loops` 提升 51.02%，Callgrind 指令读取减少 37.06%。修复后第一热点转移到 `ZrCore_Stack_MakeFramePlace`，占最小数值样本指令读取的 26.16%。

修复并不等于达到目标。当前压力档数值循环的 ZR 字节码中位数仍为 30,591.891 ms，分别是 Lua 的 9.77 倍、QuickJS 的 6.56 倍、.NET 的 19.37 倍。解释器还存在帧地址重复验证、48 字节值复制、64 字节栈槽、热代码体积、对象/容器双表示等结构成本；若要接近 CoreCLR 稳态吞吐，还需要把现有 AOT C 路径纳入正式基准，并在证据不足时再决定是否建设 JIT。

## 问题优先级

| 优先级 | 问题 | 当前证据 | 处置 |
|---|---|---|---|
| P0 已完成 | 稠密 `frameSlotLayouts` 每次线性扫描 | 修复前独占 45.03% Callgrind 指令读取 | O(1) 自校验命中，保留稀疏回退 |
| P0 | 基准混合冷启动、编译、JIT 和执行成本 | 每个预热/样本都新建进程；core 默认仅 1 次测量 | 分离 cold-start 与 steady-state 协议 |
| P0 | 每次值槽访问重复创建并验证 frame place | 修复后 `ZrCore_Stack_MakeFramePlace` 独占 26.16% | 帧建立时验证，热路径直接计算地址 |
| P1 | `SZrTypeValue` 为 48 字节，栈槽为 64 字节 | GCC x64 编译探针 | typed scalar lane，按边界物化完整值 |
| P1 | 调度文件 9,500 行，快速路径与大量语义分支共存 | `execution_dispatch.c` 静态审查 | 按指令族拆分，控制热代码体积并以 I-cache 数据验收 |
| P1 | Lua/.NET 覆盖缺少 6 个新热路径 case，GC case 仅有 C/ZR | benchmark registry | 补齐同算法实现与表示等价审计 |
| P1 | 原始整数数组与通用 node map 双表示存在同步/物化成本 | `object_super_array_internal.h` | 明确 canonical storage 和边界物化 |
| P2 | “ZR binary”只是预编译字节码加载，不是本机码 | benchmark harness 与 CLI 模式 | 把现有 AOT C 作为独立实现加入报告 |
| P2 | 解释器无法单靠微优化匹配 CoreCLR 优化代码 | 数值压力档差距 19.37 倍 | AOT 优先；达不到门禁时启动 baseline JIT RFC |

## 计划导航

- [00-audit-and-baseline.md](00-audit-and-baseline.md)：环境、口径、原始结果、已落地优化与残余差距。
- [01-measurement-and-gates.md](01-measurement-and-gates.md)：冷启动/稳态分离、统计协议、等价性和 CI 门禁。
- [02-interpreter-hot-path.md](02-interpreter-hot-path.md)：frame place、值表示、分派、调用与安全点计划。
- [03-memory-object-gc.md](03-memory-object-gc.md)：数组、对象、字符串、分配和 GC 计划。
- [04-aot-jit-parity.md](04-aot-jit-parity.md)：AOT C 正式基准、deopt 合同和 JIT 决策门。
- [05-execution-roadmap.md](05-execution-roadmap.md)：按任务拆分的实施顺序、命令和验收条件。

## 统一完成标准

1. 所有性能声明同时给出正确性 checksum、环境指纹、中位数、样本数、离散度和原始 JSON。
2. 冷启动和稳态报告分开；任何 .NET 对比不得用新进程预热冒充 JIT 稳态。
3. 每个优化先有可失败的单元/性能测试，再修改生产代码；收益低于 3% 或 95% 置信区间跨过 0 时回退该优化。
4. 解释器阶段目标：完整代表集几何平均不慢于 Lua 2 倍，单 case 不慢于 5 倍。
5. 优化执行层阶段目标：几何平均处于 Lua 的 1.25 倍内、QuickJS 的 1.25 倍内、.NET 稳态的 1.5 倍内，且没有 case 慢于对应运行时 3 倍。
6. 性能门禁之外，WSL GCC Debug/Release 核心回归、ASan/Valgrind 代表集和 GC/所有权/异常/闭包/栈重定位测试必须通过。

