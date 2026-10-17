---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_gc_map_internal.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - tests/cmake/ssa-gc-map-metadata-consumers.cmake
  - tests/cmake/ssa-execir-owned-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_gc_map_internal.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
plan_sources:
  - .codex/plans/20261016-ssa-gc-map-outer-shape.md
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
tests:
  - tests/parser/support/ssa_execir_gc_map_outer_shape_cases.inc
  - tests/parser/test_ssa_core_model.c
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
  - tests/core/test_ssa_exec_ir_owned_module.c
  - tests/core/test_ssa_exec_ir_module_copy.c
  - tests/parser/test_ssa_gc_map_metadata_consumers.c
  - tests/core/test_ssa_owner_try_capture.c
  - tests/parser/test_ssa_primitive_frame_validation.c
  - tests/parser/test_ssa_deopt_aggregates.c
  - tests/cmake/ssa-gc-map-metadata-consumers.cmake
  - tests/cmake/ssa-execir-owned-direct-validation/CMakeLists.txt
doc_type: testing-guide
status: partial
---

# SSA GC-map outer-shape finite acceptance

## Status and evidence identity

Root and the independent reviewer accept actual bounded Core/consumer RED and
GREEN-r2, primitive-r3, TryCapture-r1 and finite MSVC-r3 syntax. Publication and
cleanup are **PENDING**. Historical
completed finite outcomes remain **33**. Neither01.01 nor the full47 goal closes.

Actual checkout: E:/Git/zr_vm. Reused LowerB:
E:/cargo-targets/zr_vm/build/ssa-20261009-01a0fe2b-execir-owner.
Immutable evidence directory:
E:/cargo-targets/zr_vm/reports/ssa-gc-map-outer-shape-01a0fe2b/.
These receipts bind source/tool/configuration/raw/dependency/object/input identity;
no source snapshot was created.

| Record | Bytes | SHA256 |
| --- | ---: | --- |
| red-r1-functional-review.json | 76279 | E1A9D0BC4F884C597BCF8C6F3D31D4844A7E14EFECF3481AEBC88971E46ED4CC |
| green-r2-receipt.json | 1322915 | 387D11303430DB73FB77E0C95AB68901C5D6D1176E8E66325C1BABD6426616DC |
| green-r2-functional-review.json | 3055 | BC840AD802FDA563B2065732AEFFDB5DDC6A0465DA2CDED8C43646007EF6A974 |

Original RED controller failures and raw evidence remain retained. Accepted
metadata recovery does not relabel the original controller attempts as successful.

## Representation and production scope

| gcMap | Count/capacity | Meaning |
| --- | --- | --- |
| NULL | 0/0 | Absent |
| non-NULL | 1/1 | One owned header, including present-empty |

Multiple records are entries inside the header; entries, slot-index pool and
inline-reference-offset pool retain independent checked geometry and ownership.
Zero and multiple entries are legal. Readable, live, properly owned input remains
a caller precondition; the private predicate cannot authenticate arbitrary pointers.

Six production paths document/enforce this rule: public field comment, private
predicate, CloneFunction, Core structural/storage verify, interpreter metadata
validator and Parser projection. Four guard entries preserve earlier refusal
ordering, existing categories and nine-field storage diagnostics; Parser binding
rows remain earlier. Public layout/signatures/gcMap-gcMaps alias/schema/hash
algorithms stay unchanged. Embedded projection maps, AOTIR maps and GC descriptors
with other representations do not inherit the Core outer predicate contract.

Absent clone remains NULL/0/0; a present candidate publishes1/1 after header
allocation. Three-pool allocation order, overflow checks, temporary candidate,
partial cleanup, pointer-based Free and final destination replacement remain
transactional. Later malformed module functions may follow earlier allocations:
require rollback and old-owner stability, not whole-module zero allocations.

## Actual RED boundary

