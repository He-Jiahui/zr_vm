---
related_code:
  - tests/parser/test_ssa_source_aot_owned_module.c
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h
  - zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-execbc-vm.cmake
implementation_files:
  - zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h
  - zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c
plan_sources:
  - .codex/plans/20261007-ssa-core-owned-module.md
  - .codex/plans/20261006-ssa-core-scalar-owner-design-v2.md
tests:
  - tests/parser/test_ssa_source_aot_owned_module.c
  - tests/parser/test_ssa_source_aot_scalar_text.c
  - tests/parser/test_ssa_source_aot_descriptor.c
doc_type: testing-guide
status: finite-core-owner-genuine-red
---

# Independent Core owner lifetime test guide

## Invocation inventory

The actual test source has 677 lines. Its admitted branch requires Windows64,
_MSC_VER and x64/AMD64, excluding ARM64/ARM64EC. These counts describe the authored
inventory. Actual r1 passed the prerequisite mode and established isolated
genuine feature RED; the no-argument ten-case inventory remains unproved.

| Invocation | Admitted-host inventory | Current expected phase |
| --- | --- | --- |
| --prerequisites-only | Two actual 9/8 descriptor/ABI/pure-buffer cases | Actual r1: 2 PASS, exit 0 |
| --features-only | Two actual 9/8 producer-lifetime cases | Actual r1: 2 FAIL only Clone-success assertion, exit 2 |
| No arguments | Ten cases: two prerequisites and eight ownership cases | Later functional acceptance, currently unproved |
| Other arguments | Returns 2 before Unity execution | Invalid invocation |
| Other actual host, including either recognized mode | One actual target UNSUPPORTED case | Host refusal; never substitute lifetime RED |

The eight ownership cases are 9 and 8 surviving all producers, owner-to-owner
survival after first Free, exact self-source replacement, actual 9-to-8
replacement, actual opcode-category error atomicity, actual contract error
atomicity, and required arguments/NULL-safe Free. The final case also exercises
owned C/LLVM buffer boundaries. Current stub returns UNSUPPORTED, so the lifetime
assertion expects 0 and observes 14. Root's actual r1 logs establish that exact
failure for both literal cases, with prerequisite mode passing.

## Producer chain and lifetime proof

The shared fixture builds real finalized literal SCRIPT metadata and compiler
state. The test binds the actual module, verifies/compacts the source graph,
checks retained source maps and three CONSTANT/NOP/RETURN instructions, creates
the actual host primitive layout, attaches the frame, lowers canonical callable
facts and constructs the real projection descriptor. It asserts actual ownership
bindings, target, frame/value slot facts, literal and present-empty state map.
No replacement graph or source-field clearing makes the consumer pass.

Before Clone, witness records Core module validity, no-args-i64 ABI, semantic hash,
function identity, literal, target, contracts and logical-map identity/generation.
It contains values only. Independent-copy comparison, while both source/copy are
live, traverses every schema array and optional map header. Nonempty spans must
have independent addresses and equal live bytes. Empty spans become NULL; map
capacities equal copied counts; present-empty headers remain present.

Successful own_source then releases descriptor, projection, compacted function,
fixture/compiler and runtime state in that order, clears freed globals, and only
then validates/ABI-checks/emits through OwnedModuleView. The whole C/LLVM formatter
grammar is consumed, checking actual function ID, decimal 9/8, NUL, length and
end-of-buffer. Generated text is never executed. Unity teardown owns all resource
slots before assertions, so aborting an assertion can clean up safely.

Owner-to-owner cloning uses NULL diagnostic, frees the first owner and consumes
the second. Exact self-source saves integer address witnesses, requires distinct
module/function/instruction addresses after replacement and never dereferences
old storage. Replacement constructs a fresh actual 8 producer after retaining 9.

The opcode-category refusal sets a bit outside the actual known instruction flag
mask; it does not replace the instruction's opcode. Contract refusal perturbs the
actual function contract signatureHash. Each calls existing Core validation and
Clone, restores both fields before assertions, and compares exact diagnostics,
source semantic hash, old owner/view identity and live-storage observation.
The observation includes pointer bytes solely to detect mutation; it is not a
new ABI or module hash.

Buffer checks cover insufficient capacity and adjacent sentinel preservation,
exact required bytes plus NUL, NULL output/length, zero capacity, diagnostic status
and owner observation stability. Required Clone arguments preserve a preexisting
owner. View(NULL), Free(NULL), repeated empty Free and Free followed by View(NULL)
are explicit assertions, although this combined case currently cannot reach them
because initial Clone is unsupported.

## Build registration and evidence interpretation

Build target: `zr_vm_ssa_source_aot_owned_module_test`; CTest name:
`ssa_source_aot_owned_module`. The direct validation registration compiles the real
test and shared fixture, links the existing five-TU scalar buffer support and
Core's actual glob-collected owner TU, and records source/settings/header metadata.
Normal registration uses `zr_vm_link_parser_core_plus_library`, labels `ssa;aot`,
timeout 120 and MSVC stack 8388608. Avoid a duplicate owner object in the test.

Root owns all commands and finite Git commits. Sources must remain direct
`E:/Git/zr_vm` inputs. Existing build output is
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`;
reports are `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`, with temporary
drivers under `E:/cargo-targets/zr_vm/tmp`. No snapshot, copy or worktree is part
of the procedure. Inspect actual source membership, settings, header hashes and
compile_commands; registration alone is not proof that those inputs were compiled.

Genuine RED requires actual configure/build exit 0, prerequisite 2/2 pass exit 0
and isolated features 2/2 fail exit 2 solely at the Clone assertion. Retain actual
logs, native terminal states, hashes and receipt. Missing symbol/build errors,
failed prerequisites and unsupported-host refusal invalidate that interpretation.
[Acceptance](../../docs/acceptance/ssa-aot-owned-module.md) records actual r1:
configure PID38688/build PID6876/prerequisite PID6504 exited 0, features PID37064
exited 2, and Root PID86154 naturally terminated with observed exit 0. Warnings
were zero and no UBSan diagnostic was observed. The final RED receipt SHA-256 is
`5AC6E5B9FFA2CC4304428C719F9EAE346429F096A6DF39619A6CB8FB4F969501`.
No new run was made for this guide. Earlier scalar-text GREEN is a prerequisite,
not owner GREEN; default10 and successful ownership remain OPEN.

## Subsequent finite tests and limits

Allocator/OOM and direct arithmetic-helper tests are not in these ten cases.
A subsequent production Clone phase must append the OOM status/string. The following
separate fault/arithmetic gate must compile the real owner TU in an independent
OBJECT target with test-local allocator redirection, and expose
only a private test wrapper calling actual checked geometry. Verify exclusive
owner symbol resolution; do not include production .c text or duplicate formulas.
Expected arithmetic cases include exact SIZE_MAX fit, multiplication, padding and
addition overflow, zero-count spans and invalid alignment without enormous fake
arrays. OOM initial/replacement/self-source attempts require one attempted arena
allocation, unchanged old owner, exact diagnostics, retry and exactly-once Free.
These are future expectations, not observed results or fullmatrix credit.

Real finite sources have an empty logical map and no GC header. Inventory loops
do not prove nonempty GC/deopt/CFG or general multi-function acceptance. Linux,
full MSVC, native, normal returned-program retention and full47 remain OPEN.
SourceExecute/oracle/generated-native/loader/network/FFI/provider/security
execution is outside this finite procedure. Projection remains non-runnable.
