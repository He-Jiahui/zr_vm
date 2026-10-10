---
related_code:
  - tests/parser/support/ssa_source_returned_crt_ledger.h
  - tests/parser/support/ssa_source_returned_crt_ledger.c
  - tests/parser/support/ssa_source_returned_retention_fixture.h
  - tests/parser/support/ssa_source_returned_retention_fixture.c
  - tests/parser/test_ssa_source_returned_crt_ledger.c
  - tests/cmake/ssa-source-returned-crt-ledger.cmake
  - tests/cmake/ssa-source-returned-crt-ledger-direct-validation/CMakeLists.txt
  - zr_vm_core/include/zr_vm_core/aot_ir.h
implementation_files:
  - tests/parser/support/ssa_source_returned_crt_ledger.h
  - tests/parser/support/ssa_source_returned_crt_ledger.c
plan_sources:
  - 'Root: 2026-10-10 finite Returned CRT ledger extraction'
  - .codex/plans/20261010-ssa-returned-crt-ledger-extraction.md
tests:
  - tests/parser/test_ssa_source_returned_crt_ledger.c
  - docs/parser-and-semantics/source-returned-crt-ledger.md
  - docs/testing-and-validation/source-returned-crt-ledger.md
doc_type: milestone-detail
status: finite-green
---

# Returned CRT ledger extraction acceptance

## Current state and scope

`finite-green`: Root accepted actual Windows clang-cl UBSan RED r1/GREEN r1 and separate MSVC compile-only retry r2 on 2026-10-10. Public `TryStats` was the sole refusing RED feature; GREEN changed only its five-line guard/copy implementation. Existing ledger callbacks and other read APIs are actual extracted behavior, not synthetic mocks. Prior Source, LiveSpan, owner or finalizer evidence cannot become a result for this child.

The one extracted ledger contains both kinds' actual CRT allocations, records, events and named counters. The lifecycle coordinator delegates to it and retains asserting compatibility accessors. The finite executable excludes that coordinator and every Source/VM/provider archive. Common/Core headers supply actual typed geometry only.

Root's plan is `.codex/plans/20261010-ssa-returned-crt-ledger-extraction.md`. Shared registration and Git/normal-index transactions are tracked separately by Root's session. These native receipts cover finite runtime/compiler gates only and make no commit or Source-execution claim.

## Accepted functional RED and GREEN

The [testing guide](../testing-and-validation/source-returned-crt-ledger.md) fixes the unchanged physical eighteen-case order. Actual RED r1 observed 16 explicit `RETURNED_CRT_STATS_FEATURE actual TryStats must succeed` failures in cases 1–9 and 12–18, plus genuine guard/lookup passes in cases 10–11; total 18 tests, 16 failures and zero ignored. Every failure followed actual cleanup, expected health checks and other observations. Actual GREEN r1 observed all eighteen PASS, zero failures and zero ignored, without warnings or UBSan diagnostics.

Refused TryStats outputs preserve every sentinel field. GREEN's positive public stats returned exactly one and matched the six exact expected counters. The internal old Stats copy was used only for purity and cleanup health; no success through that helper counts as public feature acceptance. Root and Core verified that replacing only the GREEN function with its original RED body reconstructs the whole RED ledger file exactly. Current ledger C is 99 physical lines/4,711 bytes, SHA256 `D4115AFC71D6B7D938D1DC1F08B67407D8DF1CAF6D654F2F5E8C1AD72EE167B4`; other code/CMake pins were preserved.

## Finite design coverage

| Behavior | Actual finite test coverage | Current runtime state |
|---|---|---|
| Both kinds and live/free counters | Empty inventories, actual owner request, all six descriptor allocations, freed record reads | PASS |
| Descriptor geometry and zeroing | Actual six typed `sizeof` widths and 3/1/1/3/1/1 counts, readable actual calloc bytes | PASS |
| Six descriptor injections | Separate genuine prefix and actual injected event at each site | All six PASS |
| Refusal preservation and read purity | Independent outputs, actual before/after named counters/events | PASS |
| Exact current-live lookup | Actual bases, interior/one-past/foreign/cross-kind/freed refusal | PASS |
| Pending faults and rebase | Actual next owner request, rebased descriptor/owner sites, unarmed fault preservation | PASS |
| Invalid geometry | Zero/overflow/wrong shape refuses with exact errors and no event/attempt | PASS |
| Real attempt inventory limit | 256 actual small owner allocations, refused next request, exact events and record frees | PASS |
| Recovery errors and idempotence | Actual live rows in both kinds, one error each, explicit recovery and retained fault | PASS |
| Null and readable foreign free | Null no-op, foreign refusal with exact errors and no CRT free | PASS |

No case resets a live ledger, constructs a row, mocks a provider or probes an arbitrary invalid pointer. Every returned allocation is immediately registered. Ordinary cleanup disarms and frees before Unity assertions; intentional recovery cases transfer the registered roots to ledger recovery and retain the visible expected errors. Recovery does not conceal leaks or turn an unhealthy ledger into zero-error acceptance.

## Actual receipts and retained failure history

All receipt and raw-log files below are under `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`.

