---
related_code:
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_union.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_switch_patterns.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_union.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_union.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic_type_prototypes.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
  - tests/parser/test_union.c
  - tests/module/test_metadata_token_model.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_union.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_braced_primary_member.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_switch_patterns.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_union.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_union.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_type_prototypes.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
plan_sources:
  - user: 2026-06-17 Rust-like union syntax from docs/plans/using
  - docs/plans/using/04-union-types.md
tests:
  - tests/parser/test_union.c
  - tests/language_server/test_union_pattern_diagnostics.c
  - tests/module/test_metadata_token_model.c
  - tests/acceptance/2026-06-17-union-types.md
doc_type: module-detail
---

# Union Types Frontend Slice

This page records the implemented P3 union slice from `docs/plans/using`.

## Implemented Surface

- `union` is a lexer keyword and a top-level declaration.
- Union declarations support unit, tuple-style, and struct-style variants:
  - `Empty;`
  - `Circle(radius: float);`
  - `Rect { width: float; height: float; }`
- Generic declarations parse for `Option<T>` and `Result<T, E>`.
- A single variant in a union can be marked with `@` as the default validation variant. Duplicate default markers are rejected while parsing.
- Variant constructors parse as member paths:
  - `Shape.Circle(2.0)`
  - `Shape.Empty`
  - `Option<int>.Some(42)`
  - `Option<int>.None`
  - `Shape.Rect { width: 3.0, height: 4.0 }`
- Type inference returns the declared union type name for those constructors, including concrete generic instance names such as `Option<int>`.
- Basic `switch` cases can match a variant tag without payload destructuring:
  - `(Shape.Circle)`
  - `(Option<int>.None)`
- Tuple-style `switch` cases can bind positional payload fields after the tag match:
  - `(Shape.Circle(r))`
- When the `switch` subject has a known union type, switch cases may omit the union prefix:
  - `(Circle(r))`
  - `(Empty)`
  - `(Rect { width: w, height: h })`
- Struct-style `switch` cases can bind named payload fields and lower them in declaration order:
  - `(Shape.Rect { width: w, height: h })`
- `switch` owner payload bindings can opt into transfer with `move` on the local binding:
  - `(Resource.Open(move handle))`
  - `(Open { handle: move h })`
  - `(Open { move handle })`
  Struct switch patterns keep the existing `field: local` direction; the same-name shorthand uses the field as the local name.
- Pattern shape mismatches are rejected: tuple-style variants cannot use object payload patterns, and struct-style variants cannot use tuple payload patterns.
- Pattern arity mismatches are reported as structured diagnostics with expected and actual binding counts. Struct variants also surface the available field names in the fix suggestion.
- Pattern variant mismatches are reported as structured diagnostics when a `using` annotation names a variant on a different union than the resource expression type.
- `using` can act as a single-variant guard for a known union resource:
  - `using (var []: Shape.Empty = shape) { ... } else { ... }`
  - `using (var [r]: Shape.Circle = shape) { ... } else { ... }`
  - `using (var {width, h: height}: Shape.Rect = shape) { ... } else { ... }`
  Legacy unannotated variant-call binders such as `using (var Circle(r) = shape)` are no longer accepted for `using`; the variant selection belongs in the annotation, while the binder remains tuple or object destructuring.
- When a union has an `@` default variant, `using` may omit the variant name and destructure the default payload directly:
  - `using (var [m]: DynamicModule<Plugins> = %import("zr.plugins")) { ... } else { ... }`
  - `using (var [m] = %import("zr.plugins")) { ... } else { ... }`
  - `using (var {width, height}: Shape = shape) { ... } else { ... }`
  Tuple payloads use tuple destructuring, and struct payloads use field destructuring. The same shape rule is enforced for explicit annotations and `@` default-variant using forms. Explicit `DynamicModule<Plugins>.Available` annotations remain valid. Typed and no-annotation `%import` payload bindings are registered with the plugin escape scanner before the guard body is compiled, so returning, passing, aggregating, closure-capturing, or assigning the payload module through an inner region into an outer local that later escapes without a future `share()` path is rejected.
