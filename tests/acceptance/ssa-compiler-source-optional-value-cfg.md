---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_receiver_guard.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir_optional.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_member.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_optional_value.inc
  - tests/parser/test_pre_semantic_ir_callable_isolation.inc
  - tests/parser/test_ownership_intrinsic_member_separation.c
status: partial
---

# SSA 01.02: nullable optional value merge

## Scope

A known value-producing `receiver?.method(arguments)` call now keeps its
canonical nullable result in the source-owned CFG. The receiver branch sends
the true edge through the present and invoke blocks and the false edge to a
dedicated absent block. The invoke keeps ordered normal and exception edges.
Only the normal continuation reaches the value merge; the exceptional
continuation remains the explicit zero-instruction propagation sink.

The compiler creates one temporary Place with the chain's canonical nullable
TypeId before branching. On the normal path it converts the call's non-null
return value to that nullable type and stores it. On the absent path it creates
a null constant carrying the same nullable TypeId and stores it into the same
Place. Both paths then enter the join, which loads one fresh merged ValueId.
These semantic-only merge instructions do not duplicate or reorder the
existing ExecBC merge-slot operations.

A single resolved `receiver?.field` access now enters the same present/absent
source CFG when the receiver is a local or parameter Place and the field has a
canonical symbol, an explicit non-null type annotation, and a primitive result
type. The present path projects the
field Place from that symbol, loads the non-null field value, and converts it
to the canonical nullable result type. The absent path stores a typed null;
the join loads the shared merge Place. This path has no invoke or exception
continuation.

Direct member calls use their bound receiver as the semantic typed-callee
operand. Runtime `argCount` also includes that hidden receiver, so the bridge
subtracts it before reading the remaining explicit argument slots. This keeps a
zero-argument member call from probing an unrelated stack slot and preserves
the order of real source arguments.

## Focused fixture

`test_source_optional_value_merges_present_and_absent_paths` compiles
`receiver?.read()` after an explicit nullable `wake(weak)`. It checks the exact
eight-block topology, including present, absent, join, invoke, normal,
exception, and exit blocks. It requires one call at the invoke tail, a nullable
`CONVERT` plus `STORE` on the normal path, a typed null `CONSTANT` plus `STORE`
on the absent path, one `LOAD` at the two-predecessor join, and the same Place
and TypeId for every merge operation. The built ExecIR must contain exactly one
`INVOKE` and one exception block.

`test_source_optional_field_value_merges_present_and_absent_paths` compiles
`receiver?.value` after an explicit nullable `wake(weak)`. It requires the
five-block entry, present, absent, join, and exit topology; ordered true/false
edges; one canonical field projection and load on the present path; the
nullable conversion and typed stores; and one merged join load. The field load
has a distinct non-null TypeId from the nullable conversion and join. The
projected Place must descend from the local receiver and identify the resolved
class field symbol. ExecIR construction must also succeed.

`test_unmodeled_optional_value_abandons_partial_source_cfg` keeps an optional
access with unknown field facts conservative. `test_optional_getter_value_keeps_conservative_source_cfg`
does the same for a known property getter.
`test_optional_unannotated_field_keeps_conservative_source_cfg` exercises a
resolved field without an explicit type annotation, and
`test_optional_field_chain_keeps_conservative_source_cfg` exercises a longer
member chain. Each starts with a source `if` graph, then requires a legacy
two-block graph with no synthetic branch after the unsupported access.

## Validation evidence (2026-09-18)

- MSVC 19.44.35228, WSL GCC 11.4.0, and WSL Clang 14.0.0 each rebuilt and
  passed the focused pre-execution Semantic IR suite 26/26.
- The adjacent `ssa_builder_cfg`, `ssa_builder_dominance`,
  `ssa_builder_control_edges`, `ssa_builder_fact_identity`,
  `ssa_place_eligibility`, `ssa_place_promotion`, and
  `ssa_value_validation` gate passed 7/7 on all three toolchains.
- The receiver-guard performance executable passed its one test on all three
  toolchains.
