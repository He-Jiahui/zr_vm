---
related_code:
  - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h
  - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c
  - tests/core/test_ssa_platform_matrix.c
implementation_files:
  - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c
plan_sources:
  - docs/plans/ssa/10-jit-platforms/03-platform-matrix.md
tests:
  - tests/core/test_ssa_platform_matrix.c
doc_type: acceptance
status: scoped-accepted-msvc-debug-release
---

# SSA Platform Runtime Acceptance Policy

Validation date: 2026-10-02 UTC / 2026-10-03 Asia/Shanghai.
Repository baseline: `ef64478bf505b8a805f775d9023f990c8487dd3a`.

## Failure and Correction

`Check` originally validated only the runner enum.  A cross-compile observation
with compiled/executed/semantic-passed fields and a passed outcome could return
`OK`; `IsRuntimeAcceptance` could also return true.  The latter additionally
accepted Android/iOS/WASM Host JIT declarations and JIT feature requirements
when the machine-code execution bit was false.  `ZrTests_Ssa_CheckPlatform`
consumes `Check`, so the inconsistency affected the same platform fixture
contract through both public acceptance entry points.

One private helper now applies the observation policy to both entry points.
Cross-compile observations claiming execution, semantic success, or a passed
outcome return `OBSERVATION_INVALID`; normal unexecuted unavailable records
retain `RUNTIME_UNAVAILABLE`.  All three mobile/WASM JIT triggers return
`MACHINE_CODE_JIT_FORBIDDEN` and false runtime acceptance.  No public ABI or
CMake registration changed.

## Evidence

All private snapshots, compiler outputs, temporaries, probes, and logs are under
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/review/jit-platform/`.  The root-owned MSVC
environment cache was frozen and its SHA-256 remained unchanged:
`7019fe0f651257b7c76061b18eac474279e0ffb33aa7c16d9b4906b7bb764e53`.

| Validation | Build Exit | Run Exit | Result |
| --- | --- | --- | --- |
| Original implementation + explicit policy probe | 0 | 1 | 10 failed expectations |
| Fixed implementation + expanded existing platform fixture, debug | 0 | 0 | pass |
| Fixed implementation + expanded existing platform fixture, `/O2 /DNDEBUG` | 0 | 0 | pass |
| Fixed implementation + identical policy cases, debug | 0 | 0 | 0 failed expectations |
| Fixed implementation + identical policy cases, `/O2 /DNDEBUG` | 0 | 0 | 0 failed expectations |

Compiler: MSVC x64 19.44.35228, C11, `/W4 /WX /utf-8`.  Debug flags were
`/MDd /Od /Zi /RTC1`; optimized flags were `/MD /O2 /DNDEBUG`.  The explicit
probe undefines `NDEBUG` in its test translation unit, so optimized production
validation does not silently remove fixture assertions.  The repository fixture
also undefines `NDEBUG` before including `assert.h`, so all existing and new
checks stay active in optimized registered builds.  Both suites use the
actual common implementation, with the original fixture frozen for the probe.

Reproduction entry point:

```text
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/review/jit-platform/validate_policy.py
```

The probe was first compiled against the frozen original source and headers;
`red-build.log` records build exit 0, and `red-run.log` records ten failures and
run exit 1.  The validator writes exact command lines, working directories,
exit codes, and elapsed times to `compiler-version.log`, `green-debug-*.log`,
and `green-probe-{debug,ndebug}-*.log`.  `receipt.json` records SHA-256 hashes of
the baseline/fixed source and test, frozen common headers, probe, and MSVC cache.
The compiler-version query intentionally returns 2 because `/Bv` was invoked
without a source file; it is a version receipt, not a build failure.

The new fixture cases assert the precise status, false result through both
acceptance entry points, runner/target/backend diagnostic context, and retained
source/instruction location.  Existing fixture cases retain ABI, numeric,
layout, import, semantic witness, unsupported feature, and unavailable behavior.
Positive control rows include allowed execution runners, actual policy fixture
execution, and mobile/WASM ExecBC observations in the explicit probe.

## Registered Target Verification

Root rebuilt the existing `zr_vm_ssa_platform_matrix_test` target in its shared
MSVC matrix and ran the registered `ssa_platform_matrix` CTest.  Build exit was
0 (13.437 seconds).  CTest reported 1/1 passed (test 41.78 seconds, total 46.03
seconds; wrapper 48.086 seconds).  Full logs are
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/platform-policy-formal-build.log`
and `D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/platform-policy-formal-ctest.log`.
The build emitted the existing CMake `/W3` overridden by `/W4` D9025 option
warning; no source diagnostic or build failure resulted.

The scoped acceptance combines that registered MSVC Debug fixture gate with
the D-only native MSVC `/O2 /DNDEBUG` helper and complete fixture gates above.
Production source and always-active test were frozen before root verification;
this evidence update changes only acceptance documentation.

## Limits

These are value-contract fixtures executed on Windows x64.  They do not prove
Android, iOS, or WASM device/runtime execution, or actual Host JIT machine-code
generation.  Linux toolchain/provider and Android/iOS/WASM device execution were
not run for this slice; no Linux or device gate is claimed.  No worker invoked
Ninja, changed a shared build directory, installed tools, or updated software.

The common file remains a single-purpose verifier; this patch consolidates an
existing observation rule without introducing another responsibility.  No
allocator, lease, callback, asynchronous task, or executable-memory operation
is added, so OOM, cancellation, and ownership balance are not applicable to this
slice.  Null/schema/ABI rejection continues through the existing entry guards.
