---
related_code:
  - zr_vm_core/include/zr_vm_core/host_baseline_jit.h
  - zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
implementation_files:
  - zr_vm_core/include/zr_vm_core/host_baseline_jit.h
  - zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c
plan_sources:
  - docs/plans/ssa/10-jit-platforms/02-host-baseline-jit.md
  - docs/plans/ssa/10-jit-platforms/01-backend-service.md
  - "user: 2026-09-13 实现 10.02 host baseline/JIT contract leaf"
tests:
  - tests/core/test_ssa_host_baseline_jit.c
doc_type: module-detail
status: implemented-subset
---

# Host Baseline JIT Contract

## Purpose

`host_baseline_jit` 为可选的 x86-64/AArch64 machine-code backend 提供 core 层 C ABI。它冻结目标 ABI、允许导入、可执行代码发布前的安全登记和 code-cache lease 生命周期；没有 LLVM/ORC 的构建仍然可以使用 AOT/ExecBC。该模块不生成机器码，也不保存宿主地址，因此可以先独立验证边界，再由后续 C++ ORC/JITLink 适配层消费。

## Contract layers

`SZrHostJitTargetContract` 是运行时 target witness：schema、架构、host 平台、指针宽度、端序、execution ABI 版本、target triple hash 和 layout hash 必须一致。当前 contract 只接受 host x86-64 与 AArch64、8-byte little-endian target；Android/iOS/WASM 会被明确标记为 unsupported，而不会创建 machine-code 路径。

`SZrHostJitImportManifest` 由稳定的 symbol/signature identity 组成。resolver 必须逐项调用 `ZrCore_HostJit_ValidateImports`，只允许 manifest 内的 runtime/native symbol 及匹配签名；代码不做任意宿主地址搜索。manifest 和每个 import 都要求非零 identity、已知 kind、正确 schema 和非零 allowlist hash。

`SZrHostJitPublicationFacts` 以稳定标量承载发布前 proof；其中的 import manifest 是只读的 borrowed validation view，不会复制到 code record。它必须声明 typed scalar、control、direct call、simple member 或 array 中至少一个受支持操作，且 signature/layout/ABI、GC map、unwind、debug、deopt map 和 code identity 都非零。machine-code 与 W^X 标志必须同时存在，四种 registration（roots、unwind、debug、deopt）全部完成后才能进入 published 状态。

## Lifecycle

`ZrCore_HostJit_Code_Prepare` 先完整验证 facts，再占用一个 free record；失败时不会留下可发布的部分记录。`Code_Publish` 在 manager lock 下把旧 published record 标为 retired，再发布新 record。`Code_AcquireActive` 创建 lease；`Code_Evict` 只撤销 active 入口并标记 retired，不释放仍有 lease 的 record。`Code_CollectRetired` 仅回收 lease count 为零的记录，因此 active frame 期间不会释放其 code page 的拥有者。宿主适配层应在调用本 API 前后负责 W^X 页权限和真实 unwind/debug/root 注册，登记完成后才提交 facts。

Code handle 只携带 record identity 和 lease 状态，不暴露函数指针或 executable address。`Resolve` 可以读取 semantic signature/layout/ABI witness 及状态；实际 entry address 必须留在适配层的进程内存中，不能进入 artifact、persistent CallBinding 或 cache key。

## Design rationale and scope

本实现使用 C11-compatible fixed-width fields 与 compiler intrinsic spin lock，避免 Windows GCC 4.8 因缺少 `stdatomic.h` 而无法编译；manager lock 串行化 record 状态和 lease 变化。它是生命周期和拒绝策略 contract，不宣称已完成 LLVM lowering、ORC resolver、W^X 系统调用、stack-map/unwind 注册或两架构实际机器码执行。

The contract deliberately validates all registrations before publication. A backend compile failure or unsupported operation must retain AOT/ExecBC fallback and must not install a partially initialized entry. The independent backend-service state machine, asynchronous queue, cancellation, generation key and deopt resume remain dependencies/follow-up from 10.01.

## Test coverage

`tests/core/test_ssa_host_baseline_jit.c` verifies:

- valid host target and explicit WASM rejection;
- manifest import acceptance and unlisted symbol rejection;
- W^X/publication registration completeness;
- prepare/publish/active lease/eviction/retired collection sequence;
- no collection while an active lease remains, followed by collection after release.

The fixture is intentionally core-only so it can be compiled directly without changing shared CMake or requiring an unavailable LLVM installation. The parent integration task may register it in `tests/cmake/ssa-tests.cmake` as `ssa_host_baseline_jit`.

## Open issues and follow-up

This leaf has no executable memory allocator, LLVM version pin, real ORC/JITLink symbol resolver, generated stack maps, platform unwind/debug registration, or AOTIR lowering. Those pieces require the 10.01 backend service and 07.02 C/LLVM lowering contracts. Platform smoke and unavailable records belong to 10.03. Performance acceptance must separately measure compile latency, cold start, warm throughput, cache and RSS using the 00.01 measurement contract.
