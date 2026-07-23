# Syntax 05 M4 Ref-return/Place/Region Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use repository TDD, support-first regression,
> module-documentation, and three-toolchain validation workflows. Execute every checkbox in order.

**Goal:** Implement `ref T` and `ref readonly T` property getters as canonical reference-producing
accessors, lower their results through Place/region facts, preserve managed interior references across
VM/AOT/artifacts, and reject every invalid setter, escape, receiver, and ownership boundary.

**Architecture:** A ref property remains storage-free. Resolve one visible PropertySymbol and linked
getter SymbolId, invoke the getter once, obtain a structured reference result, and dereference that
result into a Place only in a value/store context. Region and mutability come from the getter return
TypeId, receiver Place/GC handle, and canonical loan facts. No consumer may reinterpret a value result,
property name, hidden accessor name, source text, raw pointer, or cache state as a reference contract.

**Tech Stack:** C11, unified property AST, canonical TypeId/SymbolId, Place/LoanId/Semantic IR,
reference escape analysis, VM frame slots and managed references, C/LLVM AOT contracts, executable
artifact IO, Unity, CMake/Ninja, GCC/Clang/MSVC.

---

### Task 1: Freeze syntax, contract, and lowering RED

**Files:**
- Create: `tests/parser/test_property_ref_return.c`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: Register one focused M4 target**

Register `zr_vm_property_ref_return_test` with parser/core/library harness support. Keep syntax,
compiler, runtime, artifact, and negative probes in this one target so all toolchains execute the same
contract.

- [x] **Step 2: Freeze ref/ref-readonly declaration contracts**

Cover `property value: ref T` and `property value: ref readonly T`, a single explicit getter, exact
property/accessor TypeIds, `exportsWritableRef`, receiver effect, and source ranges. Reject set/init,
bodyless concrete ref getters, ordinary value returns, and writable/readonly signature mismatch.

- [x] **Step 3: Freeze Place-use RED cases**

Cover value load, `ref propertyAccess` identity, writable assignment, writable compound assignment
without a setter, and readonly write rejection. Assert the getter and receiver execute exactly once.

- [x] **Step 4: Freeze region and escape RED cases**

Cover class field, addressable struct field, ref-struct/view element, and static field sources. Reject
local/temporary returns, non-addressable struct receivers, escaped shorter regions, writable export
through readonly receiver capability, active owner move/drop, and unsupported native raw-pointer
projection.

- [x] **Step 5: Capture focused RED**

Build and run the target with a real process exit. Record failures by layer: parser surface, property
contract, reference call result, Place projection, region validation, runtime representation, and
artifact/AOT parity.

### Task 2: Bind canonical ref getter contracts

**Files:**
- Extend: `tests/parser/test_property_ref_return.c`
- Modify as required by RED: property parser/AST sources
- Modify as required by RED: `compiler_property.c`, receiver-effect and canonical binding sources
- Modify as required by RED: return-statement/reference syntax sources

- [x] **Step 1: Represent explicit ref return intent**

Parse the design's explicit ref getter body forms without conflating them with parameter passing or
prototype references. AST carries return/reference intent and exact range; normal `return expr` stays
unchanged.

- [x] **Step 2: Validate property shape**

A ref property requires one concrete getter and forbids set/init. The getter return TypeId must exactly
match `ref T` or `ref readonly T`; bodyless/auto storage and property ref rebinding remain unavailable.

- [x] **Step 3: Publish receiver/ref effects**

`ref readonly T` keeps readonly receiver effect. `ref T` sets `exportsWritableRef` and requires a
writable receiver unless the canonical view type explicitly owns an independent writable-ref
capability. Override/interface contracts match ref kind invariantly.

- [x] **Step 4: Reuse canonical call/query projections**

Property, getter, callable return, semantic query, diagnostics, and artifact metadata expose one
reference TypeId and one accessor identity. Do not derive ref kind from display strings or getter body
text.

- [x] **Step 5: Run syntax/contract GREEN**

Run focused parser/binder cases plus M1/M2/M3 property suites, reference syntax, canonical consumers,
semantic query, receiver boundary, parser, and literal surfaces.

### Task 3: Lower PropertyRefGet and Deref Place

**Files:**
- Extend: `tests/parser/test_property_ref_return.c`
- Modify as required by RED: expression/assignment lowering and call-result metadata
- Modify as required by RED: Place/Semantic IR projection sources

- [x] **Step 1: Emit one reference-producing accessor call**

Capture the receiver once and invoke the exact getter once. The call result remains a reference value
with canonical access and source region; it is not eagerly copied as the referent value.

- [x] **Step 2: Project context-sensitive Place use**

Value context emits Deref+Load. `ref propertyAccess` preserves reference identity. Assignment and
compound assignment through `ref T` emit Store/operator/Store to the dereferenced Place and never
require a setter. `ref readonly` rejects stores before partial effects.

