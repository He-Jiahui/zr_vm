---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_gc_map_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_fusion_contract.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_container_specialize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects_linear.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - zr_vm_core/src/zr_vm_core/object/container_storage_contract.c
  - tests/parser/test_ssa_gc_map_parser_consumers.c
  - tests/parser/support/ssa_gc_map_parser_consumers_fixture.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/core/support/ssa_exec_ir_owned_module_shim.h
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/cmake/ssa-gc-map-parser-consumers.cmake
  - tests/cmake/ssa-execir-owned-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_gc_map_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_fusion_contract.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_container_specialize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
plan_sources:
  - .codex/plans/20261016-ssa-gc-map-parser-consumers.md
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
tests:
  - tests/parser/test_ssa_gc_map_parser_consumers.c
  - tests/parser/support/ssa_gc_map_parser_consumers_fixture.h
  - tests/cmake/ssa-gc-map-parser-consumers.cmake
doc_type: testing-guide
status: validation-accepted
---

# Parser GC-map consumer guards

## Validation checkpoint: 2026-10-17

This page records the 2026-10-17 validation checkpoint. The actual baseline RED,
four minimal production edits, GREEN configure/build, and 99-case GREEN direct
test have been accepted. GREEN and the finite seven-TU MSVC syntax result have
Root and independent acceptance. Commit and cleanup results are recorded in
this task's `finite-close.json` and the [SSA progress ledger](../plans/ssa/index.md).
The complete **47-leaf** SSA goal remains active.

Actual source is E:/Git/zr_vm. The reused physical LowerB configuration is
E:/cargo-targets/zr_vm/build/ssa-20261009-01a0fe2b-execir-owner. Evidence is retained
under E:/cargo-targets/zr_vm/reports/ssa-gc-map-parser-consumers-01a0fe2b/.
No source tree, module copy, or snapshot is used.

| Gate | Validation observation or record location |
| --- | --- |
| Baseline RED | 99 independent Unity cases; 21 legal PASS, 58 malformed FAIL, 20 malformed PASS, 0 ignored; all 78 malformed observations present. |
| RED process | Direct executable naturally exited 58, matching Unity.TestFailures; Root controller naturally exited 0 after validating the expected RED protocol; streams closed, stderr 0, UBSan reports 0. |
| Production | Four storage predicates now call the existing Core private outer-shape predicate; necessary includes added; independent static review accepted. |
| GREEN configure | 30 unique TUs in roles 15/12/3; argv/link/roles equal actual RED; source pin differences are only the reviewed four files. Configure evidence grants no build/test credit. |
| GREEN build | 30 unique TUs in roles 15/12/3; 60 actual compile events (30 direct + 30 same-target Ninja), 219 unique actual headers, matching BEFORE input pins, frozen nonselected objects, 0 cache credit. All 33 phases naturally exited 0 with closed streams, empty stderr, and no warnings/errors/UBSan reports. |
| GREEN direct test | 99 PASS = 21 legal + 78 malformed; 0 FAIL/ignored, all 78 malformed observations present. All malformed inputs reject before allocation with owner/state/diagnostic checks passing. 325 actual consumed inputs admitted; executable and controller naturally exited 0, streams closed, stderr 0, UBSan reports 0. Root and independent review accepted. |
| Finite MSVC | Root and independent review accepted r2: 8 natural exit-0 phases including 7 syntax checks, 7 unique C TUs in 2 roles, 100 actual headers and 107 consumed inputs. Warnings/errors/unknown output 0; streams closed, stderr empty. W0 applies only to this finite syntax scope. |
| Commit and cleanup records | See this task's `finite-close.json` and the [SSA progress ledger](../plans/ssa/index.md); this page records the validation checkpoint. |

The actual RED fail counts by entrance are 4/8/8/8/10/10/10 in the order below.
Some malformed rows were already rejected by the weak guards. Neither all 78
rows failing RED nor an artificial out-of-memory failure is required or claimed.

GREEN test-r1 stopped before native execution when a broad candidate inventory
pin changed in an unrelated header; it is retained as an admission failure, not
a functional test result. The accepted r2 checks the 325 actually consumed inputs
and retains the historical candidate inventory. MSVC-r1 likewise stopped before
native execution during overly broad repository-root candidate discovery; its
failure evidence is retained. The r2 discovery scope uses the actual common,
Core, Parser, and tests roots plus the fixed MSVC/SDK roots.

### Retained evidence identities

