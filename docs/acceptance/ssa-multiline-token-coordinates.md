---
related_code:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - tests/parser/test_ssa_source_range_identity.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
plan_sources:
  - .codex/plans/20261005-ssa-multiline-token-coordinates.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_ssa_source_range_identity.c
  - tests/acceptance/ssa-multiline-token-coordinates.md
doc_type: acceptance
status: finite-multiline-token-coordinate-green
---

# Multiline Token Coordinate Acceptance

## Finite decision and environment

**Finite multiline token coordinate GREEN** covers bare CR, LF, CRLF and mixed
newlines in actual legal template tokens. The [module contract](../parser-and-semantics/ssa-multiline-token-coordinates.md)
and [test guide](../../tests/acceptance/ssa-multiline-token-coordinates.md)
define the raw bytes, fixed coordinates, lookahead/Next and AST shape checks.
The historical [13-case SCRIPT/RETURN baseline](ssa-source-range-identity.md)
and its original 85-case regression retain their frozen evidence unchanged.

Route `01a0fe2b-2f6b-7063-9a67-ac0e08c0d82d` uses the direct checkout at
`E:/Git/zr_vm`, build root
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`,
and temporary root `E:/cargo-targets/zr_vm/tmp`. No source snapshot was created.
The local compiler environment is Windows x64 clang-cl 19 Debug assertions
with UBSan; CTest logs use `halt_on_error=1:print_stacktrace=1`.
All receipt/log names below are relative to
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/`.

## Frozen RED

Real RED commit: `44e5ab10240a1720c842bcf95d91fe5f3f42823f`, one changed test
path, author and committer He-Jiahui <814111601@qq.com>. Its receipt was produced
against preceding HEAD `aa49954910b85944161b99bab1eeefff186b82ff`.
Configure/build exited 0; `--baseline-only` reported 13/13 PASS and exit 0.
Full focused CTest exited 8: 17 cases, two failures, zero ignored. The bare-CR
template end line was Expected 2 / Was 1; mixed was Expected 4 / Was 3.
LF and CRLF controls passed. No UBSan diagnostic was observed.

| RED artifact | SHA-256 |
| --- | --- |
| `multiline-token-red-receipt.json` | `BBE6812C239F7E4E14FFD20DCADD0A40195742FF5FE2D7F73B8F27F87F2E0667` |
| `multiline-token-red-configure.log` | `DB8F7810E9D7859A6C7B3C57B725B6F875C94F981D5121470165FF45E78EB54B` |
| `multiline-token-red-build.log` | `6C350F8E4A1CE7C15EC0D6F6E017076E441B40127EAD2194B04DD848F3CB0D87` |
| `multiline-token-red-baseline.log` | `3F130AD628CDC2E45ED42421704CFB3AF4D1DD3E1C1D346A6AC803A31335F79C` |
| `multiline-token-red-ctest.log` | `71FF15CC83F287D05115DD5DFBF6CF39E411ECECC2E8548F88F5AF8C3A914EC2` |
| Frozen test fixture | `FBB42872EEE119E28D96E33C075F7A20CCE1159A5773E5536A230603E533D1C8` |
| Historical `parser_state.c` | `43C9BE6D6FC4D61D1DF74F6E1ADDD53B17163874D151F773EFE219C1FA7F2ECB` |
| Historical range executable | `C28C153441F7C5CB3A2CF5991C7E18A2DD9D65C4E22DEFF95B63E6B439604A01` |

RED receipts/log hashes were verified from the retained files. The historical
production file, executable and `source-direct-target-sources.tsv` were
superseded in place by GREEN; their RED pins are historical receipt identities
and do not match today's artifacts. The two other metadata files still match
their RED pins. Root's RED driver 57134 and commit job 54764 naturally exited 0.

## Actual GREEN and consumer regression

The GREEN receipt's base HEAD and RED commit both identify
`44e5ab10240a1720c842bcf95d91fe5f3f42823f`. Configure, build and CTest each
exited 0. All 13 CTest entries passed: 12 Unity suites total 170 cases, zero
failures and zero ignored, plus standalone dominator CFG PASS. The 170 split
is 17 range + 85 original consumers + 68 dependency consumers.