Four actual CTests fail at five new assertions: malformed owner source after an
earlier function, first Core Clone rejection, module-copy replacement, validator
rejection and projection rejection. Core stops at the first real two-header(2,2)
Clone row. **No all24 Core RED or Verify RED credit.**
Actual RED:45 compiles,5 exact-cache,50 selected objects,247 unique dependencies.
Owner cases/two61-site sweeps,copy88 sweep and three legal consumer cases remain
observable; two new consumer rejection cases fail. Original CRT/UBSan .lib before
pins were incomplete; no complete linked-library identity claim follows.

## Actual bounded GREEN-r2

| Field | Actual result |
| --- | --- |
| Selected TUs | 50 |
| Actual compiles / exact cached | 41 / 9; each cache object from one intact RED proof, no mixed histories |
| Unique dependencies / consumed inputs | 248 / 332 |
| Exact CTests | ssa_core_model, ssa_exec_ir_owned_module, ssa_exec_ir_module_copy, ssa_gc_map_metadata_consumers:PASS |
| Configure/build/deps/CTest | Natural exit0; streams closed |
| Compile warnings | 0 in this increment; no full-build W0 |

Root freshly checks332 consumed inputs,50 objects and57 frozen objects/products;
independent review checks397 deduplicated current pins. Two unrelated unconsumed
foreign test files changed and remain preserved. No clean-HEAD native-build claim.

### Malformed and legal coverage

| Pointer | Count/capacity |
| --- | --- |
| Real two-header storage | 2/2 |
| Real two-header storage | 1/2 |
| Live header storage (Core: two; consumers: one) | 0/0 |
| NULL | 1/1 |
| NULL | 0/1 |
| Real storage | 2/1 |

Core GREEN:6rows × Clone/Verify × NULL/poison diagnostics =24calls, early refusal
before direct clone allocation, source/old destination stability and healthy retry.
Poisoned storage diagnostics check INVALID_ARGUMENT,actual function token and
zero remaining coordinates/version/hash fields.

Consumer Unity cases:validator rejection,projection rejection,absent,present-empty
and multi-entry projection =5cases. Each actual rejection entrance exercises all
six malformed rows with both diagnostic modes, unchanged input/owned old projection,
zero early guard allocations and healthy retry. Later projection OOM atomicity is
not claimed. The validator is metadata-only; no interpreter execution is called.

Legal absent/present-empty/multi-entry lifecycle checks independent header/three
inner pools, clone mutation, source teardown,repeated Free and continuing use of
the independent clone/projection. Stable tokens and same-identity local IDs hold;
cross-function remapping remains outside this fixture.

### Failure atomicity

Measured GC CloneFunction sweep:7OOM stations, source/keeper preservation,
candidate cleanup,balanced live ledger and retry. Eight diagnostic fields exact;
expectedVersion checked only within{1,2}, without an exact allocation-site oracle.
Owned-module sweeps61+61; module-copy88. These finite fixture sub-observations
do not add Unity cases.

## Auxiliary gates — actual finite acceptance

Root and independent reviews accept the following actual routes. Their receipts
bind raw output, configured argv, consumed source/header inputs and products;
older failed attempts retain their original failure status.

| Gate | Actual route and result |
| --- | --- |
| Primitive-r3 | LowerB; 23 TUs/4 roles; exact18-case CTest PASS; measured/completed allocation faults18/18; incremental W0 |
| TryCapture-r1 | Original EXE direct run; 3 TUs/one role; exact12-case protocol, 11 PASS plus one intentional prior-failure FAIL; acquired90/freed90/live0 |
| MSVC-r3 | 16 /Zs syntax phases; 12 unique C sources/10 roles; all natural exit0, closed streams, stderr0; warnings/errors/unknown0; finite syntax W0 only |

Primitive-r3 compiled five TUs initially, three directly and three during relink:
11 native compile events across eight unique TUs. Fifteen cached objects each
have one intact main-GREEN history proof; 20 unaffected objects were frozen
(15 cached plus five initially compiled). Final selected dependencies cover all
23 objects with 254 unique headers. The eight native phases exited0 with closed
streams and stderr0. All344 consumed pins and387 deduplicated current pins,
including23 objects/26 products, matched. Primitive-r1/r2 failures remain retained.

