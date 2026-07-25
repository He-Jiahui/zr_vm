# Syntax 05 M5 Property Consumers/Reflection/Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use repository TDD, support-first regression,
> module-documentation, cross-session coordination, and frozen three-toolchain validation workflows.
> Execute every checkbox in order from the existing `main` checkout.

**Goal:** Make source, binary artifact, runtime reflection, LSP and migration tooling consume one
canonical PropertySymbol/PropertyDef contract, then remove hidden-accessor spelling as a semantic
source for current artifacts.

**Architecture:** The parser publishes one structured property contract keyed by property SymbolId and
linked accessor SymbolIds. The stable PropertyDef row preserves its current encoded width by replacing
the reserved 64-bit tail with `initializerToken` and `nameStringOffset`; signature/contract rows remain
the source of TypeId, receiver/ref-export effects and callable constraints. Reflection and LSP join
these identities. Legacy source migration is produced as structured parser edit facts, never inferred
from a diagnostic message by the LSP.

**Tech Stack:** C11, canonical TypeId/SymbolId/SemanticQuery, fixed-width artifact schema, executable
metadata/reflection, LSP UTF-16 projection and workspace edits, Unity, CMake/Ninja, GCC/Clang/MSVC.

---

### Task 1: Freeze canonical consumer and migration RED

**Files:**
- Create: `tests/parser/test_property_consumer_contracts.c`
- Create: `tests/language_server/test_lsp_property_contract_cases.h`
- Modify: `tests/language_server/test_lsp_interface.c`
- Modify: `tests/language_server/stdio_smoke.js`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: Register the lower-layer focused target**

Add `zr_vm_property_consumer_contracts_test` with parser/core/library/runtime harness support. Keep
PropertyQuery, PropertyDef bytes, source/reloaded reflection and migration-fix assertions in this
target. The LSP contract cases run in the existing interface target so protocol and UTF-16 conversion
use production consumers.

- [x] **Step 2: Freeze the public query shape**

The RED test expects this information without reading AST spelling:

```c
typedef struct SZrParserSemanticPropertyQuery {
    TZrSymbolId propertySymbolId;
    TZrTypeId propertyTypeId;
    TZrSymbolId getterSymbolId;
    TZrSymbolId setterSymbolId;
    TZrSymbolId initializerSymbolId;
    TZrTypeId getterCallableTypeId;
    TZrTypeId setterCallableTypeId;
    TZrTypeId initializerCallableTypeId;
    EZrAccessModifier access;
    EZrAccessModifier getterAccess;
    EZrAccessModifier setterAccess;
    EZrAccessModifier initializerAccess;
    TZrUInt32 modifierFlags;
    EZrReceiverEffect receiverEffect;
    EZrReferenceAccess referenceAccess;
    TZrBool exportsWritableRef;
    TZrBool isStatic;
    SZrFileRange declarationRange;
    SZrFileRange selectionRange;
} SZrParserSemanticPropertyQuery;
```

Cover value, init-only, static, virtual/override/interface, `ref`, `ref readonly`, generic and decorated
properties. Exact accessor ids and ranges must match M1-M4 compiler facts.

- [x] **Step 3: Freeze PropertyDef and reflection RED**

Assert the fixed 48-byte PropertyDef row contains property/owner/getter/setter/initializer/signature
tokens, name offset, flags and hashes. Source and reloaded reflection must expose one property entry,
accessors, visibility, reference access, receiver effect, decorator metadata and no backing field.
Ordinary methods named `__get_x` or `__set_x` must remain methods.

- [x] **Step 4: Freeze LSP RED**

Assert unified-property hover canonical text, a single completion item, exact definition and rename on
the property selection range, independent explicit-field rename, contextual accessor `value` token,
ref-kind/receiver-effect display and source/binary parity. No accessor payload names may appear.

- [x] **Step 5: Freeze migration and action RED**

Cover a single legacy getter, adjacent matching getter/setter pair, bodyless interface pair, mismatched
names/types, missing interface accessor and explicit-field proxy action. Applicable migrations carry
exact structured edits; ambiguous/non-adjacent pairs are diagnostic-only.

- [x] **Step 6: Capture focused RED by layer**

Run the parser focused target, LSP interface target and stdio smoke. Record failures separately for
query publication, PropertyDef bytes, reflection join, LSP projection and structured migration.