- [x] **Step 3: Publish PropertyRefGet Semantic IR**

Emit `PROPERTY_REF_GET(propertyId, receiver) -> refValue` followed by `DEREFERENCE -> place` where
required. Preserve property/accessor identity, result TypeId, Place path, receiver loan, and source
range in structured facts.

- [x] **Step 4: Preserve evaluation and exception order**

Receiver and getter run once; compound RHS runs only after getter success. Getter/RHS/operator/store
exceptions use existing cleanup and call-info paths. No setter or name fallback is permitted.

- [x] **Step 5: Run focused Place GREEN repeatedly**

Assert values, mutation, counters, opcodes, Place ids, loan ids, and exact diagnostics across cold and
quickened execution.

### Task 4: Enforce region, managed interior ref, and ownership boundaries

**Files:**
- Extend: `tests/parser/test_property_ref_return.c`
- Modify as required by RED: reference escape/loan/NLL sources
- Modify as required by RED: VM managed-ref/frame-place/runtime sources

- [x] **Step 1: Derive result regions from structured sources**

Class field refs bind to receiver GC handle plus managed interior location; struct field refs do not
outlive the addressable receiver; ref-struct/view element refs bind to the view base region; static
field refs use static region. Getter locals and temporaries cannot escape.

- [x] **Step 2: Enforce access capability**

Readonly refs cannot store. Writable refs cannot escape through a readonly receiver unless the view
contract explicitly carries independent writable capability. Override/interface ref kind is invariant.

- [x] **Step 3: Integrate owner loans**

A ref exported from `Unique`/Shared owner receiver keeps the owner loan live through its last use.
Move/drop/share/intoGc operations conflicting with that loan fail through canonical LoanId/Place facts,
not through property spelling or diagnostics text.

- [x] **Step 4: Preserve GC compact safety**

Class interior refs use an updatable managed representation (base handle plus structured projection or
equivalent). VM stack growth and GC relocation refresh the base; AOT may not lower to a naked pointer
without an explicit pin/escape proof.

- [x] **Step 5: Run escape/ownership/GC GREEN**

Run reference escape, loan/NLL, owner borrow, ref-struct, TypeLayout, GC movement, and focused negative
matrices with exact reasons and real process exits.

### Task 5: Close artifact, VM, and AOT parity

**Files:**
- Extend: `tests/parser/test_property_ref_return.c`
- Modify as required by RED only: executable IO/artifact metadata and VM/AOT consumers

- [x] **Step 1: Roundtrip ref property identity**

Source and loaded artifacts preserve PropertyDef/accessor tokens, ref TypeId/access, receiver/export
effects, Place/call metadata, frame layouts, and execution results. Unsupported old artifacts fail or
use a structured compatibility path; they do not guess from names.

- [x] **Step 2: Prove VM cold/quickened parity**

Generic and cached paths produce the same managed ref/Place and keep receiver/owner guards alive. Cache
heat cannot change readonly, region, or lifetime behavior.

- [x] **Step 3: Prove C/LLVM AOT boundary**

Run exact AOT contract and executable smokes for supported ref returns. Raw native/FFI direct pointers
remain rejected unless a descriptor publishes the required managed/pinned contract.

- [x] **Step 4: Run VM/AOT-facing regressions**

Run compiler integration, object dispatch, debug/reflection, type layout, artifact schema, AOT call/
reference contracts, and source CLI property fixtures.

### Task 6: Document, promote, and commit M4

**Files:**
- Create: `docs/plans/syntax/05-property-unified-ast/m4-ref-return-place-region.md`
- Create: `tests/acceptance/2026-07-23-syntax-05-m4-ref-return-place-region.md`
- Modify: `docs/parser-and-semantics/type-inference.md`
- Modify: Place/reference/runtime/artifact/AOT module docs directly owned by the implementation

- [x] **Step 1: Record contracts and M5 boundary**

Document ref getter surface, PropertyRefGet/Deref, region derivation, managed interior refs, owner-loan
conflicts, artifact/AOT parity, and the LSP/reflection/migration work deferred to M5. Keep status
`in_progress` and completion unset until all gates pass.

- [x] **Step 2: Run one frozen three-toolchain snapshot**

Freeze exact M4 paths and run GCC, Clang, and MSVC focused plus parent/reference/VM/AOT matrices with
real process exits and zero focused/parent Unity failures. Record any pre-existing unrelated baseline
without claiming it GREEN.

- [x] **Step 3: Audit exact ownership**

Require snapshot byte/hash equality, `git diff --check`, forbidden LSP/unrelated Syntax/build paths
zero, and an empty shared index before staging.

- [x] **Step 4: Complete the status record and commit**

Only after promotion succeeds, write completion time/status/completed outputs under
`## 状态与产出记录`, exact-stage M4 paths, and commit:

```text
feat(syntax): lower property reference places
```

Verify exact path count, forbidden count zero, and index empty, then continue to Syntax05 M5.