- WSL GCC ASan+UBSan passed the focused suite 26/26 with leak detection and
  both sanitizers configured to halt on the first error.
- The broader MSVC ownership-intrinsic executable retained the same four
  pre-existing failures. Its nullable optional argument-skipping regression
  passed, so those failures are not attributed to this checkpoint.

## Validation evidence (2026-09-27)

The new field fixture was added before the producer change. For a quick RED
localization, `wsl.exe -- python3 /mnt/d/tmp/zr_vm/ssa-optional-member-gcc/red-manual.py`
compiled the current test source to D but linked it against the older
`/home/hejiahui/zrvm-ssa-nested-gcc.4pVemu` parser library. It reported
`110 Tests 1 Failures 0 Ignored`: the new field fixture expected an active
source CFG and received false. This mixed-library run is RED evidence only; it
does not count as the final rebuilt GREEN gate.

The complete current-source GCC target build used
`cmake --build /mnt/d/tmp/zr_vm/ssa-optional-member-gcc --target zr_vm_pre_semantic_ir_test -j 2`
and finished all 584 initial build edges. The direct Unity executable then
reported `113 Tests 0 Failures 0 Ignored`. After removing a newly introduced
test-helper qualifier warning, the 27-edge incremental rebuild and direct
Unity rerun again reported `113 Tests 0 Failures 0 Ignored`. This target has no
CTest registration in `tests/CMakeLists.txt`, so the direct Unity result is its
test gate.

The separate current-source Clang 14 target build used
`cmake --build /mnt/d/tmp/zr_vm/ssa-optional-member-clang --target zr_vm_pre_semantic_ir_test -j 2`
and finished all 847 build edges. Its direct Unity executable also reported
`113 Tests 0 Failures 0 Ignored` with exit status zero.

The native MSVC 19.44 smoke configured a separate D cache through
`VsDevCmd.bat`, then built
`cmake --build D:\tmp\zr_vm\ssa-optional-member-msvc --target zr_vm_parser_shared -j 2`.
Ninja finished 775 build edges and linked `zr_vm_parser.dll` with exit status
zero. This is a parser/shared build smoke, not an MSVC run of the Unity target.

The adjacent GCC targets `zr_vm_ssa_source_cleanup_cfg_test`,
`zr_vm_ownership_receiver_guard_performance_test`, and
`zr_vm_ownership_intrinsic_member_separation_test` built together in 42 edges.
`ctest --test-dir /mnt/d/tmp/zr_vm/ssa-optional-member-gcc -R ^ssa_source_cleanup_cfg$ --output-on-failure`
passed 1/1; the receiver guard performance executable passed 1/1. The broader
ownership executable remains failing at `53 Tests 5 Failures` in
`test_weak_receiver_guard_releases_wake_on_suffix_throw`,
`test_live_weak_optional_chain_survives_native_gc_pressure`,
`test_live_weak_missing_member_is_not_null_reference_error`,
`test_weak_optional_intrinsic_named_members_use_normal_dispatch`, and
`test_weak_direct_wake_named_member_uses_normal_dispatch`. Its optional field
chain and getter cases pass.
The full failure output is saved in
`D:/tmp/zr_vm/ssa-optional-member-gcc/ownership-regression.log`.
The suffix-throw case observed `Expected 1 Was 0`; the GC-pressure case failed
to compile with `Failed to prepare semantic optional result merge`; the
missing-member case failed to compile with `Unknown static member
'Service.missing'`; and both named-member dispatch cases observed `Expected 3
Was 1`.

Each of those five failing sources uses a method call or direct member access.
The new receiver-guard predicate requires `ZR_RECEIVER_GUARD_NULL`, one
noncomputed class-field segment, and a nullable result. The new producer call
also requires `ZR_POSTFIX_ACCESS_OPTIONAL`, `members->count == 1U`, and
`!nextIsFunctionCall`. Existing call chains short-circuit through the original
call predicate, and Weak or direct member access cannot satisfy the new field
predicate. These failures do not exercise the new field producer; the broader
ownership suite is recorded as failing, not treated as a passing gate for this
checkpoint.