| Same-task report | Bytes | SHA256 |
| --- | ---: | --- |
| red-r1-functional-review.json | 1973 | 35F0BC96642F3ECDA924649C2C9449CAF71BB0B9939B8A9FAD74B77708A827CD |
| native-red-test-r1-receipt.json | 50849 | 1EC54D2EA4E095BCDAAF20ED05DA3CF1D1762078436E9FF902A1F3644EFC9135 |
| production-edit-receipt.json | 9574 | 54C253195AC5D6649C7E425E6AC8FB27E38F6E775BF78CFA33718E9836415F99 |
| production-independent-review.json | 1284 | 80C3B3077DA3C3EC4190AD115A9F18C0732FC79544785A96773DA1FB31CCA9A8 |
| configure-green-r1-root-review.json | 1454 | 5B1DEAE5D29B0864793CA69124BFF3AC4B51513ACD75F90B62152C89A7C9833F |
| native-green-configure-r1-receipt.json | 116157 | F783BC4AB834D7563F1342A2939B7126E3E399373D5DF3EBDEEE2DF7E8363BA3 |
| native-green-build-r1-receipt.json | 866629 | 76EF6EA396CAC7A5E1EBCAE707B52D53AC0D63F29BD3EC7FDB80451FEC2E8F22 |
| native-green-test-r2-receipt.json | 48026 | 149326C979D91DDA0B75D0D9D907A21E1CF11AD13B9D7DB473D8C86263904392 |
| green-r2-functional-review.json | 1283 | C8B6DE91E73ADE298984A11A0F53C11BB6F7D727F58A2AC94516B3070AF9F3BC |
| green-r2-independent-review.json | 1427 | E14B4D699D361C3980EAE67B6DA0EC2955DD0801434BDEDF0ED9137E70335280 |
| native-green-test-r1-receipt.json | 2831 | 90DC44D9A41E120DF823A9FCA2B05FB1080881577C5AD13C0E777188DB9CD80D |
| msvc-seven-r1-receipt.json | 3546 | 50E2516A534ED837F2B313BF5EA86233D82A40444D70B89D01A15655208D2630 |
| msvc-seven-r2-receipt.json | 222415 | 7892FF926BE138A1A3FD4521B5A9D0DA6B0AB93570CBFAB85BCB16EDF0B13034 |
| msvc-r2-root-review.json | 1167 | 8E68F76C0594188259E09BBBDA5E698E21C69B29BF6969DBB9B4AFB1C3F7C8BA |
| msvc-r2-independent-review.json | 1444 | 2FD66D8DDDE63D9C0D15D39D8CCC5C5C493DAB6027F8CF28D6FF200E725CD943 |

## Representation and seven entrance contracts

`zr_exec_ir_gc_map_outer_shape_is_valid` accepts exactly these optional-header
forms. It reads only the outer function fields; it authenticates no arbitrary
pointer, allocation extent, or ownership source.

| gcMap pointer | Outer count/capacity | Meaning |
| --- | --- | --- |
| NULL | 0/0 | Absent map |
| Non-NULL | 1/1 | One header, including an empty header |

Entries, slot-index pools, and inline-reference-offset pools are inner storage.
They have separate counts/capacities, may contain multiple records, and retain
their existing geometry checks. Hash algorithms and public layouts are unchanged.

| Real entrance | Healthy fixture result | Malformed envelope contract |
| --- | --- | --- |
| Fusion InputHash | Nonzero hash | Hash 0 through the existing storage check. |
| Container Build | true; generic UNKNOWN_LAYOUT/UNSUPPORTED remark and valid plan hash | false; FUNCTION_INVALID, kind NONE, execution UNSUPPORTED; plan equals actual PlanInit state. |
| Container ValidatePlan | true against a freshly built plan; execution diagnostic clear | false with the same invalid-function detail; old plan preserved. |
| Container Specialize | true using a real stable compact-map candidate; unknown string retains UNSUPPORTED remark | false/UNSUPPORTED, also observable with NULL diagnostic. Generic-only legal Specialize may return false outside this positive fixture. |
| DCE RunPass | true; allocation permitted | false/INVALID_ARGUMENT; changed initialized true becomes false. |
| SCCP Compute, rewrite=false | true; allocation permitted | false/INVALID_ARGUMENT; changed false; prior real cache/context preserved. |
| SCCP RunPass, rewrite=true | true; allocation permitted | Same storage rejection through Compute; no duplicate wrapper guard. |

Fusion retains aggregate/binding hash and sealed/BindingRows ordering. These
prechecks do not dereference the GC header or allocate on this path. Container
Build retains DiagnosticInit, required-argument checks, PlanInit, facts-envelope
checks, then storage admission. Existing typed/sealed/scope/generation priorities
remain. Validate checks facts/function before the old plan; Specialize delegates
Build and copies detail.execution. DCE/SCCP set changed=false before storage
checks and before their first allocations. All four reject malformed envelopes
before following any GC-header pointer.