- No-block `using` destructuring shorthand is supported for non-import union resources:
  - `using [value] = option;`
  - `using [value]: Choice.Num = choice;`
  - `using {width, h: height} = shape;`
  The binding is registered in the current scope, and the tag-mismatch branch skips the binding instructions. `else` is not part of this shorthand. `%import` guard shorthand remains block-scoped.
- Union `switch` statements are checked for exhaustiveness when the switched expression has a known union type and the switch does not include a `()` default. Repeated cases for the same union variant are rejected as unreachable.
- Typed export signature blobs encode known union types with `ZR_METADATA_SIGNATURE_NODE_UNION`, including generic arguments such as `Option<int>`.
- Compiled `prototypeData` records union declarations as `ZR_OBJECT_PROTOTYPE_TYPE_UNION`, with each variant serialized as a `ZR_AST_UNION_VARIANT` member.
- Variant member metadata constants record payload field descriptors: declared field name, runtime storage name, type name, positional index, and passing mode.
- Union prototype and variant metadata include byte layout information: tag size, payload offset, prototype size/align, variant payload size/align, and each payload field's byte offset/size/align.
- Union payload metadata records the ownership qualifier for value payload fields, so owner/gc payload slots can be reconstructed as embedded `SZrTypeValue` fields in the runtime type layout.
- Typed union local variables now receive inline frame slot layout metadata, and the runtime can materialize a constructor carrier in that slot into inline tag/payload bytes for POD payload fields.
- Inline-frame member reads now expose `__zr_unionVariant` and `__zr_unionPayloadN` for typed union local slots, so `switch` and `using` pattern reads can consume inline tag/payload bytes after materialization.
- Typed union local identifiers can be copied into another typed union local through declaration initialization (`var target: Choice = source`) or simple assignment (`target = source`) without first losing inline layout through an object-shaped temporary.
- Constructor assignment into an existing typed union local (`target = Choice.Num(27)` or `target = Resource.Empty`) materializes the constructor carrier into the destination inline slot and drops the previous active owner payload before overwriting it.
- Nested inline struct fields that contain union fields can be updated and read through member chains, for example `holder.inner.choice = Choice.Num(41)` followed by `switch (holder.inner.choice)` or `using (var [v]: Choice.Num = holder.inner.choice)`.
- Struct-field inline union owner payload copy is covered in both directions. Assigning a typed union local into `holder.resource` copies the active owner payload into the inline field, and initializing `var copied: Resource = holder.resource` copies it back out to a typed local. Clearing the source field or local afterward does not invalidate the copied owner payload.
- Inline union type layouts now copy, drop, and visit GC values by active tag. Copying into an existing inline union first drops the destination's old active value payload, then copies active payload `SZrTypeValue` fields through `ZrCore_Value_Copy`.
- Constructor carrier objects that contain owner payload values are consumed when they materialize into inline union storage, so the carrier does not retain an extra strong owner reference after the inline payload copy. The same owner-aware lifecycle is covered for child functions, replacement assignment, nested struct fields, and multi-payload variants.
- `switch` and `using` payload bindings inherit the matched variant field's declared type. `Unique<T>`, `Shared<T>`, and `Loan<T>` owner payloads are borrowed by default, so the binding is registered as `Borrow<T>` and lowered through `OWN_BORROW`.
- Block-style `using` payload bindings can opt into owner transfer with a local binding `move` marker:
  - `using (var [move handle]: Resource.Open = resource) { ... }`
  - `using (var {move handle}: Resource.Open = resource) { ... }`
  - `using (var {move local: field}: Resource.Open = resource) { ... }`
  Explicit `move` keeps the payload field's declared owner type, skips `OWN_BORROW`, and clears the matched inline union payload after binding so active-variant drop does not release the moved owner again.

