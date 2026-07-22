# Syntax 05 M1 Unified Property AST/Symbol Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace split class/interface getter-setter syntax nodes with one `PropertyDecl` plus ordered accessor nodes, and bind one canonical property symbol whose accessors share property identity across class, struct, resource class, and interface declarations.

**Architecture:** A new `parser_property.c` owns contextual `property`, `get`, `set`, and `init` parsing for every container. A new `compiler_property.c` validates one declaration and projects one visible property member plus hidden callable accessor members linked by `propertySymbolId`/`propertyIdentity`; class, struct, and interface compilers delegate to it rather than pairing names. Legacy enum values and structs remain numerically stable for readers/tests, but the production parser no longer emits them and the compiler rejects manually supplied legacy nodes as semantic input.

**Tech Stack:** C11, zr_vm AST/parser/compiler APIs, canonical `TypeId`/`SymbolId`, Unity, CMake/Ninja, GCC/Clang/MSVC.

---

### Task 1: Freeze the unified syntax contract with RED tests

**Files:**
- Create: `tests/parser/test_property_unified_ast.c`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: Add one focused Unity target**

Register `zr_vm_property_unified_ast_test` with `zr_vm_add_unity_test_target`, include parser/core headers, and link through `zr_vm_link_parser_core_plus_library`.

- [x] **Step 2: Write the class/struct/resource/interface AST RED**

Use this canonical source family:

```c
const char *classSource =
        "class Box { pub property value: int { pub get { return 1; } pri set { return; } } }\n";
const char *structSource =
        "struct Pair { pub property first: int { pub get => 1; } }\n";
const char *resourceSource =
        "resource class Handle { pub property id: int { pub get { return 1; } } }\n";
const char *interfaceSource =
        "interface Named { pub property name: string { pub get; } }\n";
```

For every container assert the member node is `ZR_AST_PROPERTY_DECLARATION`, the declaration owns its name/type/access/modifiers, and each child is `ZR_AST_PROPERTY_ACCESSOR` with kind/body kind/access override and exact keyword/full ranges. Assert the interface and concrete declarations use the same node/data union members.

- [x] **Step 3: Write accessor-shape RED cases**

Cover bodyless, expression, and block bodies; `get`, `set`, and `init`; explicit and inherited visibility; static property; `value` remaining an ordinary identifier outside accessor bodies. Assert no `ZR_AST_CLASS_PROPERTY`, `ZR_AST_INTERFACE_PROPERTY_SIGNATURE`, `ZR_AST_PROPERTY_GET`, or `ZR_AST_PROPERTY_SET` node is emitted.

Add malformed inputs for a missing property body open/close, missing accessor semicolon, missing property type, and a half-written accessor. Assert the parser retains the preceding/following member where recovery is possible and reports the exact property/accessor token range.

- [x] **Step 4: Run the focused test and capture RED**

Run:

```powershell
wsl bash -lc 'cmake -S /mnt/e/Git/zr_vm -B /tmp/zr_vm-s05m1-gcc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc && cmake --build /tmp/zr_vm-s05m1-gcc --target zr_vm_property_unified_ast_test -j 2 && /tmp/zr_vm-s05m1-gcc/bin/zr_vm_property_unified_ast_test'
```

Expected: compile failure because the two unified AST kinds and structures do not exist.

### Task 2: Introduce one container-neutral AST and parser

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/ast.h`
- Create: `zr_vm_parser/src/zr_vm_parser/parser/parser_property.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_class.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/project_imports.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/writer/writer_syntax_tree.c`

- [x] **Step 1: Append stable AST kinds and structures**

Append, without renumbering old values:

```c
ZR_AST_PROPERTY_DECLARATION,
ZR_AST_PROPERTY_ACCESSOR,
```

Define:

```c
typedef enum EZrPropertyContainerKind {
    ZR_PROPERTY_CONTAINER_CLASS = 0,
    ZR_PROPERTY_CONTAINER_STRUCT,
    ZR_PROPERTY_CONTAINER_INTERFACE
} EZrPropertyContainerKind;

typedef enum EZrPropertyAccessorKind {
    ZR_PROPERTY_ACCESSOR_GET = 0,
    ZR_PROPERTY_ACCESSOR_SET,
    ZR_PROPERTY_ACCESSOR_INIT
} EZrPropertyAccessorKind;

typedef enum EZrPropertyAccessorBodyKind {
    ZR_PROPERTY_ACCESSOR_BODYLESS = 0,
    ZR_PROPERTY_ACCESSOR_EXPRESSION,
    ZR_PROPERTY_ACCESSOR_BLOCK
} EZrPropertyAccessorBodyKind;