## Callable SemanticIR isolation follow-up (2026-09-27)

The GC-pressure failure happened during compilation, before GC ran. The
optional result merge in `run` requested temporary slot 5 while the entry
sidecar still held slot 5 from the earlier `readAfterGc` method body. Function
and class-member compilers reset their ExecBC stack slot counters when entering
a callable, but previously left pre-semantic slot facts in the caller's
sidecar. Both callable compilation paths now use the existing SemanticIR
isolation helper around the full callable body. Closure analysis still reads
the saved parent sidecar for captured-local identity.

A new focused fixture first failed on a current-source GCC build: direct
`zr_vm_pre_semantic_ir_test` reported `114 Tests 1 Failures`, with the class
member compilation changing the parent slot count from 0 to 6. The GCC build
used `D:/tmp/zr_vm/ssa-optional-member-gcc`; ABI/schema/CMake input hashes
were checked stable across the RED run. After the repair, the fixture also
seeded nonempty parent SemanticIR facts and checked that class-member and
standalone function compilation left its slots, Places, Values, and
instructions unchanged. The rebuilt direct Unity run passed `114 Tests 0
Failures`.

The same current-source GCC ownership executable now reports `53 Tests 4
Failures` (direct process exit 4). The GC-pressure case passes. Its four
remaining failures are `test_weak_receiver_guard_releases_wake_on_suffix_throw`
(`Expected 1 Was 0`), `test_live_weak_missing_member_is_not_null_reference_error`
(`Expected Non-NULL`), and the named-member dispatch cases
`test_weak_optional_intrinsic_named_members_use_normal_dispatch` and
`test_weak_direct_wake_named_member_uses_normal_dispatch` (both `Expected 3
Was 1`). The adjacent `zr_vm_ssa_source_cleanup_cfg_test` passed 52/52 and
`zr_vm_ownership_receiver_guard_performance_test` passed 1/1. GCC direct logs
are `D:/tmp/zr_vm/ssa-optional-member-gcc/red-unity.log`,
`final-semantic.log`, `final-ownership.log`, `adjacent-cleanup.log`, and
`adjacent-performance.log`.

The current-source Clang 14 artifact cache built both focused executables in
14 incremental edges. Direct Unity results were 114/114 for pre-semantic IR
and 53/4 for ownership (exit 4), with the GC-pressure case passing and the
same four remaining failures as GCC. The Clang logs are
`D:/tmp/zr_vm/ssa-artifact-v6-clang/callable-isolation-*.log`.

The MSVC 19.44 artifact cache had already compiled the current parser static
library. A direct `zr_vm_parser_static` build reported no work; the native
pre-semantic IR target then built in seven edges and passed 114/114. A parallel
suffix-throw fixture edit in the shared worktree split that case into direct
and optional tests before the MSVC ownership target was built. That transient
native run reported 54 tests and five failures (exit 5): both suffix-throw
cases observed `Expected 1 Was 0`; the missing-member and two named-dispatch
failures matched GCC/Clang. The GC-pressure case passed. These are native runs
against the static parser build, not a shared-library smoke. The MSVC logs are
`D:/tmp/zr_vm/ssa-artifact-v6-msvc/callable-isolation-*.log`.

## Boundary

This checkpoint covers nullable guards around known member calls with complete
canonical receiver, symbol, result-type, and explicit-argument facts, plus one
resolved member-only access to an explicitly typed, non-null primitive class
field through a local or
parameter receiver Place. Missing or incomplete canonical facts still abandon
an active partial graph. Unannotated or nullable fields, property getters,
computed or chained member access,
Weak field guards, cleanup edges, edge-defined exception payloads and enclosing
handlers, and non-call exceptional operations remain open for the full 01.02
exit gate. The large `compile_expression_types.c` adds only a receiver snapshot
and orchestration call; field projection and value production live in
`compiler_semantic_ir_optional.c`. No ExecBC opcode is inspected to reconstruct
source control flow. Resolved general-call CFG startup is covered by
[the follow-up checkpoint](ssa-compiler-source-general-call-cfg.md).
