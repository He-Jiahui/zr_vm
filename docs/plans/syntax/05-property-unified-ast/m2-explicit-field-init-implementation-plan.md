# Syntax 05 M2 Explicit Field/Init Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `let` and `var` the explicit member/local binding surface, allow `init` property writes only during object initialization, and prove that only explicit fields own layout, metadata tokens, initialization state, and partial-construction cleanup.

**Architecture:** Append a stable `let` lexer token and project it onto the existing canonical immutable-binding fact while retaining `var const` as compatibility input. Generalize constructor field tracking around immutable explicit fields, introduce one structured compiler initialization-phase fact shared by constructors and `init` accessor bodies, and resolve property writes by linked accessor role (`SET` versus `INIT`) rather than hidden names. Existing FieldDef/TypeLayout machinery remains the sole storage source; PropertyDef stays a callable contract with no synthetic field or layout slot.

**Tech Stack:** C11, zr_vm lexer/AST/parser/compiler APIs, canonical SymbolId/TypeId/property identity, TypeLayout/metadata runtime, Unity, CMake/Ninja, GCC/Clang/MSVC.

---

### Task 1: Freeze explicit binding syntax and storage separation with RED tests

**Files:**
- Create: `tests/parser/test_property_explicit_field_init.c`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: Register one focused M2 Unity target**

Register `zr_vm_property_explicit_field_init_test` through the existing parser/core/library test helpers. Keep the target self-contained so all three toolchains can run the same binary.

- [x] **Step 2: Add lexer/AST RED cases for `let` and `var`**

Parse local, class, struct, resource-class, and interface/member surfaces. Assert `let` is immutable, `var` is mutable, declaration ranges start at the exact binding keyword, and malformed declarations retain following members. Freeze `var const` as accepted compatibility syntax but require new writer/debug output and all new fixtures to use `let`.

- [x] **Step 3: Add FieldDef versus PropertyDef RED cases**

Compile a type containing an explicit `let` field, explicit `var` field, and concrete property proxy. Assert exactly the two fields own field metadata/layout positions and stable field tokens; the visible property owns one property token and linked accessor tokens but no storage offset. Assert field and property SymbolIds remain independent even when names are related.

- [x] **Step 4: Capture RED**

Build and run the focused target. Expected failures: `let` is not a token/parser entry and `init` property assignment is not selected by the compiler.