The fixture function token is 73. Rejection execution diagnostics carry that
actual token and zero in the other fields, except code. Container's original
setter generally carries facts location IDs; this fixture's block/instruction/
source IDs are zero. NULL and poisoned diagnostics exercise refusal without
requiring a new diagnostic provider.

## Independent 99-case matrix and state ownership

Each of the seven entrances has three legal cases: absent, one empty header,
and one header containing two valid entries with independent inner pools. That
is 21 legal cases. The six malformed forms are:

| Live pointer form | Count/capacity |
| --- | --- |
| Real two-header allocation | 2/2 |
| Real two-header allocation | 1/2 |
| Non-NULL readable header allocation | 0/0 |
| NULL | 1/1 |
| NULL | 0/1 |
| Real two-header allocation | 2/1 |

Fusion has six malformed cases and no diagnostic argument. Each other entrance
has six forms in both NULL and poisoned diagnostic modes: 72 more malformed
cases, for 99 independent cases total. Each malformed case first proves its
entrance's legal baseline; each has its own Unity result, so an earlier RED
failure cannot hide later rows.

The fixture uses the real Core function lifecycle, legacy BindingRows, an
unsealed function, and valid instruction IDs 1 and 2. Bad non-NULL envelopes use
readable live outer storage borrowing valid first-header inner pools. Only the
outer pointer/count/capacity changes. Before any Unity assertion or owner Free,
all three fields are restored and borrowed headers are released. No poisoned
pointer is used.

Snapshots cover the function, actual allocated instruction/block/header/inner
pool buffers, scalar facts, old plan where required, and SCCP's prior real cache
allocation/context. Build is compared to its post-PlanInit output rather than
the pre-call poison. The real allocator ledger is unarmed. Actual malformed GREEN cases
showed zero allocation attempts; healthy DCE/SCCP calls may allocate. Cleanup
checks real live/error counts and release records, with recovery used only for
failed-test containment. `GC_LEGAL` and `GC_MALFORMED` lines preserve each actual
acceptance/hash/changed/allocation/state observation.

## Finite real-provider closure and configuration

Target `zr_vm_ssa_gc_map_parser_consumers_test` uses three roles:

| Role | Unique TUs |
| --- | ---: |
| Existing owner private Core object target | 15 |
| New explicit private provider object target | 12 |
| Fixture main, actual allocator, Unity | 3 |

The private role includes pass_manager; DCE, SCCP, copy_aliases; CFG, effects,
effects_linear, effect_loops, state_maps; fusion_contract; container_specialize;
and the actual Core container_storage_contract. These satisfy real diagnostic,
range, budget, copy-alias, CFG/effect/state-map and scalar-candidate dependencies.
No mock, extracted production provider, giant Parser/runtime archive, or extra
execution provider substitutes for them.

The accepted configure graph uses C11, MD, UNDEBUG, clang-cl UBSan, NOREF/NOICF,
and the existing finite Windows linkage. Core private include roots are explicit.
The forced allocator redirect applies to real private providers; system stdlib
is included before redirects, allocator keeps NO_REDIRECT, and Unity keeps CRT.
Original configured argv and final selected dependency/input identities remain
Root validation gates. Repeated compile events do not increase the 30 unique-TU
count. Acceptance requires the actual phase protocol and registered fresh output
paths; historical cache or matching timestamps alone provide no proof.

## Plan sources and remaining scope

The private plan and the three module documents describe the finite repair:
[scalar passes](../instruction-generation/execir-pass-pipeline.md),
[container specialization](../parser-and-semantics/container-specialization.md),
and [fusion input hashing](../instruction-generation/execbc-pattern-lowering.md).
The old plan's example separate build-root/CTest commands are not the active
native route: this task reuses the actual LowerB above and its registered staged
configure/build/deps/direct-test protocol.

This fixture calls only metadata/facts/plan entrances. It invokes no VM, Source
or Function execution, Oracle/interpreter, FFI, network/security callback,
materialization, resume, or collector path. No runtime semantic, whole-suite,
full warning-free build, SourceFrame/Finalized pre-Core geometry, general Core
span geometry, GC liveness, or complete 01.01/47-leaf acceptance follows.
BranchFacts/CallGraph/Inline remain indirect Core consumers rather than four
additional proven public bypasses. Those later scopes require separate designs,
real RED evidence, artifact registration, and authorization.