TryCapture-r1 actually compiled three unique TUs with zero cache reuse. Final
selected dependencies cover three objects and190 unique headers;253 consumed
pins and265 deduplicated current pins matched. Build/deps/direct natural exits
were0/0/1, with closed streams and stderr0. Root controller27943 completed
naturally0; original direct EXE49796 naturally1. Its exact28 nonempty lines
contain12 case/result pairs:11 PASS and intentional case11
`OWNER_TRY_CAPTURE_PRIOR_FAILURE`, followed by acquired90/freed90/live0.
Original raw/build/EXE evidence is retained. No CTest or permanent-wrapper
execution credit is granted for this protocol.

MSVC-r3 preserves role D/I/FI order and uses
/Zs /TC /std:c11 /experimental:c11atomics /W4 /utf-8 /MD /UNDEBUG.
Clang/UBSan/SDKoverride/output/warning-suppression flags are removed; actual
UTF-8 diagnostics are decoded strictly. Resource language2052 does not determine
the diagnostic codec. All16 phases naturally exited0 with closed streams and
stderr0. Raw output contains1462 include occurrences and149 unique headers;
405 consumed records (404 distinct identities) and438 deduplicated current pins
matched. Per-unit header counts are81,85,88,89,81,80,84,90,89,123,89,78,109,118,85,91.
MSVC-r1 failed preflight before any compiler phase. MSVC-r2 ran only its first
compiler successfully, then failed controller decoding; its failed receipt and
raw remain unchanged. MSVC-r3 grants finite syntax W0 only: no object/link/runtime,
second-compiler runtime, full-build W0 or Linux credit.

Evidence directory is the immutable task report directory stated above:

| Record | Bytes | SHA256 |
| --- | ---: | --- |
| aux-primitive-r3-receipt.json | 696236 | 5FE67526AC0CFDE4BC44C0BBA5D8B72289DD6AED6A0FCADB734BC92B7A8ED426 |
| aux-primitive-r3-functional-review.json | 2333 | B106237907D803CB28576CB72FF4F8F6B734110A132D8252BA867E7BD934EC04 |
| aux-try-capture-r1-receipt.json | 142551 | 6B7AC7C6C97BDCC2ADFC914085ADCAA606B769C7DA1F49A7CD8D67DFBAE14958 |
| aux-try-capture-r1-functional-review.json | 2342 | 328198352A68099FD01D1D20D791779F3BAF08519DED3CA405A51C6895A595BD |
| aux-msvc-r3-receipt.json | 694770 | BA85E7A2E1130759C61B404726687CF56C774D10D0A9E8640E4AFA477F23CE7A |
| aux-msvc-r3-functional-review.json | 2405 | 5E7AD022DF4455D3BEDC840957C7A53436F931B5435518546D9EA667470E5C74 |

Every cached object needs one intact source/argv/deps/object historical proof.
Missing proof permits only Root-authorized exact objects,original configured argv
recompile and same-target relink;mtime/arbitrary clean/mixed histories do not prove it.

## Candidate ownership and deferred exits

New consumer CMake supplies owner PRIVATE Core include support. Shared direct
registration contributes only the owned fresh-HEAD94-byte include append;foreign
prefix is excluded from the candidate. No clean-HEAD native build is claimed.

Whole-consumer admission,full GC payload hash,operation-specific cross-function
local-ID,liveness,reserved safepoint fields,collector/barrier,alias retirement,
artifact migration and complete GC semantics remain open. No VM/SourceExecute/
Oracle execution/FFI/network/security/Linux/full-build W0 completion is granted.

## Publication and lifecycle — PENDING

Commit/postcommit review and cleanup are PENDING. Actual released bytes,retained
evidence/reused builds and unknown ownership must come from Root lifecycle receipts.
Implementation or this document does not establish publication/cleanup completion.
