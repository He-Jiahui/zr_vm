---
related_code:
  - tests/parser/support/ssa_source_returned_execir_cleanup.h
  - tests/parser/support/ssa_source_returned_execir_cleanup.c
  - tests/parser/test_ssa_source_returned_execir_cleanup.c
  - tests/parser/support/ssa_source_returned_crt_ledger.h
  - tests/parser/support/ssa_source_returned_crt_ledger.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/cmake/ssa-source-returned-execir-cleanup.cmake
  - tests/cmake/ssa-source-returned-execir-cleanup-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-returned-execir-cleanup-check.cmake
implementation_files:
  - tests/parser/support/ssa_source_returned_execir_cleanup.h
  - tests/parser/support/ssa_source_returned_execir_cleanup.c
plan_sources:
  - .codex/plans/20261010-ssa-returned-execir-cleanup.md
  - 'Root: 2026-10-10 finite cleanup helper gate; Source integration separately authorized'
tests:
  - tests/parser/test_ssa_source_returned_execir_cleanup.c
  - docs/parser-and-semantics/source-returned-execir-cleanup.md
  - docs/testing-and-validation/source-returned-execir-cleanup.md
doc_type: milestone-detail
status: finite-support-accepted
---

# Returned ExecIR cleanup acceptance

## Finite support accepted

Root accepted the actual historical RED, explicit GREEN, independently fresh
default GREEN and MSVC compile-only gates on 2026-10-10. Current GREEN has four
Unity passes and two deliberate failures; the exact CTest wrapper succeeds.
The default gate applies to this finite module only. Source and full-project
acceptance remain open. Shared registration and Git/normal-index transactions
are pending Root's separately tracked work; no commit is claimed here.

The [module design](../parser-and-semantics/source-returned-execir-cleanup.md) fixes resource/report provenance. The [testing guide](../testing-and-validation/source-returned-execir-cleanup.md) fixes the unchanged six-case order and exact five-C-unit link. No Source compiler, VM bootstrap, Function/Global provider, lifecycle fixture or broader archive is selected.

## Actual finite protocols

| Gate | Required actual ordered observation | Current state |
|---|---|---|
| Supporting resource drain | Real allocations registered immediately; actual frees/recovery before report assertions; independent stats/query/free witnesses | Accepted in RED/GREEN |
| Initial refusing report RED | Six failures: five MISSING_COMPLETION and case2 actual PRIOR_FAILURE; zero ignored; Unity natural exit6 | Historical r2 accepted |
| Feature GREEN | Four passes plus case2 PRIOR_FAILURE and case4 PREHEALTH; zero ignored; Unity natural exit2 | Explicit r1 and default r2 accepted |
| Earlier Unity failure | Real UnityFail, teardown with flag unchanged, actual Conclude then next top-level saved-witness test | Accepted; ignore path OPEN |
| Residual recovery | Before live1/frees0, after live0/errors1/frees1 per ledger; original unhealthy report retained | Accepted |
| Idempotent lifecycle close | Same report and every witness unchanged; no additional free/error/fault mutation | Accepted |
| New lifecycle admission | Prior witnesses independently checked, actual live0 and one-slot caller storage before invalidation | Valid route accepted; refusal/capacity branches static only |

Historical RED refused report completion while still draining actual storage.
It is not a runtime reproduction of the old Source coordinator. Neither an
arbitrary CTest failure nor a compiler/link failure is accepted RED. GREEN
retains two deliberate Unity failures; a wrapper pass means the complete exact
negative-run protocol matched. Current CMake defaults to GREEN. The old RED
source freeze is historical evidence, not the current default.

Generic owner requests are real raw allocator storage. They are not opaque modules, VM Functions or Globals. The finite report uses caller-owned one-slot witness storage per ledger. It claims no exhaustive inventory, genuine typed-owner destruction or Source shutdown credit.

## Actual receipts, raw evidence and terminals