### Task 2: Publish canonical property query and PropertyDef rows

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/semantic.h`
- Modify: `zr_vm_parser/include/zr_vm_parser/semantic_query.h`
- Create: `zr_vm_parser/src/zr_vm_parser/semantic/semantic_property_contract.c`
- Create: `zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_property.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/semantic.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_property.c`
- Modify: `zr_vm_core/include/zr_vm_core/artifact_schema.h`
- Modify: `zr_vm_core/src/zr_vm_core/artifact_rows.c`
- Modify: `zr_vm_core/src/zr_vm_core/artifact_schema.c`
- Modify: `tests/parser/test_artifact_schema.c`
- Extend: `tests/parser/test_property_consumer_contracts.c`

- [x] **Step 1: Add owned property-contract storage**

Add a `propertyContracts` array to `SZrSemanticContext`; append-by-copy, reset and free must own no AST
text and leave SymbolId/TypeId/ranges stable. Publishing requires a PROPERTY symbol, nonzero property
TypeId and at least one linked accessor.

- [x] **Step 2: Publish from compiler binding**

After `compiler_property.c` has validated the visible member and accessors, publish the exact ids,
callable TypeIds, property/accessor visibility, static/modifier flags, receiver/ref-export effect and
source/selection ranges. Accessor callable TypeIds remain the canonical source of their individual
receiver effects. Do not inspect hidden accessor names or getter body text.

- [x] **Step 3: Implement query-by-position and query-by-id**

Expose `ZrParser_SemanticQuery_PropertyAt(...)` and
`ZrParser_SemanticQuery_PropertyBySymbolId(...)`. Position lookup chooses the narrowest canonical
selection/reference range and returns false with a zeroed output when unavailable.

- [x] **Step 4: Complete the fixed-width PropertyDef row**

Replace `reserved0` with two 32-bit fields at encoded offsets 40 and 44:

```c
TZrMetadataToken initializerToken;
TZrUInt32 nameStringOffset;
```

Validate the optional initializer as a MemberDef token, preserve zero/zero compatibility, and reject
unknown flags or a row with no accessor. Keep schema version and encoded size unchanged.

- [x] **Step 5: Run lower-layer GREEN**

Run property consumer, artifact schema, canonical consumers, semantic query and M1-M4 property targets
until ids, hashes, reset/free and malformed-row negatives pass.

### Task 3: Make runtime reflection identity-driven

**Files:**
- Create: `zr_vm_core/src/zr_vm_core/reflection_property.c`
- Create: `zr_vm_core/src/zr_vm_core/reflection_property_internal.h`
- Modify: `zr_vm_core/src/zr_vm_core/reflection.c`
- Modify: `zr_vm_core/include/zr_vm_core/object.h`
- Modify: `zr_vm_parser/include/zr_vm_parser/compiler.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/writer.c`
- Extend: `tests/parser/test_property_consumer_contracts.c`
- Modify as required by RED: `tests/module/test_module_system.c`

- [x] **Step 1: Preserve the visible property carrier**

Typed executable metadata must retain a visible property member with property identity, name,
signature/access flags and linked accessor tokens/constant indices. Accessor entries keep their role
and shared property identity; fields remain independent.

- [x] **Step 2: Build reflection by property identity**

Move property reflection assembly out of the large `reflection.c`. Join the visible carrier and
accessors by `propertyIdentity` and structured role. Populate getter/setter/initializer handles,
visibility, static/virtual/override flags, receiver effect, reference access and writable-export effect.

- [x] **Step 3: Gate legacy compatibility explicitly**

Current metadata with a visible property carrier never calls the hidden-prefix parser. A narrow legacy
reader may recognize old executable patches only when no canonical property carrier/identity exists;
ordinary current methods named `__get_*`/`__set_*` are never promoted.

- [x] **Step 4: Preserve source/reloaded/minimal-metadata parity**

Roundtrip property and accessor tokens and keep reflectable/dynamic dispatch roots during metadata
stripping. A ref getter returns the managed reference handle from M4, not a boxed referent value.

- [x] **Step 5: Run reflection and module GREEN**

Run focused reflection, module system, decorators, metadata runtime, artifact and property execution
targets for both source and reloaded modules.

### Task 4: Migrate LSP to canonical property facts

**Files:**
- Modify: `zr_vm_language_server/include/zr_vm_language_server/symbol_table.h`
- Create: `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_property_contract.c`
- Create: `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_property_contract.h`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/semantic_type_prototypes.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_tokens.c`
- Extend: `tests/language_server/test_lsp_property_contract_cases.h`
- Create: `tests/language_server/test_lsp_property_refactor_cases.h`
- Modify: `tests/language_server/test_lsp_interface.c`

