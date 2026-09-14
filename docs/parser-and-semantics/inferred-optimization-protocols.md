---
related_code:
  - zr_vm_parser/include/zr_vm_parser/optimization_facts.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_protocols.c
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/iteration_contract.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_receiver_effect.c
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - tests/parser/test_ssa_inferred_protocols.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/optimization_facts.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_protocols.c
plan_sources:
  - docs/plans/ssa/09-language-simd/01-inferred-protocols.md
  - docs/zr_language_specification.md
  - "user: 2026-09-13 implement the 09.01 inferred optimization protocol stage"
  - lua/rust/tests/ui/borrowck/borrowck-for-loop-head-linkage.rs
  - lua/rust/tests/ui/borrowck/borrowck-loan-rcvr.rs
  - lua/cpython/Lib/test/test_capi/test_opt.py
  - lua/cpython/Lib/test/test_xml_etree.py
tests:
  - tests/parser/test_ssa_inferred_protocols.c
doc_type: module-detail
status: implemented
---

# Inferred optimization facts and capability protocols

## Purpose

`optimization_facts.h` is the parser-owned summary exchanged by type,
ownership, task-effect, layout, and ExecIR producers.  It deliberately carries
only fixed-width integers: no AST node, string, runtime pointer, or host
address crosses the boundary.  Later optimization and backend stages consume
the summary instead of guessing from a container or callable name.

The contract has three outcomes:

- `ZR_OPTIMIZATION_FACTS_PROVEN` permits only the transformations justified by
  the published bits and hashes.
- `ZR_OPTIMIZATION_FACTS_UNKNOWN` is a valid source program, but a missing or
  stale proof.  The caller must retain the baseline path and may report the
  `unknownReason` as a missed-optimization remark.
- `ZR_OPTIMIZATION_FACTS_INVALID_LANGUAGE_USE` is reserved for a semantic
  error.  In particular, a borrow that is live across suspension is rejected
  with `ZR_EXEC_IR_DIAGNOSTIC_BORROWED_ACROSS_SUSPEND`; it is not downgraded to
  a missed optimization.

## Fact production

`SZrOptimizationFactQuery` accepts scalar observations from canonical parser
producers.  Existing effect bits (`READ_MEMORY`, `WRITE_MEMORY`, `ALLOCATE`,
`THROW`, and `SUSPEND`) remain the execution-effect vocabulary.  The query also
accepts:

- receiver state (`UNKNOWN`, `READONLY`, or `MUTABLE`), deriving
  `RECEIVER_READONLY` only for an explicitly readonly, non-mutating receiver;
- borrow state (`UNKNOWN`, `STABLE`, `ESCAPES`, or `ACROSS_SUSPEND`), deriving
  `BORROW_SAFE` only for a stable borrow with no suspension overlap;
- task effects (suspend, spawn, cancel, detach, external, and borrow escape)
  plus an explicit `observedTaskEffectsKnown` bit.  A known empty task mask
  derives `TASK_SEND_SYNC`; an absent contract is not treated as empty;
- canonical type capability masks containing both `SEND` and `SYNC` derive the
  type-level `SEND_SYNC` proof.  Out-of-contract ownership bits keep the result
  unknown rather than being guessed as safe;
- `SEND_SYNC` remains a type/ownership publication proof (supplied directly by
  a producer or derived from canonical `SEND|SYNC` capabilities); it is
  intentionally not inferred merely from an empty task-effect mask.
  `TASK_SEND_SYNC` is the distinct task-boundary proof;
- ownership, protocol kind/operation, module, layout, type-token, and
  generation identities.

The initializer marks ordinary parser effects as complete.  Native/plugin
producers with no effect contract must set `observedExternalEffectsKnown` to
zero or `observedExternalEffectsUnknown` to one.  Such a query remains
`UNKNOWN_EXTERNAL_EFFECTS` even when its observed effect mask is zero, so an
external call can never become pure by omission.

The initializer likewise marks task effects as complete for the checked
parser/task analysis.  A producer that cannot publish task effects must clear
`observedTaskEffectsKnown` (or set `observedTaskEffectsUnknown`); an empty mask
without that proof remains `UNKNOWN_TASK_EFFECT` and never implies
`TASK_SEND_SYNC`.

The query checks invalid language use first, then unknown external/task/borrow/
receiver evidence, forbidden effects, identity mismatches, and finally missing
required fact bits.  Missing readonly, borrow, task, and immutable/persistent
bits receive dedicated reasons (`UNKNOWN_RECEIVER`, `UNKNOWN_BORROW`,
`UNKNOWN_TASK_EFFECT`, and `UNKNOWN_IMMUTABILITY`); all other missing bits use
`UNKNOWN_UNSUPPORTED`.  Every result gets a deterministic proof hash.  The
hash excludes its own witness field, so `ZrParser_OptimizationFacts_Validate`
can detect a tampered/stale witness without creating a self-referential hash.
Optional function/block/instruction/source ids on the query are copied to a
diagnostic, allowing a missed-proof or contract mismatch to point back to the
parser/IR location without retaining an AST pointer.  The same ids are copied
into the facts' proof scope and included in its hash, so a serialized summary
retains the region in which its evidence was produced.

