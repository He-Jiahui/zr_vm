# Syntax 05 M3 Access Lowering/Receiver Effect Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use the repository TDD, support-first validation,
> module-documentation, and three-toolchain verification workflows. Execute every checkbox in order.

**Goal:** Lower source, virtual, interface, and static property get/set/init access through canonical
PropertySymbol-linked callable contracts, implement single-evaluation compound assignment, and enforce
readonly/writable/initializing receiver effects consistently in VM and AOT-facing consumers.

**Architecture:** Property access remains storage-free. Resolve the visible property once, select its linked
accessor SymbolId and canonical callable TypeId, capture the receiver into one compiler slot/Place, and emit the
existing typed call machinery. Compound assignment owns one explicit lowering plan: captured receiver, getter,
RHS, operator, setter. Dispatch and effect decisions consume property/accessor metadata, never hidden names,
source-text rewrites, PIC heat, or AST rescans in consumers.

**Tech Stack:** C11, zr_vm AST/compiler/type inference, canonical SymbolId/TypeId, receiver effects, VM call
dispatch, C/LLVM AOT-facing contracts, Unity, CMake/Ninja, GCC/Clang/MSVC.

---

### Task 1: Freeze typed access and evaluation order with RED tests

**Files:**
- Create: `tests/parser/test_property_access_lowering.c`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: Register one focused M3 Unity target**

Register `zr_vm_property_access_lowering_test` with parser/core/library test helpers. Keep all source snippets,
runtime probes, artifact roundtrips, and instruction assertions in this target so every toolchain runs the same
contract.

- [x] **Step 2: Add direct get/set RED cases**

Cover instance getter and setter execution, exact visible PropertySymbol/accessor linkage, canonical parameter and
return types, and one receiver evaluation. Use a receiver-producing function with an observable counter so a
duplicated receiver is a behavioral failure.

- [x] **Step 3: Add compound-assignment RED cases**

Cover `+=` plus one non-additive operator, getter-before-RHS-before-setter ordering, one receiver evaluation, one
RHS evaluation, operator type conversion, and getter-only/setter-only negative boundaries. Do not rewrite source
into two property expressions in the test harness.

- [x] **Step 4: Add receiver-effect RED cases**

Assert value getters are readonly, setters are writable, init accessors are initializing, and static accessors have
no receiver. A getter body cannot mutate instance storage; a setter cannot be invoked through readonly receiver
capability; ordinary code cannot select init. Preserve the M2 hidden-accessor call/reference rejection.

- [x] **Step 5: Capture RED**

Run the focused target with a real process exit. Expected RED is the current unsupported property compound path
and any remaining dynamic/name-based call projection exposed by the structured assertions.

### Task 2: Normalize typed get/set/init lowering