- [x] **Step 1: Register one unified property symbol**

Consume `ZR_AST_PROPERTY_DECLARATION` and `ZrParser_SemanticQuery_Property*`; store canonical property
SymbolId/TypeId and selection range on the LSP symbol. Do not pair `ZR_AST_CLASS_PROPERTY` nodes or
register the property as FIELD semantics.

- [x] **Step 2: Project hover and completion**

Format property type/ref access, accessor visibility and receiver effect from the canonical query.
Completion emits one item per property identity and uses the visible name. Remove current source and
type-prototype prefix stripping from the current-artifact path.

- [x] **Step 3: Project navigation and rename**

Definition, references, document symbols, highlights, prepareRename and rename target the property
selection/reference facts. Accessor navigation may expose explicit secondary targets, but hidden names
are not rename roots. Explicit fields keep separate SymbolIds and edits.

- [x] **Step 4: Project the authoritative binary property carrier**

The canonical artifact contract remains PropertyDef + exact linked MemberDef/signature/Contract
tokens. The current executable writer is still the explicitly documented `SZrIo` v34 compatibility
format and does not embed that nested table; its authoritative bridge is therefore the exact visible
compiled-property row plus accessor rows sharing `propertyIdentity`. Consume only its structured
TypeId/ref/accessor-role payload and byte-stable row identity. Missing/invalid rows return unavailable
instead of reconstructing a property from accessor names. Formal `ZRAF` executable cutover remains
the repository-level successor boundary recorded by Syntax 01 M5, not a hidden fallback in this
property milestone.

- [x] **Step 5: Classify contextual `value`**

Accessor `value` tokens use the linked initializer/setter parameter SymbolId and parameter semantic
token kind. An ordinary identifier spelled `value` or `field` follows normal resolution.

- [x] **Step 6: Run LSP consumer GREEN**

Run interface, project, semantic query/hover, token/UTF-16, source-contract and stdio/CLI targets.
Assert source/binary labels, ranges and edits are byte/UTF-16 equivalent.

