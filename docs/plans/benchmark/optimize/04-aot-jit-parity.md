---
related_code:
  - zr_vm_cli/src/zr_vm_cli
  - zr_vm_aot
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot
  - tests/cmake/run_performance_suite.cmake
  - tests/benchmarks/registry.cmake
  - tests/benchmarks/dotnet_runner/Program.cs
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot
  - tests/cmake/run_performance_suite.cmake
  - tests/benchmarks/registry.cmake
plan_sources:
  - user: 2026-08-29 pursue Lua/C# VM performance
  - docs/plans/benchmark/optimize/01-measurement-and-gates.md
  - docs/plans/benchmark/optimize/02-interpreter-hot-path.md
tests:
  - tests/parser/test_aot_c_typed_scalar.c
  - tests/parser/test_aot_c_source_contracts.c
  - tests/parser/test_aot_c_call_contracts.c
  - tests/benchmarks/registry.cmake
doc_type: milestone-detail
---

# AOT、JIT 与主流运行时对齐计划

> **当前状态（2026-08-30）：AOT Task 1 BLOCKED。** 主 CLI 仍只接受
> `interp`/`binary`，benchmark registry 与 suite 没有 `zr_aot_c` 实现分支；现有
> AOT C 只在 parser smoke harness 中生成并通过 `AotRuntime_ExecuteEntry` 执行，
> 没有独立 benchmark runner，也没有运行时 `native_percent`/`deopt_count` 统计。
> 在补齐 runner、measurement contract、checksum 和覆盖率 ABI 前，不将 AOT 加入默认
> registry，也不宣称 AOT parity。

> **Goal:** 建立一个经过语义 guard/deopt 验证的优化执行层，在稳态代表集上接近 Lua/QuickJS，并把与 CoreCLR 的差距压到 1.5 倍以内。
>
> **Architecture:** 先复用现有 typed SemIR 和 AOT C 后端形成可测优化层；解释器作为动态语义和 deopt 回退。仅当 AOT 已消除解释器成本而交互式/动态负载仍不达门禁时，才批准 baseline JIT。
>
> **Tech Stack:** ZR SemIR、现有 AOT C emitter、系统 C 编译器、metadata token/layout guards、可选 LLVM/成熟 JIT backend 的后续 RFC、.NET 8 steady-state 对照。

## 为什么字节码模式不是 AOT

当前 benchmark 的 `zr_binary` 只把源码编译成 `.zro` 并在计时外完成这一步；计时阶段仍由同一解释器执行字节码。因此它适合分离解析成本，但不能代表本机码性能。CoreCLR 则会在进程内 JIT 并进行分层优化，用新进程样本比较两者会同时低估 .NET 稳态、误标 ZR 的执行层级。

## Task 1：把现有 AOT C 纳入 benchmark registry

**Files:**

- Modify: `tests/benchmarks/registry.cmake`
- Modify: `tests/cmake/run_performance_suite.cmake`
- Add: `tests/benchmarks/aot_runner/CMakeLists.txt`
- Add: `tests/benchmarks/aot_runner/main.c`
- Modify: `tests/benchmarks/README.md`
- Test: `tests/benchmarks/test_benchmark_registry.c`

**Steps:**

1. 注册 `zr_aot_c` implementation，`prepare` 阶段执行 `zr_vm_cli --compile <project> --emit-aot-c` 并用固定 Release flags 编译 native runner。
2. 报告分别记录 ZR compile、C compile/link 和 run wall time；steady-state 比值只使用 run。
3. AOT runner 输出与解释器相同 banner/checksum，并加载同一 module metadata、stdlib 和 runtime helpers。
4. 从 numeric、branch、array、object、call、string、GC 各选择至少一个 case；不支持的 case 明确 `UNSUPPORTED_SEMIR`，不得静默回解释器后仍标为 AOT。
5. 验证生成代码中每个 dynamic fallback 都计数，报告 `native_percent` 与 `deopt_count`。

## Task 2：完成 typed scalar/control/call AOT 覆盖

**Files:**

- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_semir.c`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_binary.c`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c`
- Test: `tests/parser/test_aot_c_typed_scalar.c`
- Test: `tests/parser/test_aot_c_call_contracts.c`
- Add: `tests/parser/test_aot_c_benchmark_native_coverage.c`

**Steps:**

1. 对 benchmark trace 中前 95% typed scalar/control opcode 建立 native lowering coverage table。
2. 补齐循环、比较分支、mod/div、typed local、known direct call 和 single return；每个 lowering 先有生成 C contract 和 shared-library smoke test。
3. dynamic value、meta、reflection、exception 和 unsupported generic 通过统一 deopt bridge 回解释器，保留 program counter、frame state 和 ownership。
4. `native_percent < 90%` 的 case 不参与 AOT parity 几何平均，只作为覆盖缺口报告。
5. 接受条件：numeric/branch AOT 不慢于 C baseline 2 倍，checksum 与异常/所有权测试一致。

## Task 3：profile-guided specialization

**Files:**

- Modify: `zr_vm_core/src/zr_vm_core/profile.c`
- Add: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_profile_import.c`
- Add: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_profile_import.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_runtime_fallback.c`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c`
- Add: `tests/parser/test_aot_profile_guided_specialization.c`

**Steps:**

1. 输出稳定的 call target、shape、value type、branch bias 和 deopt site profile，包含 module/version/signature hash。
2. AOT 编译只导入 hash 完全匹配的 profile；不匹配直接忽略。
3. 单态 call/shape/type 生成 guard 后的 native fast path；guard 失败统一 deopt，不复制动态语义。
4. profile 输入不得改变 checksum、异常顺序、GC root 或所有权释放时机。
5. 接受条件：object/dispatch/mixed-service AOT 各提升至少 15%，deopt storm case 无指数退化。

## Task 4：JIT 决策门

只有同时满足以下条件才创建 JIT 实现计划：

1. persistent steady-state 协议已落地，结果 CV 小于 5%。
2. AOT native coverage 大于 90%，但交互式或高动态代表集仍慢于 Lua 1.5 倍或 .NET 2 倍。
3. 差距来自运行时类型/调用专化，而不是容器算法、GC 或 FFI。
4. 已有可序列化 SemIR、精确 stack map、deopt frame reconstruction 和 code lifetime/GC handshake 设计。

满足后，RFC 必须比较成熟 backend 的集成成本、平台支持、代码缓存、W^X、安全更新、debug mapping、异常 unwind 和许可。不得先手写机器码 emitter 再补 GC/deopt 合同。

## Parity 验收

- 解释器：代表集几何平均 `ZR/Lua <= 2.0`，worst case `<= 5.0`。
- AOT/优化层：`ZR/Lua <= 1.25`、`ZR/QuickJS <= 1.25`、`ZR/.NET <= 1.5`。
- AOT compile time 与产物大小单列；不得通过把编译时间隐藏到启动前来宣称端到端领先。
- CoreCLR 使用同一进程 warmup 后的稳定 tier；同时报告 cold-start 作为部署体验，不混合排名。