| Actual attempt | Receipt SHA256 | Accepted result |
|---|---|---|
| [Runtime RED r1](E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/returned-crt-ledger-red-r1-receipt.json) | `154841887037CC1AD0B99D8F54602E582FCA4D4C4BE735F35B15092ADEA9A5EA` | validated=true; exact18/16 feature FAIL/2 guard PASS/0 ignored |
| [Runtime GREEN r1](E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/returned-crt-ledger-green-r1-receipt.json) | `FD36B35A4D9B0F2EF95FE3D81235ADB3BE2082A10ADC91E149B93846B79301D2` | validated=true; all18 PASS/0 failures/0 ignored |
| [MSVC GREEN r1](E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/returned-crt-ledger-msvc-GREEN-r1-receipt.json) | `CB7E30A270C140A8D7FD7B7B36745C1BF932817C1D401A67D28ADD58917422ED` | validated=false; coordinator atomic-support C1189; probe uninvoked |
| [MSVC GREEN r2](E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/returned-crt-ledger-msvc-GREEN-r2-receipt.json) | `EDD9188E7A3C0AA78D2F31B5C69946EF45808BC715B4CD01FF6B5747E6A3D5DF` | validated=true; five compile-only units exit0/warnings0/no forced include |

The [RED raw CTest log](E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/returned-crt-ledger-red-r1-ctest.log) hashes to `DE01FA5BF05822D6BCD74F01880D2D836F365B0AB62367EE7101D55CEA4E1303`; the [GREEN raw CTest log](E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/returned-crt-ledger-green-r1-ctest.log) hashes to `B8FF04BAB31BE1EA2DBB8EA01ED7001305A801E91DC06D7956E0F74CED0F4983`. Neither contains UBSan diagnostics. RED controller43424/unified session53226 ended naturally at exit0; its seven actual phases were six exit0 plus CTest42020 expected exit8. GREEN controller41076/unified session50933 ended naturally at exit0 with all seven phases exit0, including CTest3952.

RED's fresh cache actually compiled main, ledger and Unity, then linked. GREEN reused the cache and recompiled only ledger, then linked. Actual configured source/settings TSVs, compile commands and latest dependencies identify this three-TU closure, with read-header counts ledger184/main191/Unity29 and primary sources pinned separately. The link map and symbol evidence identify one actual extracted ledger provider; its CRT allocation/free imports are `__imp_calloc`, `__imp_malloc` and `__imp_free`. At Root's runtime review before the documentation update, all 208 selected input pins, seven metadata pins and five artifacts (220 total) had zero drift. The remaining 219 compiled-input/tool/metadata/artifact pins remain strictly current; the old testing-guide SHA256 `DD11CDCF8BDBAD9CC66DB76592A2E70D65CA01B1AFDC110A0D89095F996EB599` is historical protocol metadata, and the owned updated guide is separately frozen with the exact eighteen-case order unchanged. No runtime rerun or C/header/tool change is claimed. Historical baseline inventories are not current admission evidence.

The first MSVC controller6748/unified session59839 ended naturally at exit1. Test, ledger and Unity compiled at exit0; coordinator27460 ended naturally at exit2 with C1189, and the header probe never ran. Root corrected only the driver option `/experimental:c11atomics`, matching top-level CMake line38, and verified the whole v2 inverse against v1. [MSVC driver v2](E:/cargo-targets/zr_vm/tmp/ssa-returned-crt-ledger-msvc-gate-v2.ps1) SHA256 is `48A9CF8D6BB3AA4317FB187B1E72CCFA3F6E99C6BDD1D90502BF1969A221BD4E`; no source correction was needed.

Retry controller13340/unified session48938 ended naturally at exit0. Actual compile-only children test5432/ledger10000/Unity27956/coordinator20644/header41040 all ended at exit0 with zero warnings, no forced include, and basename-only raw compiler output. Actual `/sourceDependencies` JSON Version1.2 recorded read-header counts86/79/19/185/18 respectively. `ProvidedModule` is present-empty; imported-module/header-unit fields are absent, not invented empty arrays. Root checked 255 selected inputs and fifteen raw/object/dependency artifacts with zero drift. The typed header probe checks declarations and actual `sizeof` expressions; no fixed ABI byte claim, link or coordinator runtime credit follows.

These observed results establish the finite runtime and compiler gates. Incremental GREEN, compiler-only checks and generated metadata cannot establish a clean/full rebuild, normal-project runtime or unexecuted Source/lifecycle behavior.

## OPEN boundaries

This finite extraction does not establish Source/Binder provenance, returned Function ownership or GC lifetime, Source acceptance after the changed coordinator, canonical admission, registry retention, VM/provider execution or post-Attach/Verify finalizer handoff. BeginCall and Reset preconditions are respected rather than tested through unsupported invalid calls.

Complete arbitrary-profile CRT OOM/retry behavior, 32-bit and layout portability, Linux, full MSVC runtime, platform parity, normal Source/full-project gates and all47 remain OPEN. No Source/VM/provider/network/FFI/security execution or new production allocator service is exercised in the dedicated link. Future gates require their own exact scope and current evidence.
