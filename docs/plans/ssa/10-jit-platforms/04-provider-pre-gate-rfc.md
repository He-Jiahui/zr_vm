---
related_code:
  - zr_vm_jit/include/zr_vm_jit/backend.h
  - zr_vm_jit/src/orc_backend.cpp
  - zr_vm_jit/CMakeLists.txt
  - zr_vm_core/include/zr_vm_core/host_baseline_jit.h
implementation_files: []
plan_sources:
  - docs/plans/benchmark/optimize/04-aot-jit-parity.md
  - docs/plans/ssa/10-jit-platforms/02-host-baseline-jit.md
  - "user: 2026-11-22 plan-jit-perf-gc-bridge r2, unit JIT-R1"
  - "user: 2026-11-23 t727u 「拍板交给opus，他有能力拍板」 — Q-J1 / Q-J3 decisions in the JGB decision register"
tests:
  - tests/core/test_ssa_host_baseline_jit.c
  - tests/core/test_ssa_host_jit_optional.c
doc_type: plan-record
status: proposed
---

# JIT-R1: machine-code provider pre-gate RFC (non-authorizing)

> **Non-authorizing. The Task 4 JIT decision gate is CLOSED.** This page does not authorize any provider code, ORC/JITLink linking, executable-memory allocation, or setting `machineCodeAvailable = true`. It turns the gate into a checklist and collects what a future provider must satisfy. It is not a 10.0x leaf: it has no requirement ID in `tests/core/test_ssa_release_acceptance.c`, it is not listed as a milestone in the [SSA index](../index.md), and it changes no leaf `status:`.
>
> 摘要：本文**不授权**实现。Task 4 JIT 决策门仍关闭。本文把门禁条件整理成检查表，并收集未来机器码 provider 必须满足的义务、留待 provider 决定的 facade 问题，以及后端比较维度（不选定任何后端）。本文不是 10.0x 叶子计划，不改变任何叶子的 `status:`。

## 1. Decision recorded (Q-J1)

- **Q-J1:** open a machine-code provider (10.02 batches 2–3) now, or keep the gate closed?
- **Answer:** keep the Task 4 gate closed for implementation. This RFC is the only authorized artifact (docs only).
- **Why:** [Task 4](../../benchmark/optimize/04-aot-jit-parity.md) allows a JIT implementation plan only when all four conditions hold and says "不得先手写机器码 emitter 再补 GC/deopt 合同". None of the conditions is measured today (§2).
- **Rejected:** open batches 2–3 now (contradicts Task 4 and the JIT-D1 status banner on [10.02](02-host-baseline-jit.md)); keep everything closed including this RFC (the deferred facade items and the ABI-18 obligation would have no durable home); a backend-selection RFC that picks LLVM ORC now (backend choice is exactly what Task 4 defers until after the gate).

## 2. Gate checklist (Task 4 preconditions)

All four must be true, with evidence linked, before a JIT implementation plan may be written. Today every box is open.

| # | condition (Task 4) | how it is measured | current state |
|---|---|---|---|
| G1 | Persistent steady-state protocol in place; result CV < 5% | PERF-M0 pinned-host measurement per [01 measurement and gates](../../benchmark/optimize/01-measurement-and-gates.md) | **open** — PERF-M0 is blocked on host capacity (Q-P3), not on a decision |
| G2 | AOT native coverage > 90%, yet the interactive / highly dynamic representative set is still slower than Lua ×1.5 or .NET ×2 | AOT phase native coverage (`nativeSites / executedSemanticSites`, see `tests/acceptance/2026-10-02-ssa-aot-phase-coverage-ratio.md`) over the benchmark registry ([04 Task 2](../../benchmark/optimize/04-aot-jit-parity.md)), plus parity ratios | **open** — no published coverage numbers for the representative set |
| G3 | The gap comes from runtime type/call specialization, not container algorithms, GC or FFI | profile attribution on the G2 cases | **open** — depends on G1/G2 data |
| G4 | Designs exist for serializable SemIR, precise stack maps, deopt frame reconstruction, and code lifetime / GC handshake | design documents reviewed and linked here | **partial** — the core code manager (records, publish, retire, leases) and the facade's state-map hash checks exist as contracts ([10.02](02-host-baseline-jit.md) status banner); no stack-map/deopt reconstruction design exists |

Ticking a box requires a link to the evidence (run artifact, report, or design doc) in a PR that edits this table. Ticking all four still does not open the gate by itself; it permits writing the JIT implementation plan that Task 4 describes.

## 3. Obligations any future provider must meet

These are requirements, not designs. A provider PR that misses one is out of contract.

1. **ABI 18 optional-receiver call kinds.** Callsite-cache kinds 12 (`DYN_MEMBER_CALL`), 13 (`DYN_MEMBER_TAIL_CALL`) and 14 must either be implemented or deopt to the VM with the staged window intact ([runtime optional receiver ABI](../../../core-runtime/runtime-optional-receiver-abi.md), lines 61–62: "Any future provider must implement these kinds or deopt to that VM with the staged window intact. A facade contract test is not machine-code execution proof."). (Recommendation for Q-J2, which is not decided here: adopt this as the formal checklist item.)
2. **Publication order.** Stack maps, unwind and debug data are registered before an entry is published; retirement first withdraws the entry, waits for leases, then unregisters and frees ([10.02](02-host-baseline-jit.md) task 3; `zr_vm_core/include/zr_vm_core/host_baseline_jit.h` Prepare / Publish / AcquireActive / Retire).
3. **Restricted imports.** The symbol resolver allows only manifest runtime/native symbols; it never searches arbitrary host addresses ([10.02](02-host-baseline-jit.md) task 2).
4. **Target identity.** Every facade entry that carries or implies a target must match the registered `impl.options.target`, checked in the fixed order ABI → triple → layout (Q-J3 decisions J3c/J3g; implemented for Compile and `query_target`).
5. **No address in artifacts.** A JIT address never enters an artifact or patch payload ([10.02](02-host-baseline-jit.md) algorithm sketch).
6. **Honest capability reporting.** `machineCodeAvailable` stays false until a provider actually links and runs machine code under CI; finding an LLVM package is not a capability (`zr_vm_jit/CMakeLists.txt` comment on `ZR_VM_JIT_HAS_ORC`).