Receipts and phase logs are retained under
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`:

| Receipt | SHA256 | Actual result |
|---|---|---|
| `returned-execir-cleanup-red-r2-receipt.json` | `E56A64F47DED0E3C53EED24A4196BE7FBEB9755A4DD1731BAD0185A022D0E9D5` | True; historical explicit RED, child exit6, wrapper0 |
| `returned-execir-cleanup-green-r1-receipt.json` | `39468B07E53C488FE873BCFC8803E3515ED4D7FFD1C4CD93F45DE449F27E0D5A` | True; explicit GREEN only, child exit2, wrapper0 |
| `returned-execir-cleanup-green-r2-receipt.json` | `D90930CB60F58B275EB58B9AB90C0215FBC2306C73FB3D496895EF6647BFC175` | True; independently fresh default GREEN, child exit2, wrapper0 |
| `returned-execir-cleanup-msvc-GREEN-r2-receipt.json` | `867069AC50091FDB8069578D846884AB1C359F8888DDF5DDDD517AD1F81E9322` | True; five compile-only children, exit0/warnings0 |

Default runtime controller19148/unified session93461 reached actual natural
exit0 and closed. Its six children were configure13548, build25048,
latest-deps10004, defined-symbols15668, undefined-symbols35664 and CTest42140,
all naturally terminal0. A separate fresh build/cache/map compiled all five
C sources and linked once, with zero warnings or UBSan diagnostics. Configure
omitted a phase override; actual generated settings contain expected_phase
GREEN. The CTest checker then received that configured phase and validated
all six names in their unchanged order, all 15 fields in each CASE, FINAL,
Unity summary/footer and exact reasons. No `WILL_FAIL` shortcut was used.

MSVC controller4168/unified session95995 reached actual natural exit0 and
closed. Main39744, worker23584, CRTledger41632, ownerallocator41912 and
Unity13440 each naturally exited0, warnings0. Raw streams contain the source
basename only. Actual `/sourceDependencies` Version1.2 headers are
88/26/79/31/19; each record contains Source/Includes and present-empty
ProvidedModule, while ImportedModules and ImportedHeaderUnits are absent.
Actual C11/atomics, `/MD`, `/W4`, `/UNDEBUG` and no forced include were used.
There was no link, executable, runtime or fixed ABI byte-layout validation.

Default runtime build child:
`E:/cargo-targets/zr_vm/build/ssa-20261010-01a0fe2b-returned-execir-cleanup-green-r2`.
Key physical pins from that accepted review are:

| File | SHA256 |
|---|---|
| Current main `tests/parser/test_ssa_source_returned_execir_cleanup.c` | `E3FBA98DE963A546237492A713507C852C79A18CE7947876B2F18C312E7DD299` |
| Current worker `tests/parser/support/ssa_source_returned_execir_cleanup.c` | `91F5560BB612D5AEF8614B48CD1AA39771144C56A8398642DF288ADFC9B26577` |
| Current worker header | `50F818FC30BE1D65A1B3B6DFF9507EB72004F2032A022572FC3FA925C0B55914` |
| Current default-GREEN CMake module | `1BF73D254A815B1E9F9011D918A6B754C3DBCD04BAE038BFD36F62F267CF8035` |
| Build child `returned-execir-cleanup-GREEN-stdout.log` | `25618178A05EDA7A86ACBA8D01ED9CE09718C962609F9F80AAAAAD0261C07291` |
| Build child `returned-execir-cleanup-GREEN-stderr.log` | `B0F5D6F35CD59E1EE5DC1EB56A7385ACFA51CD27FD03D284D8269F9B79FD2FAC` |
| Build child `zr_vm_ssa_source_returned_execir_cleanup_test.exe` | `778BAD091B17D70BE6B33387C926CD52E309207623C3DF50B573BADB2F4267E4` |
| Build child `returned-execir-cleanup-green-r2.map` | `63DBAE46F6AD05CE37868B9BE73064DB1DF9453FCA08704D4E625B8C8774526A` |
| Report `returned-execir-cleanup-green-r2-ctest.log` | `844B2E3B69677E4F3EC339D809BC1D832FFEBC64EE6849438F6EA21C7884EFCE` |

Historical RED stdout is pinned separately at
`9EEF2B916B6CF090AEFEA99B25FEC18804B447DA21DADAC8EB80EFC74E30789F`.
Its raw reasons are five exact MISSING_COMPLETION plus case2 PRIOR_FAILURE.
Explicit GREEN r1 and default GREEN r2 independently preserve identical raw
stdout bytes, with PASS cases1/3/5/6, PRIOR_FAILURE at2 and PREHEALTH at4.
Every run records zero ignored and the final line
`RETURNED_EXEC_CLEANUP_FINAL drain=1 live=0 record_frees=3 errors=3 flags_preserved=1`.

The five actual source/object edges and latest read-header inventories were
checked before runtime. Defined/import evidence covers all five objects and
map/nm identify ten sole selected worker, ledger and Unity providers; actual
`/MD` CRT imports remain unredirected. Root freezes contain metadata only,
including exact18 repository inputs; no source snapshot, archive or Source/VM
provider was used.

Before this owned documentation update, all441 entries of the combined
runtime/MSVC evidence list matched: runtime217 selected +8 metadata +7 artifacts
+2 child streams +18 phase logs, and MSVC169 selected +20 artifacts. Both
selected-input lists contain the three guides. Their original exact18 freeze
pins are historical bridge metadata now: parser
`758D7219A0D472715E77DD0FBED1752EFFCE943ABBFA171031F43291B3FB964E`, testing
`82E43B0070B439DD0EE6F2B83A4A88DDBA83BD433D233CA163714EB6D2194811`, acceptance
`0BBF023CAFA6AEF9E311CBF1C98B2E2DFB3ABC03F820299AA18359594C55DDAA`.
Only these three logical guide paths are updated; remaining code, headers,
tools, metadata, artifacts and raw logs retain strict current pins. Updated
guides are frozen separately for Root's later transaction. No all441-current
claim, runtime rerun or self-hash recursion follows from this update.

The intentional residual case's existing owner recovery emits exactly `EXEC_OWNER_LEDGER errors=1 live=1`; it requires the matching independent before/after witness. This is an expected unhealthy-recovery observation, not healthy shutdown or broad permission to ignore ledger diagnostics. All other ledger diagnostics remain unexpected.

No raw runner is registered with an unconditional `WILL_FAIL` escape. The
checker validates the exact configured phase and protocol, with raw child
exit6/2 distinct from wrapper0. Capacity/refusal, invalid storage, missing
report/begin and general inventory guard branches have static-review credit
only; the finite runtime exercises valid one-slot storage and real residual
recovery. Configuration metadata alone establishes no additional result.

## OPEN boundaries

Actual earlier-ignore cleanup is OPEN; this protocol has zero ignored tests. Pure callback primitives and genuine lifecycle coordinator wiring are separately leased. The existing twenty Source cases, AOT support regression, fresh fault/retry Globals, returned Function/GC ownership and finalizer publication after Attach/Verify remain separate gates.

No Source/VM/network/FFI/provider/security execution is exercised by this child. Complete OOM/retry, leak instrumentation, full inventories, fixed ABI bytes, Linux, full MSVC runtime, platform parity, normal/default/full-project, M1 and all47 remain OPEN. Future receipts require their own current scope and pins.
