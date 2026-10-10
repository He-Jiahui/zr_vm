---
related_code:
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/core/support/ssa_exec_ir_owned_module_shim.h
  - tests/core/test_ssa_exec_ir_owner_live_span.c
  - tests/unity_config.h
  - tests/cmake/ssa-owner-live-span.cmake
  - tests/cmake/ssa-tests.cmake
  - tests/cmake/ssa-owner-live-span-direct-validation/CMakeLists.txt
implementation_files:
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
plan_sources:
  - 'Root: 2026-10-09 finite test-ledger live-span query contract'
  - .codex/plans/20261009-ssa-exec-owner-live-span-protocol-v1.md
tests:
  - tests/core/test_ssa_exec_ir_owner_live_span.c
  - docs/testing-and-validation/exec-ir-owner-live-span.md
  - docs/acceptance/exec-ir-owner-live-span.md
doc_type: milestone-detail
status: finite-green
---

# ExecIR test-ledger live-span acceptance

## Current acceptance state

`finite-green`: Root accepted actual functional RED r3, dedicated Clang UBSan GREEN r1, and separate MSVC GREEN r1 compile-only evidence. GREEN's twelve real-heap cases all PASS. The current header and allocator implement the finite numeric query contract. Shared registration is present in the checkout; normal full-project configuration/runtime remains unexercised by this finite child. Root's session tracks the commit and normal-index transaction separately from these native runtime/compiler receipts. There is no complete allocator-failure denominator or general cleanup claim.

The work is a test-support prerequisite for later observers. Existing Core owner, module-copy, primitive-frame, callable-query and declared finalizer evidence remains evidence for those respective scopes. It is not acceptance of this new helper and does not turn a genuine Source wrapper GREEN.

## Contract under test

`ZrTests_ExecOwner_TryLiveSpan(first,last,pointer,count,width,output)` has an integer success/refusal result and independent genuine writable typed output. Refusal preserves output; success returns the uniquely containing current live record, allocation base, and recorded allocation bytes. The result does not authenticate an owner type or dereference/validate the queried storage.

Required preflight includes half-open nonempty ordered record bounds inside current inventory and capacity; nonnull pointer/output; nonzero count/width; representable size multiplication and numeric interval endpoints. Global unique live containment precedes record-window selection. Realloc's current record pointer/bytes govern results; freed records do not count.

The implementation also checks byte-count and pointer round trips, and refuses malformed current live rows with nonzero frees, zero bytes, nonrepresentable conversion or wrapped ends. It publishes a temporary result only after the complete scan and window checks. Those malformed-row and duplicate-containment branches are implemented but not experimentally exercised by the finite real-heap fixture.

The query must allocate and free nothing, consume no fault attempt, append no event, mutate no ledger statistic, invoke no callback, and assert nothing. No nine-field Core diagnostic is promised by this API. Output struct size is determined by the actual C ABI, not a numeric layout assumption.

## Accepted finite closure

The actual selected target has only three repository translation units, with no linked Core/Parser/VM archive and no forced include: actual test main, shared actual allocator, real Unity. Actual settings are Clang 19.1.5 `clang-cl`, Ninja Debug, C11 without extensions, `/W4 /UNDEBUG`, dynamic CRT and UBSan with no recovery. The standalone project reads `E:/Git/zr_vm` directly and builds under `E:/cargo-targets/zr_vm/build/ssa-20261009-01a0fe2b-owner-live-span`; no source snapshots were created.

Twelve ordered behavioral cases cover the actual small allocation profiles and observed matrix in the [testing guide](../testing-and-validation/exec-ir-owner-live-span.md). Cleanup is performed before semantic Unity assertions and recovery health is compared against the already-clean original ledger state. Protocol mapping is `.codex/plans/20261009-ssa-exec-owner-live-span-protocol-v1.md`, authored separately by Root before execution authorization; its accepted SHA-256 is `DCB69B13775293F1C3C4F2060D7C988D8BF78A45DBE163E1B4B70B3BB342C879`.

## Actual receipts and preserved attempts

All receipts and raw logs below are under `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`. Each row names the actual attempt; failed evidence gates remain preserved.

| Receipt file | Validation and observed scope | SHA-256 |
|---|---|---|
| `owner-live-span-red-r1-receipt.json` | Rejected dependency-source model; three actual phases exit 0; three fresh compiles plus link; no CTest | `6EC51D465B60F928ABF75FCD7D2D9FC2DDEE0E9032B8E779848591C983E45276` |
| `owner-live-span-red-r2-receipt.json` | Rejected symbol-output diagnostic scan; four actual phases exit 0; incremental link; no CTest | `1DC7943A140A3EC7808DE20E43FB115D2DDA101776CC40CEE30DE6169789280A` |
| `owner-live-span-red-r3-receipt.json` | Accepted functional RED; seven phases, CTest exit 8; cases 1–6/11–12 feature FAIL, 7–10 guard PASS | `4C426ACA7CED669EE9D817A483937C4E2CE18E891E2E2101B9DB7392DFB71048` |
| `owner-live-span-green-r1-receipt.json` | Accepted finite GREEN; seven phases exit 0; 12 PASS, 0 failures, 0 ignored | `78C9283EE93AC06DBB15465A1BFE650D82CF2DA493CFAEF79E543BD9BC2706F9` |
| `owner-live-span-msvc-GREEN-r1-receipt.json` | Accepted four compile-only units; four exits 0, zero warnings, no forced include | `74A3860AAFB3D7C3123A940C8DF93EFE42695F81E5C429FB686C49C874B3B812` |