## Compiler Lowering

The current compiler lowering still uses a runtime carrier object as the constructor bridge. Typed local initialization and simple constructor assignment have limited inline materialization paths, and `switch`/`using` pattern matching can now read the inline slot through the same pseudo-member names when the receiver is a typed union local.

Variant constructors lower to a plain object with stable metadata fields:

- `__zr_unionType`: declared type or generic instance name.
- `__zr_unionVariant`: variant name.
- `__zr_unionPayload0`, `__zr_unionPayload1`, ...: positional payload values.

Tuple-style constructors write payload slots from call argument order. Struct-style constructors validate object payload keys against the declared variant fields, then write payload slots in declaration order so later pattern destructuring can share one carrier shape.

This carrier keeps constructed values executable through the existing object runtime. When the destination is a typed union local with inline frame layout, the compiler emits a self `SET_STACK` after the initializer and the runtime converts the carrier into inline `[tag][payload]` bytes using serialized variant metadata. A later `target = Union.Variant(...)` assignment uses the same inline-frame copy hook: the runtime accepts the carrier from either the physical stack slot or the logical frame value slot, drops the destination's current active union payload through the resolved union type layout, and then writes the new tag/payload bytes. The verified path covers POD payload fields such as `Shape.Circle(radius: float)` and value-sized owner/gc payload fields that are stored as embedded `SZrTypeValue` slots.

When an expression is just a typed union local identifier and the destination slot is known, `compile_expression_into_slot` now emits a direct `SET_STACK` from the source local slot and records the destination union layout hint. Simple assignment uses the same union-only direct source slot path for `target = source`. This is intentionally not generalized to inline structs, whose constructor argument and nested-field copy paths continue to use their established temporary/normalization lowering.

For nested member assignment and access, the compiler allocates fresh never-used slots for inline struct/union prefixes so frame-layout metadata can classify those slots as inline even if lower-numbered temporaries were previously plain values. Assignment writeback then stores the modified child inline frame back to its parent field. `using` pattern guards also register the hidden resource slot with the inferred union type hint before reading pseudo-members, so a subject like `holder.inner.choice` remains on the inline union member path. Direct field reads whose declared field type is inline now fall back to declared-field member resolution when the type-member metadata table has no field entry, so `holder.resource` can keep its inline union return layout instead of falling back to object member access.

Frame-layout hint preservation also depends on typed metadata not confusing control-flow operands with stack slots. Conditional jump instructions store relative offsets in their primary operands; the plain-value slot classifier therefore treats those operands as branch metadata and only marks actual extra stack-slot operands. This prevents hidden inline receivers from being downgraded to plain value slots before `GET_MEMBER_SLOT`.

`switch` statement cases that name a union variant read `__zr_unionVariant` and compare it with the case variant name. Tuple-style and struct-style payload bindings then read `__zr_unionPayloadN` into case-local variables before compiling the case block. When a union variant pattern resolves, the compiler keeps the matched variant AST and registers each payload binding with the declared field type. For `Unique<T>`, `Shared<T>`, and `Loan<T>` payload fields, the default binding path emits `OWN_BORROW` and records the local as `Borrow<T>` in the type environment. This makes default destructuring non-consuming: `%release(handle)` and `%detach(handle)` are rejected on the destructured binding. `Weak<T>` payloads remain weak so `%upgrade` semantics are preserved. Source-level `switch` move patterns use the same `isMoveBinding` flag as `using`: `(Open(move handle))`, `(Open { handle: move h })`, and `(Open { move handle })` keep the declared owner type, skip `OWN_BORROW`, and clear the matched payload slot after binding. For typed union local identifiers, the compiler keeps the original inline local slot instead of first copying through a generic temporary, so runtime `GET_MEMBER` can read the inline tag and payload bytes. Carrier objects remain supported through the existing object member path, and non-union switch cases keep the existing expression comparison path.

