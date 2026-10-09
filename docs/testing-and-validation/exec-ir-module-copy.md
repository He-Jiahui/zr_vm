---
related_code:
  - tests/core/test_ssa_exec_ir_module_copy.c
  - tests/core/support/ssa_exec_ir_module_copy_fixture.h
  - tests/core/support/ssa_exec_ir_module_copy_fixture.c
  - zr_vm_core/include/zr_vm_core/exec_ir_module_copy.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/core/support/ssa_exec_ir_owned_module_shim.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
plan_sources:
  - .codex/plans/20261009-ssa-module-final-function-copy-design-v1.md
tests:
  - tests/core/test_ssa_exec_ir_module_copy.c
  - tests/core/support/ssa_exec_ir_module_copy_fixture.h
  - tests/core/support/ssa_exec_ir_module_copy_fixture.c
  - tests/core/test_ssa_exec_ir_owned_module.c
  - docs/acceptance/exec-ir-module-copy.md
doc_type: testing-guide
status: green-observed-rootproof-commit-pending
---

# Whole-module copy finite validation design

## Twenty cases and actual finite GREEN

Actual GREENr1 observes20PASS/0FAIL/0ignored. Historical REDr1 has3 guard PASS/
17 exact feature FAIL and is preserved in acceptance. Main/fixture paths are
registered above. The actual constructed profile uses two fixed one-row functions
with equal replacement geometry; these twenty cases do not cover arbitrary
source graphs or storage geometry.

| Case | Required observation |
| --- | --- |
| 1 | Individual/combined NULL arguments reset nine fields and preserve old output |
| 2 | NULL diagnostic supports success and refusal |
| 3 | Zero/out-of-range selectedId gives exact32 count/id payload before malformed source storage |
| 4 | Exact source==destination refusal preserves source and owning records |
| 5 | Two-function module: selected replacement and unselected/module pools preserved |
| 6 | Nonempty binding/source/GC/state/deopt/frame storage copied independently |
| 7 | Optional present-empty headers and clone capacity normalization |
| 8 | Replacement borrowed from selected source slot |
| 9 | Replacement borrowed from old destination; old records released once after reads |
| 10 | Candidate/replacement id, token, function signature mismatches and precedence |
| 11 | Every immutable contract mismatch and simultaneous mismatch order8–18 |
| 12 | Changed function frame/layoutHash copies; module layoutHash remains source value |
| 13 | Storage-valid non-admitted fixture copies with equal unknown/reserved/noncurrent values |
| 14 | Malformed source storage preserves actual CloneModule diagnostic |
| 15 | Malformed replacement/binding/nested StateMap preserves actual CloneFunction diagnostic; earlier identity wins |
| 16 | Release source/replacement producers; output witness remains independent |
| 17 | Two independent outputs; free one without changing the other |
| 18 | Repeated replacement releases old output once; final FreeModule idempotent |
| 19 | Every measured composite allocation failure preserves inputs/output, cleans candidate and retries |
| 20 | Empty and populated old output balance, with cleanup checks before recovery |

Parameter loops within a case do not create additional Unity cases. Poison all
nine diagnostic fields at entry; compare inactive fields as well as active code,
token, location and32/64 payload direction. Equal layoutHash is not required.
Direct lower diagnostics must come from the corresponding real clone input and
phase, not a synthesized expected OOM record.

## Logical witness and lifetime checks

The actual constructed profile has at most two fixed one-row functions,
equal source/replacement geometry,4096 witness words and256 address/record slots.
Present-empty checks cover actual GC/state/frame header presence, empty counts,
StateMap count/capacity equality and value/instruction capacity>=live count.
These limited observations do not claim arbitrary geometry or spare bytes.
Preflight complete typed headers, live lengths and witness budget before capture.
Compare logical headers/counts/presence and every live row across module and
function storage. Do not compare padding, spare bytes or universal capacities.
Alias/liveness checks occur while both copied and producer storage are live.
After release use only pointer-free numeric records and a surviving output view;
never dereference a stale source/old-destination borrow.

The candidate includes all partially cloned functions in rollback through the
existing CloneModule mechanism. Failed replacement clone must free candidate
storage before fallback teardown, while producer and old output records remain
live. Disarm faults before cleanup/assertions/retry. Recovery records an error;
it cannot turn incomplete cleanup into PASS. Deferred observers cannot Unity
longjmp inside production clones or callbacks.

## Strict allocator closure and independent denominator

Use the existing bounded typed ExecIR allocator ledger and paired allocation/
free shim across fifteen Core clone/storage TUs plus the new module-copy TU.
Actual REDr1 and GREENr1 use small-direct all-OBJECT closure:41 configured TUs/41 objects
and6 unique provider rows. It has no residual archive, Parser or vendor-body
compilation. Normal-project/Parser integration and future residual exclusions
are separate OPEN gates. No broad ordinary-library redirection or unknown-free
exemption is part of this finite gate.

Measure actual composite allocation count and the boundary between module clone
and replacement clone. Generic/Function61-site counts are separate prerequisites
and cannot be reused or summed into this denominator. Selected candidate storage
is released during successful replacement; output-live records alone cannot
measure all transient allocations.

For each measured site require exact lower diagnostic witness, source/replacement/
old-output logical preservation, exact candidate allocation/free balance, no
hidden bookkeeping allocation, and disarmed clean retry. Both empty/populated
output lifecycle paths must end with balanced live/free/error records before
fallback or reset. Actual GREENr1 independently measures module60/replacement28/composite88
allocations and completes88 injected faults with direct lower geometry/diagnostic
comparison and disarmed retry. This is one Unity case within20, not88 cases.
The equality60+28=88 is observed for this exact profile, not a general count.

## Required evidence and open gates

Root owns builds, tests, registrations, current source/header/object/map pins and
commits. Genuine functional RED must fail at feature assertions after real lower
prerequisites; later independent GREEN must pass the full twenty-case inventory,
measured composite sweeps and lower support suites. Native phase PIDs/natural
exits, raw logs, warning/sanitizer/ledger diagnostics and finite compiler receipts
must be actual evidence. Compile-only checks cannot prove runtime behavior.

Actual GREENr1 raw logs match receipt, all five phases terminate naturally0 and
contain no warning/ledger/UBSan diagnostic. CoreModel and generic14 PASS as
separate support. Root current proof and GREEN commit remain pending. Historical
finite MSVC REDr1 has7 zero exits/warnings, compile-only. Actual GREENr1 also
completes7/7 natural0/zero warnings with matching raw-log/object hashes; this
remains compile-only.
Full MSVC/Linux, normal runtime/backend parity, default canonical output,
source-family integration, registry lifetime and all47 remain OPEN.

- [Storage and diagnostic contract](../core-runtime/exec-ir-module-copy.md)
- [Pending acceptance record](../acceptance/exec-ir-module-copy.md)