RED r1 expected the primary C source inside the Ninja MSVC header-dependency list. The corrected model takes primary source identity from the configured compiler object/source edge and read headers from actual latest VALID Ninja dependencies. RED r2 treated the COFF `R` symbol string containing `EXEC_OWNER_LEDGER` as a runtime diagnostic; that symbol inspection false positive was corrected before RED r3. Neither rejected attempt is behavioral RED evidence.

Accepted RED r3 observes the eight exact feature failures with zero attempt/free deltas in their failure markers, four refusing-input guard passes, no ignored case, and no ledger or UBSan diagnostic. GREEN r1 observes all twelve passes and no feature refusal, warning, UBSan, ledger, unknown-free or resource diagnostic in the selected raw outputs. RED r3 was an incremental link; GREEN r1 recompiled main and allocator and linked in three steps, reusing Unity's object. A clean GREEN full rebuild is not claimed.

## Physical inputs and provider evidence

Runtime receipts retain selected compile commands, source/settings metadata, tool and actual selected input pins, object/executable/map pins, actual latest dependencies, symbol output, and post-run current pins. The link map identifies `ssa_exec_ir_owned_module_allocator.c.obj` as the unique provider of `ZrTests_ExecOwner_TryLiveSpan`; main references that symbol, and allocator references real CRT malloc/calloc/realloc/free. The actual Ninja rule has `deps = msvc` and `/showIncludes`; primary source edges and read headers are checked separately.

| Physical input | RED r3 SHA-256 | GREEN r1 / MSVC GREEN r1 SHA-256 |
|---|---|---|
| Allocator header | `0DBF1436F5E9E3BDB485F39C40EF0BFADB9381AF69376BEB2D1A9D25DEBD15C2` | `3571F64F3A8E6524A6CB66813B9BEBD7558DFD3A1F79D21B530F72EF3DF9097C` |
| Allocator C | `6C4E4F85B3B61F1DC9B2CAB3BD65C1F0BA44A45F7F43BE6CD9A2ED19983B7CAD` | `41CD25D473BB8CA87C52F7AAEC3B1FE7A9AB9778830B22572DAA70177F1EDC07` |
| Finite test main | `835B3AA3BB91A195BA56A45A2D92B83D94CB668C3CDD3BFCC1F10F74DEEA41C2` | Same |

The supplied CMake pair remained pinned across accepted runtime attempts: direct-validation `CMakeLists.txt` SHA-256 `24CFA798A42D20090422C40ADC869A2CCA9E6617375AC300591702224E91EEE1`; target module SHA-256 `F331AF5DC758058648D19263269B34A5530AFC544BE185090E0A3E7D68C65B71`. Shared registration was added after these receipts, so the receipt's dedicated graph does not establish normal full-project configuration or execution.

## Natural process and MSVC compile-only boundaries

Accepted GREEN runtime controller PID 15724 and unified process 57070 naturally ended with exit 0. Its actual receipt children are configure 39960, build 13468, dependency inspection 42168, allocator defined-symbol inspection 24940, allocator undefined-symbol inspection 6800, main undefined-symbol inspection 36744, and CTest 23632; each exited 0 through an actual `WaitForExit`. RED r3 controller 11396 recorded six pre-CTest child exits 0 and CTest child 37052 exit 8. These are historical actual terminal records, not claims about newly running processes.

Separate MSVC GREEN r1 controller 21116 and unified process 58489 naturally ended with exit 0. Actual `cl.exe` children were main 37648, allocator 35144, Unity 22104, and header probe 40064, all naturally terminal with exit 0 and zero warnings. Each raw log contains only its expected source basename on one nonempty line. Compile arguments include `/TC /std:c11 /W4 /UNDEBUG /Od /MD /utf-8 /c` and no forced include. No link, runtime execution or fault arming occurred in this check.

The fourth unit is a fresh authored tiny typed declaration probe under `E:/cargo-targets/zr_vm/tmp`, not an embedded or copied repository source. Actual `/sourceDependencies` JSON Version 1.2 identifies the four primary sources and respectively 26/31/19/7 read headers. `ProvidedModule` is present and empty; `ImportedModules` and `ImportedHeaderUnits` are absent in the actual C metadata. Root independently reviewed 100 selected physical input pins and actual artifacts with zero current drift. The probe compiles the typed function-pointer declaration and a `sizeof(SZrOwnerLiveSpan)` expression; it establishes no fixed ABI byte-size or 32-bit/runtime result.

Root independently accepted the runtime and MSVC evidence. The module is included by `tests/cmake/ssa-tests.cmake` in the current checkout; normal full-project configure/runtime remains unexercised by this finite child. These native receipts cover runtime/compiler gates only; Root's session separately tracks the commit and normal-index transaction. No deletion of obsolete objects or general cleanup is asserted by these receipts.

## OPEN boundaries

Duplicate live containment, 32-bit geometry, arbitrary storage widths, capacity saturation and complete OOM/retry sweeps are not established by the initial finite design. Numeric containment is distinct from readable initialized typed storage, source provenance, valid tagged unions, owner uniqueness, canonical admission, registry retention, and safe execution.

Source/Binder integration, normal full-suite validation, default backend choice, Linux, full MSVC runtime, native parity and all47 remain OPEN. No Source/VM/provider execution, Oracle, network, FFI, security behavior, new provider service or registry ownership is exercised in this dedicated closure. Future expansion needs its own explicit scope and current evidence.