### Task 5: Publish structured actions and legacy migration

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_class.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/parser/parser_property.c`
- Create: `zr_vm_parser/src/zr_vm_parser/parser/parser_property_migration.c`
- Create: `zr_vm_parser/src/zr_vm_parser/parser/parser_property_migration.h`
- Modify: `zr_vm_parser/include/zr_vm_parser/compiler.h`
- Create: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_property_requirements.c`
- Modify: `zr_vm_language_server/include/zr_vm_language_server/conf.h`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/lsp_code_actions.c`
- Create: `zr_vm_language_server/src/zr_vm_language_server/lsp_property_code_actions.c`
- Create: `zr_vm_language_server/src/zr_vm_language_server/lsp_property_code_actions.h`
- Extend: `tests/parser/test_property_consumer_contracts.c`
- Extend: `tests/language_server/test_lsp_property_contract_cases.h`
- Modify: `tests/language_server/stdio_smoke.js`

- [x] **Step 1: Publish a stable legacy-property diagnostic**

Use stable code `legacy_property_syntax`. Parse legacy declarations only into a temporary structured
node, publish exact declaration/name/type/body ranges, then discard it as a semantic member. The
diagnostic remains even when no safe migration exists.

- [x] **Step 2: Build migration edits in the parser**

For a single accessor or an adjacent matching getter/setter pair, create the complete unified
`property name: Type { ... }` replacement from parsed roles and exact source slices. A matching pair
must agree on owner, name, type, static/access contract and adjacency. Otherwise publish no edit.

- [x] **Step 3: Consume only structured edits in LSP**

The code-action provider exposes parser migration fixes and snapshot revalidation already used by safe
fixes. It may not branch on diagnostic message/title or scan `get`/`set` source text.

- [x] **Step 4: Add canonical refactor actions**

Use PropertyQuery to generate a missing required interface accessor or an explicit `pri let/var`
field plus proxy accessor. The field receives its own SymbolId; rename remains independent unless the
user selects the explicit associated refactor.

- [x] **Step 5: Prove negative boundaries**

No action is emitted for mismatched legacy pairs, generated/native/binary-only declarations, stale
snapshots, ref properties requiring storage, ambiguous interface requirements or invalid source.

### Task 6: Stress, document, promote and commit M5

**Files:**
- Create: `docs/plans/syntax/05-property-unified-ast/m5-property-consumers-reflection-migration.md`
- Create: `tests/acceptance/2026-07-23-syntax-05-m5-property-consumers-reflection-migration.md`
- Create: `tests/language_server/test_lsp_property_incremental_cases.h`
- Create: `tests/parser/test_property_consumer_stripping_cases.h`
- Modify: `docs/parser-and-semantics/type-inference.md`
- Modify: `docs/parser-and-semantics/artifact-schema-and-type-projection.md`
- Modify: `docs/core-runtime/property-accessor-dispatch.md`
- Modify: `docs/parser-and-semantics/lsp-semantic-resolution-and-native-imports.md`
- Modify: `docs/parser-and-semantics/index.md`
- Modify: `docs/core-runtime/index.md`
- Modify: `zr_vm_aot/docs/parser-and-semantics/property-reference-place-semir-aot.md`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.h`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c`
- Modify: `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c`
- Modify: directly owned reflection/LSP module docs and indexes

- [x] **Step 1: Run stress and incremental boundaries**

Cover many properties/accessors, deep override/interface lookup, large-file body-only edits, contract
edits, source-to-binary replacement and metadata stripping. Body-only edits preserve unrelated
property facts; contract edits invalidate exact dependents.

- [x] **Step 2: Record the final contract**

Document PropertyQuery, fixed-width PropertyDef extension, reflection fields, LSP surfaces, structured
migration, explicit legacy compatibility gate and the prohibition on member-name/display fallback.

- [x] **Step 3: Run one frozen three-toolchain snapshot**

Freeze exact M5 paths and run GCC/Clang/MSVC focused, property M1-M5, parser/query/artifact/reflection,
LSP interface/project/UTF-16/source contracts, stdio/CLI and relevant VM/AOT targets. Preserve real
process exits and compare any unrelated failure with clean HEAD before accepting it as baseline.

- [x] **Step 4: Audit exact ownership**

Require snapshot SHA-256 equality, `git diff --check`, forbidden unrelated Syntax/build/generated
paths zero, explicit coordination for all LSP paths and an empty shared index before staging.

- [x] **Step 5: Complete status and commit**

Only after every gate passes, write completion time/status/items under `## 状态与产出记录`, exact-stage
M5 paths and commit:

```text
feat(syntax): converge property consumers
```

Verify exact path count, forbidden count zero and index empty before advancing beyond Syntax 05.

## M5.1: Unified Property Variance Follow-up

### Exact Write Set

| Layer | Paths | Responsibility |
|---|---|---|
| LSP semantic analyzer | `zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_variance.c`, `semantic_analyzer_typecheck.c`, `semantic_analyzer_internal.h` | Extract interface variance validation behind a narrow internal API and apply the existing canonical rule to `ZR_AST_PROPERTY_DECLARATION` using structured accessor kinds. |
| LSP regression | `tests/language_server/test_semantic_analyzer.c` | Keep the six-position variance fixture on current unified property syntax. |
| docs | M5 status/record and LSP semantic-resolution module document | Record the compatibility removal, range of acceptance, and unrelated marker boundary. |

### Steps

1. Replace the legacy `pub get/set` fixture input with the current bodyless
   `property` declaration and retain the six required `invalid_variance`
   locations.
2. Extend only the LSP analyzer's interface variance switch to classify
   canonical accessor kinds: `get` is output, `set/init` is input, and a
   mixed property is invariant. Do not branch on names or source text.
3. Run the semantic-analyzer regression, the frozen GCC/Clang/MSVC 18-target
   LSP matrices, and the three stdio/CLI smokes per toolchain. Record unrelated
   Unity markers separately.

#### M5.1 Acceptance

Completed 2026-07-26 04:21 +08:00. The old fixture had become invalid before
variance analysis because legacy accessor declarations are no longer semantic
members. The LSP analyzer now handles the same unified PropertyDecl accessor
facts already used by the compiler: getter-only properties are output,
setter/init-only properties are input, and mixed properties are invariant.
GCC 11.4, Clang 14.0, and MSVC 17.14 each completed the 18-target LSP matrix
with true process exit zero, and all nine stdio/CLI smokes exited zero. The
same five Unity assertion markers in native constructor, receiver completion,
foreach shadowing, and container inference remain outside this exact write
set and are not counted as M5.1 passing evidence.