typedef struct SZrPropertyDeclaration {
    SZrAstNodeArray *decorators;
    EZrAccessModifier access;
    TZrBool isStatic;
    TZrUInt32 modifierFlags;
    SZrIdentifier *name;
    SZrFileRange nameLocation;
    SZrType *typeInfo;
    SZrAstNodeArray *accessors;
} SZrPropertyDeclaration;

typedef struct SZrPropertyAccessor {
    EZrPropertyAccessorKind kind;
    EZrAccessModifier access;
    TZrBool hasAccessOverride;
    EZrPropertyAccessorBodyKind bodyKind;
    SZrAstNode *body;
    SZrFileRange keywordLocation;
} SZrPropertyAccessor;
```

Add both structures to `SZrAstNode.data`; retain legacy structures only for numeric/source compatibility.

- [x] **Step 2: Parse contextual property syntax once**

Expose:

```c
SZrAstNode *parse_property_declaration(SZrParserState *ps,
                                       EZrPropertyContainerKind containerKind);
TZrBool parser_property_declaration_starts_here(SZrParserState *ps);
```

`parser_property_declaration_starts_here` must use `current_identifier_equals(ps, "property")` after decorator/access/static/declaration modifiers, not add a global lexer keyword. Parse declaration type after `:`, require `{`, preserve accessor order, parse `=> expression;` through `parse_expression`, parse blocks through `parse_block`, and require `;` for bodyless/expression accessors.

- [x] **Step 3: Route every container to the shared parser**

Class/resource class, struct, and interface member dispatch call the same parser and append the same AST kind. Keep container kind only as parser validation context; do not encode container-specific property node kinds.

- [x] **Step 4: Free/project/write the new nodes**

Free decorators, type, accessor array, and accessor body exactly once. `project_imports.c` and syntax writer traverse/property-label the new nodes; no name-based accessor reconstruction is added.

- [x] **Step 5: Run parser GREEN**

Run the Task 1 command. Expected: all unified AST cases pass with zero failures.

### Task 3: Freeze the canonical PropertySymbol contract with RED tests

**Files:**
- Extend: `tests/parser/test_property_unified_ast.c`

- [x] **Step 1: Add bound-symbol assertions**

Compile class, struct, resource class, and interface sources and inspect their `SZrTypePrototypeInfo.members`. For each declaration assert exactly one visible member has `memberType == ZR_AST_PROPERTY_DECLARATION`, a nonzero canonical `symbolId`, the source property name, structured value type, one `propertyIdentity`, and accessor symbol links. Assert getter/setter/init callable members use hidden runtime names only as dispatch payload and share the visible property's identity.

- [x] **Step 2: Add negative semantic cases**

Capture compiler diagnostics for duplicate `get`, duplicate `set`, duplicate `init`, `set` plus `init`, accessor access wider than property, concrete bodyless accessor, interface accessor body, missing accessor, interface implementation property-type mismatch, and a manually constructed legacy property node. Expected stable diagnostics identify declaration/accessor ranges and never infer pairs from hidden/member names.

- [x] **Step 3: Run and capture RED**

Expected: AST cases remain green; bound-symbol/negative cases fail because the property compiler contract is absent.

### Task 4: Bind one property symbol and linked callable accessors

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/compiler.h`
- Create: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_property.c`
- Create: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_property.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_interface.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_member.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_receiver_effect.c`

- [x] **Step 1: Make property/accessor roles structured**

Replace raw accessor role integers with:

```c
typedef enum EZrPropertyAccessorRole {
    ZR_PROPERTY_ACCESSOR_ROLE_NONE = 0,
    ZR_PROPERTY_ACCESSOR_ROLE_GET,
    ZR_PROPERTY_ACCESSOR_ROLE_SET,
    ZR_PROPERTY_ACCESSOR_ROLE_INIT
} EZrPropertyAccessorRole;
```

Extend `SZrTypeMemberInfo` with `propertySymbolId`, `propertyValueTypeId`, `getterAccessorSymbolId`, `setterAccessorSymbolId`, `initAccessorSymbolId`, and `exportsWritableRef`. The visible property member owns these fields; callable accessor members retain `propertyIdentity`, `propertySymbolId`, and one accessor role.

- [x] **Step 2: Validate declaration/accessor invariants before emitting members**

`compiler_property_validate` must count accessor kinds, reject duplicates and `set`+`init`, enforce at least one accessor, enforce visibility narrowing, require interface accessors bodyless, forbid concrete bodyless accessors, and require all accessors to consume the declaration's single `typeInfo`. Do not compare accessor names because accessor nodes do not own names.

- [x] **Step 3: Emit the visible property first**

Allocate one `propertyIdentity` and one property `SymbolId`; emit a visible `ZR_AST_PROPERTY_DECLARATION` member carrying the source name/type/access/static/modifiers. Then emit one callable member per accessor, create its runtime hidden name from the property name only after semantic identity is fixed, assign getter readonly/setter mutable/init initializing receiver effect, and link symbol ids back to the visible property.

- [x] **Step 4: Compile accessor bodies from accessor nodes**

Refactor `compile_class_member_function` input extraction so a property accessor receives the declaration's name/type and its own body. Setter/init synthesize the canonical readonly `value` parameter using the property type; getter return type is the property type. Struct accessors use the same helper; interfaces emit bodyless callable contracts without compiling functions.

- [x] **Step 5: Delegate container compilers**

The class, struct, and interface member loops call `compiler_property_bind`; remove their old class/interface property pairing branches. Resource class automatically follows the class path and receives the same contract.

- [x] **Step 6: Run bound-symbol GREEN**

Run `zr_vm_property_unified_ast_test`. Expected: all AST, symbol, duplicate, visibility, body-kind, and legacy-source rejection cases pass.

### Task 5: Remove legacy semantic consumers and migrate focused fixtures

**Files:**
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_generic_semantics.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects_declarations.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_using_plugin_guard_escape_statement.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/syntax_contract.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership.c`
- Modify: `tests/parser/test_parser.c`
- Modify: `tests/parser/test_compiler_features.c`
- Modify: `tests/parser/test_reference_receiver_call_boundary.c`

- [x] **Step 1: Switch semantic consumers to the visible property identity**

Member lookup returns the visible property member. Existing read/write paths choose linked getter/setter symbols by `propertySymbolId`/accessor role, not by `_zr_get_`/`_zr_set_` name lookup. Task effects, ownership dataflow, compile-time traversal, generic semantics, receiver effect, syntax contract, and debug labels traverse declaration/accessor children.

- [x] **Step 2: Reject legacy nodes in the production compiler**

If a manually constructed `ZR_AST_CLASS_PROPERTY` or `ZR_AST_INTERFACE_PROPERTY_SIGNATURE` reaches the compiler, publish a stable migration diagnostic and fail compilation. Do not silently adapt it into a canonical property.

- [x] **Step 3: Migrate focused repository fixtures**

Rewrite the existing parser/compiler/receiver property fixtures to `property name: Type { ... }`. Setter bodies use contextual `value`; abstract/interface contracts use bodyless accessors inside the property body. Keep legacy syntax only in explicit negative/migration tests.

- [x] **Step 4: Run parent regressions**

Build and run:

```text
zr_vm_property_unified_ast_test
zr_vm_parser_test
zr_vm_compiler_features_test
zr_vm_reference_receiver_call_boundary_test
zr_vm_compiler_integration_test
zr_vm_canonical_consumers_test
zr_vm_semantic_query_test
```

Expected: every process exits zero and every Unity summary has zero failures.

### Task 6: Record M1 outputs and run promotion gates

**Files:**
- Create: `docs/plans/syntax/05-property-unified-ast/m1-unified-ast-symbol.md`
- Create: `tests/acceptance/2026-07-23-syntax-05-m1-unified-property-ast-symbol.md`
- Modify: `docs/parser-and-semantics/ast-and-syntax-contracts.md`
- Modify: `docs/parser-and-semantics/type-inference.md`
- Modify: `docs/parser-and-semantics/index.md`

- [x] **Step 1: Document the contract and boundaries**

Describe contextual grammar, container-neutral AST, one visible PropertySymbol plus linked callable accessors, legacy-source rejection, and deferred M2–M5 field/init-phase/lowering/ref-return/LSP-reflection work. The milestone record must contain `## 状态与产出记录`, completed items, final status, and completion time only after all gates pass.

- [x] **Step 2: Run three-toolchain matrices**

Use fresh/frozen exact-path snapshots. GCC, Clang, and MSVC must run the focused target plus parser/compiler/canonical/semantic-query parent gates with real process exit codes and zero failure markers. Run source CLI smoke; do not use LSP consumer reconstruction as M1 evidence.

- [x] **Step 3: Run final audit**

Require `git diff --check`, exact snapshot hash equality, no LSP paths, no external dirty Syntax drafts, and an empty shared index before staging.

- [x] **Step 4: Commit the completed milestone**

Exact-stage only M1 paths and commit:

```text
feat(syntax): unify property declarations
```

Verify the commit path count, forbidden path count zero, and index empty; then continue to Syntax05 M2 without marking the overall syntax goal complete.
