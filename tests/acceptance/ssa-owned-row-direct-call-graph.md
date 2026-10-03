---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_target.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_target.h
  - tests/parser/test_ssa_owned_row_direct_call_graph.c
  - tests/cmake/ssa-owned-row-direct-call-graph-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_target.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_target.h
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/02-static-binding-facts.md
tests:
  - tests/parser/test_ssa_owned_row_direct_call_graph.c
  - tests/parser/test_ssa_interprocedural_inlining.c
  - tests/acceptance/ssa-owned-row-direct-call-graph.md
doc_type: acceptance
---

# Owned-row DIRECT graph acceptance

## Required consumer contract

Use only local metadata fixtures and actual Core/parser translation units.
Every valid fixture must first pass public `VerifyModule(ALL)` and
`BuildCallGraph`; malformed preflight cases assert the exact failure and old
graph preservation instead. A metadata row must not execute a callback.

| Gate | Required evidence |
| --- | --- |
| Genuine baseline RED | Caller 1, owned ref 1, legacy decoy 2, real target 3; full Core/build succeeds, desired resolved target assertion fails |
| Independent validator RED | Forged target and missing/duplicate typed DIRECT, unsupported and typed-empty site fail against original graph, covering all three checksum modes |
| Local resolution | CALL and INVOKE resolve actual same-module effective publication; old numeric hints cannot choose the decoy |
| Conservative rejection | Valid indirect, external relocation, owner-layout, missing/ambiguous, signature/module/generation disagreement stays UNKNOWN with zero identity |
| Atomic preflight | Malformed Core row rejects with exact instruction/source diagnostic and original graph storage/content unchanged |
| Validation | Recomputed and zero checksum cannot bypass typed scalar identity, unsupported inlining, required native effects or site correspondence |
| Summaries and rewrites | Patchability, semantic hashes, imported native effects and SCC recursion remain sound; typed Inline/Devirt remain UNSUPPORTED |
| Compatibility | Existing unchanged fourteen interprocedural groups pass |
| Build provenance | Actual MSVC plus Linux GCC/Clang ASanUBSan TUs, source/header hashes and registered standalone CTest receipt |

## Evidence lineage

The immutable baseline and mutable draft are D-only private copies; all source
and header pins are recorded in `lineage-manifest.json`. The first baseline
DIRECT RED was observed using actual MSVC and GCC with fourteen real Core TUs
and the original graph. The independent validator matrix ran thirty desired
semantic RED cases after full Core/build preconditions passed, with no setup
failure and no drift in sixty-six first-party inputs. Its retained-checksum
cases fail the desired TARGET_MISMATCH diagnostic because the old validator
only notices a generic checksum error; zero/recomputed modes expose acceptance
of the forged or incomplete graph directly.

The historical first DIRECT fixture has an unreachable reversed-argument
Validate call after its desired failing assertion. That source and warning
history remain preserved. The candidate fixture uses the actual public
`Validate(graph,module,diagnostic)` order throughout.

Candidate build failures and corrections remain under distinct versioned D
output labels. Actual current-production acceptance below binds the adopted E
source epoch and has separate receipts from private candidate GREEN.

## Current production validation

The adopted current E source epoch passed fresh MSVC compilation of 38 original-E
translation-unit declarations and registered CTest 2/2, including 59 owned-row
cases and all fourteen unchanged interprocedural groups. GCC and Clang each
freshly compiled nineteen real Core/parser/fixture TUs from the 68-input
source/header snapshot verified against current E, then passed all 59 cases
under ASan/UBSan in actual Linux ELF executables. Native Windows LLD used actual
GNU/Clang Linux SDK plans; the receipts bind compile/link/runtime PIDs and argv,
dependencies, SDK/object/binary hashes and raw logs. Source guards stayed stable.

Root independently repeated the registered 2/2 tests and both sanitizer ELF
fixtures, with an explicit Linux D temporary directory for its Linux repeats.
The original source-validation drivers' inherited Linux temporary-directory
scope remains recorded in their historical receipts. Root verified the actual
parent CMake include directly after `ssa-tests.cmake` and its registration in
the serialized DIRECT integration epoch. DIRECT integration proceeds
independently of the DIV work.

The formal current-production receipt is `D:/tmp/zr_vm/ssa-20261003-01a0fe2b/validation-control/direct-current-e-formal/receipt.json`,
SHA256 `241784cc81d2e2538de1390b2cb9ac86947270802c90ef42bf6f57ad18374cc0`, bound to root-verified main HEAD `c8c793da5d5868e2f42aeb4812e00e13c0fab9c6`.
It binds the original source gates, actual root repeats and parent registration
receipts. Earlier D baseline RED, private candidate passes and setup failures
remain historical evidence.

Typed execution, external/indirect resolution, inlining and devirtualization
remain outside this local scalar graph contract. Validation uses local metadata
fixtures without provider callbacks, plugin loads or network/security routes.