## 4. Facade questions deferred to the provider (Q-J3 residue)

Q-J3 split the facade TODOs. Items J3a/J3b/J3c/J3g are decided and assigned to JIT-U3. The items below depend on how a real provider publishes and falls back, so they stay open here. Anchors are at main `b3782a2c8`.

| item | fact | question for the provider design |
|---|---|---|
| **J3d** map-hash checks differ | The request validator compares only `frameLayoutHash` with `publication.layoutHash` (`orc_backend.cpp:1244-1250`); `prepare_locked` compares the frame layout and all four map hashes (`orc_backend.cpp:1011-1015`). `backend.h:180-182` and `:338-339` carry the matching TODOs. | Must the fallback / compile-request path reject a root, unwind, debug or deopt map hash that differs from publication, or is the frame-layout check sufficient before a provider exists? Decide together with the provider's fallback request contract. |
| **J3e** convenience `ZrJit_Host_Prepare` | Declared at `backend.h:545`, defined at `orc_backend.cpp:1465`; no in-repo caller, including tests (only the code-review coverage rows mention it). | Keep as a contract/test convenience, or remove when the provider publication path lands? (No removal before then: it would be a public API break with no consumer benefit.) |
| **J3f** output-slot overwrite | Facade outputs are cleared or overwritten without checking whether the slot holds a live lease (`orc_backend.cpp:1471`, `:1498`, `:1561`, `:1650`). The core contract forbids reuse (`host_baseline_jit.h`, Prepare / AcquireActive notes). | Should the provider-facing facade add a defensive check (and what status would it return), or keep the documented precondition? |
| **J3h** `ZR_VM_JIT_HAS_ORC` | Set by `zr_vm_jit/CMakeLists.txt:17-23`, passed as a compile definition (`:40`), read by no source. | Wire it into capability reporting with the provider, or delete it if the provider is not ORC. |

## 5. Backend comparison axes (no backend chosen)

When the gate opens, the provider RFC must compare candidate backends on at least these axes (Task 4 list, plus items specific to this codebase). This RFC deliberately does **not** rank or pick a backend.

| axis | what to establish |
|---|---|
| Integration cost | C++ isolation inside `zr_vm_jit/` only; core stays C11; build-time and binary-size cost when the option is OFF must stay zero |
| Platform support | host x86-64 / AArch64 only; Android / iOS / WASM must refuse machine-code JIT at configuration ([10.02](02-host-baseline-jit.md) task 1, [10.03](03-platform-matrix.md)) |
| W^X | executable-memory protocol per platform; no RWX pages |
| Unwind / debug registration | registration and deregistration order matching obligation 2; exceptions crossing JIT frames |
| Stack maps / deopt | precise roots at safepoints; frame reconstruction into the interpreter with the staged window (obligation 1) |
| Code lifetime / GC handshake | interaction with the core code manager's leases and retirement |
| Code cache | cache key (signature, layout, ABI hashes), invalidation, and the rule that addresses never enter artifacts |
| Compile latency | baseline-tier latency measured, not assumed (10.02 notes ORC does not guarantee baseline latency) |
| Version pinning and security updates | how a backend version is pinned and updated without breaking the ABI 18 contract |
| Licensing | compatibility with this repository's license |

## 6. What would change this page

- A PR that ticks a G1–G4 box with linked evidence.
- A recorded decision on Q-J2 (ABI-18 kinds checklist) or on any §4 item.
- Opening the gate requires all of §2 plus a new, separately reviewed JIT implementation plan; editing this page alone never opens it.

## 中文摘要

- **Q-J1 决定：** Task 4 JIT 决策门保持关闭；本 RFC 是唯一被授权的产物（仅文档），不授权任何 provider 代码、ORC/JITLink 链接或 `machineCodeAvailable = true`。
- **门禁检查表：** G1 CV<5%（依赖 PERF-M0，受主机容量阻塞）、G2 AOT native coverage>90% 且动态代表集仍慢、G3 差距来自类型/调用专化、G4 stack map/deopt/code lifetime 设计——目前全部未满足（G4 部分具备）。
- **provider 义务：** ABI 18 kinds 12–14 必须实现或带 staged window 回退到 VM；先注册图再发布入口；受限符号解析；target 一致（ABI → triple → layout）；地址不进入产物；能力如实报告。
- **留给 provider 的 facade 问题：** J3d（map hash 校验差异）、J3e（便捷 Prepare 去留）、J3f（输出槽覆盖检查）、J3h（`ZR_VM_JIT_HAS_ORC` 未被读取）。
- **后端比较维度：** 集成成本、平台、W^X、unwind/debug、stack map/deopt、code lifetime/GC、代码缓存、编译延迟、版本固定与安全更新、许可——本文**不选择**后端。