| CTest entry | Actual result |
| --- | --- |
| `ssa_source_range_identity` | 17 Unity cases PASS |
| `ssa_source_execbc_vm_loops` | 6 Unity cases PASS |
| `ssa_source_straight_line_cfg` | 35 Unity cases PASS |
| `ssa_source_callable_return` | 8 Unity cases PASS |
| `ssa_source_script_entry_tokens` | 8 Unity cases PASS |
| `ssa_source_callable_identity` | 11 Unity cases PASS |
| `ssa_source_script_entry_identity` | 13 Unity cases PASS |
| `ssa_typed_binding_contract` | 4 Unity cases PASS |
| `ssa_dominator_cfg` | Standalone PASS |
| `ssa_dead_source_places` | 30 Unity cases PASS |
| `ssa_host_primitive_layout` | 15 Unity cases PASS |
| `ssa_primitive_source_frame` | 19 Unity cases PASS |
| `ssa_primitive_source_frame_storage` | 4 Unity cases PASS |

| GREEN artifact | SHA-256 |
| --- | --- |
| `multiline-token-green-receipt.json` | `FCFC11C4B3C1CC89C35D112266C2FEB04AEC0C6FFD5A5072752AC010EDD60E07` |
| `multiline-token-green-configure.log` | `1C66011DB51E75935E87E02C1B1EE6DA4334ECB8E53BF2E7B2C6A2290767E57F` |
| `multiline-token-green-build.log` | `B7BB7AF1DAD3E8F95AFB3D20FB8CCFBB009855F2232469A7B96F2EB441F59DAA` |
| `multiline-token-green-ctest.log` | `F631E62BBAD49F8CCFCFFFB3C4CF4692762804B6E243C1C7DDD2CB4C434F91E5` |
| Tested `parser_state.c` | `05D2A8889CE2564A70D16781F110FB7DB9234BA92D122464ADA1519BC4620204` |
| Tested range fixture | `FBB42872EEE119E28D96E33C075F7A20CCE1159A5773E5536A230603E533D1C8` |
| Range executable, `bin/zr_vm_ssa_source_range_identity_test.exe` | `6481E9F602D6BE2E5A7DEE39616444C21E73489225D1CC2F67870593BC688D87` |

The receipt contains all 13 source pins, exact commands and three generated
metadata pins. Each current source pin, metadata pin, phase log and the range
executable was hashed and matched when preparing this record. Actual CTest
totals and PASS lines were inspected; no UBSan diagnostic was observed.
The source graph records the direct checkout. Root's aggregate receipt job
85784 naturally exited 0. Root records the finite GREEN commit separately,
avoiding a commit hash that would depend on this document's own contents.

## MSVC compile-only recovery

The initial GREEN driver 27182 naturally exited 2 after successful configure,
build and the above CTest run. Its MSVC invocation overescaped quoted macro
values and emitted C2001/C5102. The original failure log remains retained.
Only argument transport changed to `ProcessStartInfo.ArgumentList` with
`UseShellExecute=false` and literal quoted macro values. No production source
changed and the 13 CTest entries were not rerun for this argument repair.

MSVC 19.44 (`14.44.35207`) then compiled the changed `parser_state.c` TU with
`/c /TC /std:c11 /W4`, exit 0 and no warnings. Tool cell 11/chunk `db7456`
naturally completed with exit 0. This establishes compile-only compatibility
for that TU; it does not establish linking, runtime or full MSVC matrix GREEN.

| MSVC artifact | SHA-256 |
| --- | --- |
| Retained `multiline-token-green-msvc-smoke.log` | `7E805A863C6558540710241F13AB8E29CB26FB69DF5B16489C2B354FC69A14FB` |
| `multiline-token-green-msvc-smoke-r2-receipt.json` | `7E93057D11868C9939FB383CF6575FC695BBF8AF33BA090A522B711D9EAA9934` |
| `multiline-token-green-msvc-smoke-r2.log` | `9BF3B9BAC853C71FA2F8F0953D0832E9E66DE4B891FBF3086520517EE2F71A7F` |
| Build-root `msvc-multiline-token/parser_state.obj` | `476412E8E862152B354E305E514ABB8AB1E573391BF5D966F2A03454AF3FDE3E` |

Both MSVC logs, recovery receipt, object and its two source pins were hashed
and matched. The recovery is embedded in the aggregate GREEN receipt.

## Remaining OPEN gates

Full Linux GCC/Clang acceptance is **OPEN** from existing WSL E_ACCESSDENIED;
full MSVC matrix is **OPEN**. A span ending in CR has a static bound argument,
without an independent dynamic fixture. Template AST ranges remain **OPEN**:
`parser_literals.c` advances Next before template construction despite the
earlier saved `literalLoc`. Offset-cache CR handling, diagnostic snippet CR
handling, full parser/OOM, descriptor/native text execution, retention and the
full 47-item SSA plan remain **OPEN**.

This new gate exercises local parse/scan behavior. Consumer regressions include
existing local VM/Oracle fixtures; they add no network/import, FFI, provider,
capability or security test evidence. No external delivery or whole-tree
acceptance is implied by the finite result.