Before lowering a union switch, the compiler infers the switched expression type and resolves the union declaration. If the switch has no `()` default, every declared variant must be covered by a union variant case; otherwise compilation reports `Non-exhaustive union switch; missing variant '<name>'`. The compiler also rejects a second case that covers an already-covered variant with `Unreachable union switch case; variant '<name>' is already covered`. A `()` default is the explicit escape hatch for intentionally partial variant lists.

`using` pattern guards reuse the same pseudo-members for a single variant. The block-style form names the variant in the type annotation and uses the payload shape as the binding pattern: `using (var []: Shape.Empty = shape) { ... } else { ... }` for unit variants, `using (var [r]: Shape.Circle = shape) { ... } else { ... }` for tuple payloads, and `using (var {width, h: height}: Shape.Rect = shape) { ... } else { ... }` for struct payloads. The no-block shorthand uses the same destructuring shape with an ordinary assignment form, such as `using [value] = option;`, `using [value]: Choice.Num = choice;`, and `using {width, h: height} = shape;`. The compiler compares the tag, binds the destructured payload, and falls back to `else` when a block body exists. In no-block form, `body == NULL`; bindings are emitted in the current scope, and the false branch skips the binding instructions. The compiler rejects object destructuring for tuple variants and array/tuple destructuring for struct variants in both annotated and default-variant forms. Payload bindings share the same variant-field type registration as `switch` cases. Owner payloads are non-consuming by default: `using (var [handle]: Resource.Open = resource)` exposes a declared `Shared<Box>` payload as a borrowed local, so owner-consuming builtins such as `%release(handle)` are rejected. Explicit move syntax is available for block-style `using`: `using (var [move handle]: Resource.Open = resource)` and `using (var {move handle}: Resource.Open = resource)` bind the declared owner type instead. If the union declaration marks exactly one default validation variant with `@`, typed `using` may name only the union type and still match that default variant; tuple payloads bind through `var [field]`, while struct payloads require `var {field}` or alias forms. If `shape` is a typed union inline local, those pseudo-members are served from inline frame bytes; otherwise carrier object reads continue through ordinary object member lookup.

For explicit `move` bindings, statement lowering skips `OWN_BORROW`, registers the local with the payload field's declared ownership qualifier, and emits `SET_MEMBER_SLOT_NULL` against the matched `__zr_unionPayloadN` pseudo-member after the local has been bound. Runtime inline-frame member writes understand union payload pseudo-members, so the null write updates the active inline union storage instead of falling back to object member state. This is what prevents the moved owner from being released again when the active union variant later drops or is overwritten.

The compiler rejects the old `using (var Variant(field) = value)` binder in `using` guards so the active surface stays aligned with the Rust-like destructuring rule. `switch` case patterns still use the existing `Shape.Circle(r)` / `Shape.Rect { width: w }` case syntax; this rejection is scoped to `using`, where `guardTypeInfo` or the default `@` variant carries the variant identity.

Typed and no-annotation `%import` guards use the same default-variant rule for plugin availability checks. For example, `DynamicModule<T>` may declare `@Available(m: Module)`, and `DynamicModule<Plugins>.Available`, `DynamicModule<Plugins>`, and `using (var [m] = %import(...))` forms lower through the import-guard helper before the block is entered. Explicit `DynamicModule<Plugins>.Available` is accepted only when `Available` is the union's `@` default validation variant; naming a non-default variant is rejected instead of binding the import payload. The bare payload type name `Module` is treated as an implicit builtin type during type parsing so this surface does not require spelling `zr.builtin.Module` inside the union declaration. The payload binding is treated as a guard-scoped plugin value for `plugin_type_escape`, so return, call-argument, aggregate, closure-capture, and inner-region assignment into an outer escaping local are rejected unless a later `share()` promotion makes the escape explicit.