### Task 2: Add the explicit `let`/`var` binding surface

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/lexer.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/lexer.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_loops.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_class.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/writer/writer_syntax_tree.c`

- [x] **Step 1: Append a stable lexer token**

Append `ZR_TK_LET` after the existing token enum tail so no prior token id changes. Add `let` to both token display and keyword lookup tables.

- [x] **Step 2: Parse one canonical binding choice**

Local/member declaration entry points accept `let` directly as immutable and `var` directly as mutable. Continue accepting legacy `var const`/`const` field forms as compatibility input, but do not introduce a second immutable semantic fact or infer binding kind from a name.

- [x] **Step 3: Route statement/member recovery through both keywords**

Update statement switches, loop declaration starts, class/struct/interface member classifiers, and recovery token sets so `let` has the same syntactic reach as `var` without changing unrelated `const fn` or compile-time `const` behavior.

- [x] **Step 4: Project binding kind in syntax output**

Expose immutable/mutable state in the syntax writer/debug output from the AST fact. Do not preserve source spelling as semantic identity.

- [x] **Step 5: Run syntax GREEN and parent parser regression**

Run the focused target plus `zr_vm_parser_test` and the const-containing `zr_vm_literal_surface_test` with real exit codes and zero Unity failures.

### Task 3: Freeze and implement the initialization-phase contract

**Files:**
- Extend: `tests/parser/test_property_explicit_field_init.c`
- Modify: `zr_vm_parser/include/zr_vm_parser/compiler.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_member.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_support.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_internal.h`

- [x] **Step 1: Add init-selection RED cases**

Cover constructor assignment to an init-only property, ordinary method assignment rejection, setter selection outside initialization, `set+init` remaining invalid, static/foreign receiver rejection, and exact diagnostic ranges. Assert resolution uses the visible PropertySymbol's linked accessor id/role.

- [x] **Step 2: Add immutable-field RED cases**

Cover declaration initializer, constructor direct assignment, write from an `init` accessor body, repeat assignment, compound assignment, foreign receiver write, shallow handle immutability, and struct subfield write rejection. `var` remains replaceable after construction.

- [x] **Step 3: Introduce one structured initialization-phase fact**

Replace constructor-only decisions at the affected field/property write boundary with a compiler initialization phase that is entered by a constructor and by compilation of an `init` accessor body, then restored across nested functions/lambdas. This fact must not be inferred from function/accessor names.

- [x] **Step 4: Resolve property writes by accessor role**

Change the hidden-accessor resolver to accept an explicit expected role. During initialization, prefer the linked `INIT` accessor; outside initialization, require linked `SET`. Imported source prototypes may fall back only by exact property identity plus structured role. Legacy native descriptors remain role-gated and cannot synthesize init semantics from hidden names.

- [x] **Step 5: Generalize immutable field checks without weakening const compatibility**

Use the existing canonical field mutability fact for `let` and legacy const fields. Permit one direct `this` initialization in the initialization phase, reject replacement and compound assignment, and retain path-sensitive constructor definite-assignment checks. Diagnostics use `let field` terminology for new syntax while compatibility tests may retain legacy wording only where source spelling is unavailable.

- [x] **Step 6: Run semantic GREEN**

Run focused, const-keyword, compiler-features, receiver-boundary, semantic-query, and canonical-consumer targets.

### Task 4: Prove layout, metadata, reflection, and artifact roundtrip

**Files:**
- Extend: `tests/parser/test_property_explicit_field_init.c`
- Modify as required by RED only: parser artifact/metadata projection sources
- Modify as required by RED only: core module/reflection projection sources

- [x] **Step 1: Assert explicit fields are the only layout inputs**

Inspect compiled TypeLayout and metadata rows. `let`/`var` fields each have independent field identity/token/offset and mutability; property/accessor rows do not increment field count or create GC/drop slots.

- [x] **Step 2: Assert source reflection separation**

Reflection enumerates explicit fields independently from one visible property and its accessors. No auto/backing field appears, and a similarly named field is not associated without explicit metadata.

- [x] **Step 3: Assert artifact roundtrip**

Serialize and load the focused type. Preserve field mutability/token/layout and property/accessor identity. Imported lookup must not rebuild a field-property association from names.

- [x] **Step 4: Keep metadata changes support-first**

Only modify production metadata/reflection code when a focused lower-layer assertion proves a missing structured fact. Do not add string/name fallbacks or broaden the artifact schema unnecessarily.

### Task 5: Prove construction completion and partial cleanup

**Files:**
- Extend: `tests/parser/test_property_explicit_field_init.c`
- Modify: `tests/core/test_type_layout_inline_copy.c`
- Modify as required by RED only: constructor/type-layout compiler or core runtime sources

- [x] **Step 1: Add constructor path cases**

Cover all-path initialization, missing branch/switch assignment, early return/throw, declaration initializer, and repeated immutable assignment. A successfully published instance has all required immutable explicit fields initialized.

- [x] **Step 2: Freeze partial-construction cleanup**

Construct a layout with droppable explicit fields, initialize a strict prefix, force constructor failure, and assert the runtime initialization bitmap drops exactly initialized fields once. Property/accessor metadata must not consume bitmap positions.

- [x] **Step 3: Run VM/AOT-facing regressions**

Run compiler integration, type-layout inline-copy, artifact roundtrip, debug metadata, decorator reflection, and the focused CLI source smoke. Any lower-layer failure is fixed before consumer expectations change.

### Task 6: Document M2 and run promotion gates

**Files:**
- Create: `docs/plans/syntax/05-property-unified-ast/m2-explicit-field-init.md`
- Create: `tests/acceptance/2026-07-23-syntax-05-m2-explicit-field-init.md`
- Modify: `docs/parser-and-semantics/ast-and-syntax-contracts.md`
- Modify: `docs/parser-and-semantics/type-inference.md`
- Modify: `docs/parser-and-semantics/index.md`
- Modify as needed: field/layout/artifact module documentation directly owned by changed production modules

- [x] **Step 1: Record contract and deferred boundaries**

Document `let/var`, compatibility input, initialization phase, FieldDef/PropertyDef separation, partial cleanup, and exact deferred M3 lowering work. The record contains `## 状态与产出记录`; status and completion time remain `in_progress`/unset until every gate passes.

- [x] **Step 2: Run same-snapshot three-toolchain matrices**

Freeze one exact-path snapshot and run GCC, Clang, and MSVC focused plus parent matrices with real exit codes and zero failure markers. Run source CLI and any artifact/AOT smoke from that same source snapshot.

- [x] **Step 3: Audit exact ownership**

Require snapshot hash equality, `git diff --check`, forbidden LSP/Syntax draft/build paths zero, and an empty shared index before staging.

- [x] **Step 4: Complete the record and commit**

Only after promotion gates pass, write completion time/status/completed items under `## 状态与产出记录`, exact-stage M2 paths, and commit:

```text
feat(syntax): establish explicit property fields
```

Verify exact path count, forbidden count zero, and index empty, then continue to Syntax05 M3 without marking the overall Syntax goal complete.