**Files:**
- Extend: `tests/parser/test_property_access_lowering.c`
- Modify as required by RED: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c`
- Modify as required by RED: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c`
- Modify as required by RED: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_internal.h`
- Modify as required by RED: compiler call/quickening helpers directly owning the emitted call contract

- [x] **Step 1: Resolve one visible property contract**

Resolve the owner TypeId and visible PropertySymbol first. Select the linked getter/setter/init SymbolId by exact
role and validate the accessor's canonical callable TypeId, value type, visibility, static flag, and receiver effect.
Legacy artifact fallback is allowed only through structured property/accessor metadata.

- [x] **Step 2: Capture the receiver once**

Materialize or borrow the receiver into one stable slot/Place before accessor invocation. Struct receivers retain
addressability/writeback provenance; class handles retain the one evaluated value; static properties do not create
or bind a receiver.

- [x] **Step 3: Emit typed getter and setter calls**

Route reads and simple assignments through the existing canonical call lowering with exact parameter/return
contracts. Preserve property identity in member-entry/call-site metadata where available. Do not emit dynamic
member access merely because an accessor is hidden from source lookup.

- [x] **Step 4: Preserve init as a distinct capability**

Continue using the M2 initialization phase and initialization member-entry provenance. Setter lowering must never
inherit init capability, and init lowering must not be selected outside construction.

- [x] **Step 5: Run focused and parent GREEN**

Run focused, M1, M2, receiver-boundary, canonical-consumer, semantic-query, parser, and literal-surface targets.

### Task 3: Implement single-evaluation compound property assignment

**Files:**
- Extend: `tests/parser/test_property_access_lowering.c`
- Modify as required by RED: expression lowering/operator conversion sources
- Modify as required by RED: compiler temporary-slot/Place helpers

- [x] **Step 1: Build one compound lowering plan**

Represent the captured receiver, resolved getter/setter identities, RHS expression, base operator, value TypeId,
and result slot as one compiler-owned plan. Reject missing getter/setter before emitting partial writes.

- [x] **Step 2: Enforce source evaluation order**

Emit receiver once, getter once, RHS once, checked/operator computation once, then setter once. Preserve exception
and cleanup order; a throwing getter prevents RHS evaluation and a throwing RHS prevents setter invocation.

- [x] **Step 3: Validate operator and accessor contracts**

Apply the same operator resolution and conversion rules as ordinary compound assignment. Getter result must be
compatible with the operator and setter input; result expression behavior must match the language's assignment
contract.

- [x] **Step 4: Keep ref-return lowering deferred**

Do not implement the M4 `ref T` direct-Place compound path here. M3 rejects or preserves the existing unavailable
boundary for ref-return property compound assignment.

- [x] **Step 5: Run effect/order GREEN**

Run focused probes repeatedly and assert exact counters/event order, not only final values.

### Task 4: Close virtual, interface, static, and inherited dispatch

**Files:**
- Extend: `tests/parser/test_property_access_lowering.c`
- Modify as required by RED: property contract/override/interface binding sources
- Modify as required by RED: VM call-site and AOT callable-provenance consumers

- [x] **Step 1: Freeze dispatch RED cases**

Cover virtual override getter/setter selection through a base-typed receiver, interface getter/setter dispatch,
inherited non-override property access, static get/set without receiver, and negative half-override/accessor-role
cases.

- [x] **Step 2: Dispatch by linked accessor identity**

Use the accessor's virtual/interface slot and base-definition identity already attached to the linked property
contract. Do not search hidden getter/setter names or splice property/member names at runtime.

- [x] **Step 3: Preserve receiver effects across contracts**

Override/interface compatibility cannot strengthen readonly getter requirements or weaken writable setter/init
requirements. Static accessors remain receiver-free across source and artifact import.

- [x] **Step 4: Prove source/artifact parity**

Serialize and reload the focused hierarchy. Source and artifact execution must select the same accessor identities,
slots, effects, and results.

### Task 5: Prove VM/AOT behavior and regression boundaries

**Files:**
- Extend: `tests/parser/test_property_access_lowering.c`
- Modify as required by RED only: VM/AOT call lowering or metadata consumers

- [x] **Step 1: Freeze runtime instruction/call-site provenance**

Assert ordinary, quickened, virtual, interface, and static paths retain canonical accessor identity and do not
degrade to dynamic/name-based semantics after warmup.

- [x] **Step 2: Add throw/cleanup ordering cases**

Cover getter throw, RHS throw, setter throw, and receiver temporary cleanup. Assert each initialized/owned value is
released exactly once and no setter runs after an earlier failure.

- [x] **Step 3: Run VM/AOT-facing regressions**

Run compiler integration, object known-native/dispatch boundaries, debug metadata, decorator reflection,
type-layout, artifact roundtrip, one AOT contract target, and the source CLI property fixture.

### Task 6: Document, promote, and commit M3

**Files:**
- Create: `docs/plans/syntax/05-property-unified-ast/m3-access-lowering-receiver-effect.md`
- Create: `tests/acceptance/2026-07-23-syntax-05-m3-access-lowering-receiver-effect.md`
- Modify: `docs/parser-and-semantics/type-inference.md`
- Modify: `docs/parser-and-semantics/index.md`
- Modify as needed: directly owned compiler/runtime/AOT module docs

- [x] **Step 1: Record contracts and deferred M4 boundary**

Document canonical accessor selection, receiver capture, compound order, receiver effects, dispatch behavior,
throw cleanup, and the exact ref-return work deferred to M4. Keep status `in_progress` and completion unset until
all gates pass.

- [x] **Step 2: Run one frozen three-toolchain snapshot**

Freeze exact M3 paths and run GCC, Clang, and MSVC focused plus parent matrices with real process exits and zero
Unity failures. Run source CLI and AOT/artifact smoke from the same source bytes.

- [x] **Step 3: Audit exact ownership**

Require snapshot byte/hash equality, `git diff --check`, forbidden LSP/unrelated Syntax/build paths zero, and an
empty shared index before staging.

- [x] **Step 4: Complete the status record and commit**

Only after promotion succeeds, write completion time/status/completed outputs under `## 状态与产出记录`,
exact-stage M3 paths, and commit:

```text
feat(syntax): lower typed property access
```

Verify exact path count, forbidden count zero, and index empty, then continue to Syntax05 M4.