`compiler_metadata_token.c` now recognizes union type names that are present in the current script AST while building typed export signature blobs. A known union type is emitted as a `UNION` node with its base union name and recursively encoded generic type arguments; unknown object type names continue to use the existing `TYPE_REF` node.

`compiler_union.c` registers union declarations into the same prototypeData stream used by structs/classes/interfaces/enums. Each variant is stored as a member descriptor: `declarationOrder` and `fieldOffset` track the tag order, `fieldSize` stores the variant kind, `parameterCount` stores the number of payload fields, and `fieldTypeName`/`returnTypeName` point back to the owning union type.

Each variant member also carries a metadata object through the existing member-level metadata constant path. The object has `kind = "unionVariant"`, `ownerType`, `variantName`, `tag`, `variantKind`, `payloadFieldCount`, `tagSize`, `payloadOffset`, `layoutByteSize`, `layoutByteAlign`, `variantPayloadSize`, `variantPayloadAlign`, and a `payloadFields` array. Each payload field entry records `index`, source `name`, runtime `storageName` such as `__zr_unionPayload0`, source `type`, `passingMode`, `ownershipQualifier`, `byteOffset`, `byteSize`, and `byteAlign`. Runtime inline-frame materialization consumes this metadata to write the tag and payload bytes for typed local constructors, and inline-frame member access consumes the same metadata to map tag bytes back to variant names and load or store payload fields for pattern bindings and explicit move cleanup. Runtime type-layout resolution also consumes `ownershipQualifier`: value-sized payload fields become active-tag-managed GC fields, and owned payload fields additionally participate in deterministic drop.

`compiler_typed_metadata.c` stores inline union frame `typeLayoutId` values in the same serialized prototypeData index space that `ZrCore_Function_ResolvePrototypeFrameTypeLayout` reads at runtime. This matters when compile-time type prototypes include non-serialized placeholders: the byte-frame metadata must point at the serialized union prototype blob entry, not the transient compiler array index.

## Tooling

`ZR_OBJECT_PROTOTYPE_TYPE_UNION` is registered in display/reflection/metadata mappings. The language server can bootstrap union type prototypes and collect union declarations plus variants into the existing enum-style symbol path.

## Remaining Work

- Basic guard-scoped module `.share()` promotion is implemented: no-arg `share()` on a guarded module handle lowers to the native shared-owner helper, keeps the original plain module binding usable inside the guard, and returns a releasable shared owner. Scoped plugin release and complete cross-region/global/async plugin type escape semantics remain open. No-type-annotation block `%import` destructuring sugar, typed/untyped import payload escape scanner reuse, and inner-region assignment propagation for outer plugin aliases are implemented. No-block `%import` guard lifetime has not been extended.
- Further complex expression/member-level union mutation matrices remain planned beyond the typed-local and struct-field owner-copy cases. Basic typed-local constructor materialization, constructor assignment, nested struct-field constructor assignment/read via `switch`/`using` such as `holder.inner.choice = Choice.Num(41)`, struct-field owner payload copy in both directions, default-borrow payload binding, block-style `using` explicit move destructuring, no-block `using` tuple/object destructuring shorthand, switch explicit-move source syntax, local-to-local copy/assignment, multi-owner payload release coverage, and active-variant owner-aware copy/drop/GC dispatch are implemented. The nested using resource path is also covered: `using (var [v]: Choice.Num = holder.inner.choice)` keeps the hidden resource slot typed as the inline union before pseudo-member tag/payload reads.
- Legacy unannotated `using (var Variant(...)=...)` compatibility has been removed from the current surface; regression coverage now keeps the annotation-plus-destructuring contract in place.
- Pattern shape mismatch, unknown-field, arity mismatch, and variant mismatch compiler-level coverage and LSP golden coverage are implemented for `using` union pattern diagnostics.
- `%import` guard lowering now requires explicit variant annotations to target the `@` default validation variant; mixed struct using aliases such as `{width, h: height}` are covered by regression tests.