If a candidate negative bit conflicts with a known observation (for example
`NO_ALLOC` together with `ALLOCATE`), the query downgrades the result to an
`UNKNOWN_*` optimization miss.  Only a caller that directly publishes a
`PROVEN` contradictory record is rejected by the validator.

Validation rejects unknown masks, contradictory negative effects on a
`PROVEN` record (for example `NO_ALLOC` with `ALLOCATE`, `PURITY` with
write/throw/suspend), impossible receiver/borrow/task combinations, invalid
enum values, and a non-matching non-zero proof hash.  An `UNKNOWN` record may
retain contradictory raw observations for diagnostics, but its validity keeps
them out of optimization consumers.  Malformed producer records are distinct
from ordinary optimization misses.

## Capability protocols

`SZrOptimizationProtocol` describes contiguous views, typed arrays, batch
operations, iteration, immutable updates, and persistent updates.  Operation
bits include load/store/slice/iterate/batch/update, plus `NEXT`, `BORROW`, and
`READONLY` for richer iterator and view contracts.  A protocol can require
fact bits and declare execution effects; its stable identity is the
`protocolToken`, kind, capability set, signature hash, and (when requested)
layout/module/generation identity.

`ZrParser_Optimization_RecognizeProtocol` treats the provider capability set as
an available superset: required operations and facts must be present, while
effects must match exactly.  Signature, layout, module, and generation
mismatches produce their corresponding execution diagnostic.  `elementTypeToken`
is descriptive metadata and is intentionally not compared, allowing unrelated
source type names to implement one protocol.  A generic element/layout
distinction that matters for safety belongs in the signature or layout hash,
not in a string/type-name branch.
The deterministic protocol hash still covers every scalar field (including the
descriptive element token) when a caller wants an exact serialized witness;
recognition itself uses the capability identity rules above.

## Design rationale and evidence

The API reuses the repository's canonical receiver effect, ownership/loan
analysis, iterator protocol, and execution-effect masks; it does not add a
second AST or runtime dispatch path.  This follows the same safety split seen
in Rust borrow-checker regressions (immutable and mutable loans remain distinct
and invalid overlap is an error) and in CPython's iterator specialization
tests (iteration is a capability of the object protocol rather than a concrete
container spelling).  CPython's immutable-type tests likewise support keeping
immutability as an explicit capability rather than assuming that a shared
handle's internals are `Send`/`Sync`.

The deliberate `zr_vm` divergence is that immutable/persistent and task
effects are represented in a compact, serializable parser summary so ExecIR
can make a conservative optimization decision without retaining source
objects.  Unknown evidence never changes language semantics; it only disables
the optimization.

## Test coverage

`tests/parser/test_ssa_inferred_protocols.c` is a standalone focused fixture
and is registered in the SSA CMake layer by the parent integration task.  It
covers:

- missing facts and known readonly/borrow/task evidence;
- mutable receivers and escaping borrows as conservative unknowns;
- borrow-across-suspend as a precise language diagnostic;
- missing external/task effects not becoming pure;
- capability-based contiguous, iteration, and persistent protocol matching;
- layout mismatch diagnostics, generation staleness, proof-hash tampering,
  and malformed mask rejection.

The source-level task validator, receiver-call tests, and iterator contract
remain the producers of language semantics.  This stage does not add syntax,
runtime allocation, or native ABI behavior; it provides their scalar summary
and leaves shared CMake/umbrella registration to the milestone owner.

## Validation evidence

In WSL (GCC 11.4 and the installed Clang), the focused fixture was compiled
with `-std=c11 -Wall -Wextra -Wpedantic -Werror` and executed successfully.
The same fixture passed GCC and Clang AddressSanitizer/UndefinedBehaviorSanitizer
runs (`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`), plus `clang --analyze`
and GCC `-fanalyzer`.  The parser static library also rebuilt successfully
with `cmake --build build/ssa-gcc-debug --target zr_vm_parser_static -j 4`.
The dedicated CTest target is intentionally registered by the parent SSA CMake
integration; no shared build/index file is changed in this stage.

The query/validation entry points allocate no storage and are re-entrant for
independent records; repeated calls overwrite only the supplied facts and
diagnostic on valid input.  There is therefore no partial-allocation or OOM
rollback path in this scalar contract.
