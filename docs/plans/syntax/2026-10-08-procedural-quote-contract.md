---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compile_tool.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_descriptor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor_internal.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/attribute_contract.c
  - zr_vm_parser/include/zr_vm_parser/attribute_contract.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_transaction.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/include/zr_vm_parser/location.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
implementation_files:
  - M1 token source/tree/span/context and resource ownership modules (planned)
  - M2 immutable syntax views and append-only member validation modules (planned)
  - zr_vm_parser/include/zr_vm_parser/compile_tool.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_descriptor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/attribute_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - compile-time artifact/provider and expansion transaction modules
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - docs/plans/syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md
  - lua/rust/library/proc_macro/src/lib.rs
  - lua/rust/library/proc_macro/src/quote.rs
  - lua/rust/library/proc_macro/src/to_tokens.rs
  - lua/rust/compiler/rustc_expand/src/base.rs
  - lua/rust/compiler/rustc_expand/src/expand.rs
  - lua/rust/compiler/rustc_expand/src/proc_macro_harness.rs
tests:
  - tests/compileTime/test_compile_time_execution.c
  - tests/compileTime/test_comptime_contract.c
  - tests/compileTime/test_declaration_transform_contract.c
  - tests/compileTime/test_proc_macro_quote_contract.c (new)
  - tests/compileTime/test_proc_macro_binary_parity.c (new)
doc_type: milestone-detail
status: m4-contract-read-only
---

# Procedural macros and quote: M4 contract

This document narrows the M4 portion of the accepted
`macro-tokenstream-structured-generation-design` plan. It is a contract and
test gate, not an implementation claim. M4 production work waits for accepted
M1 (owned token source/tree/span/context and resource lifetime) and M2
(immutable syntax views, parsing, and append-only member validation). M3's
registration and expansion scheduler is also a prerequisite for invocation
syntax. Root owns CMake/build integration and milestone commits.

## Current substrate and required boundary

The current `zr.compile` descriptors expose build flags and diagnostics only;
`zr.compile.declaration` exposes typed declaration patches. Constructed typed
values are ordinary VM objects carrying a hidden role field. That is adequate
for the old declaration contract but is spoofable by a script object and is not
an acceptable token or syntax value boundary. AST locations retain a source
name and offsets, while lexer tokens are ephemeral and source bytes are
borrowed; an M4 value cannot rely on either lifetime. Compile-time frames copy
raw `SZrTypeValue` pointers and are freed after each call, so returned token
values must be retained/promoted before frame teardown.

Existing declaration decorators resolve through `parse_decorator_expression`,
capture a snapshot, execute each transform against that same snapshot, and
commit patches right-to-left. M4 must not inherit that ordering accidentally:
attribute macros are sequential replacements and derives consume the previous
merged target. The existing patch transaction also documents a borrowed-name
GC hazard; the token/syntax transaction must close the same hole.

Binary compile-time imports currently serialize a typed runtime projection,
not a source body or procedural role. A projected runtime function is not a
procedural macro provider. Source and binary providers therefore need one
controlled evaluator contract (or an explicit unsupported-provider error),
with no source-name dispatch in shared runtime code.

## Canonical modules, roles, and values

Append new role IDs; do not renumber existing CompileTool role values or alter
the existing public contract hashes. The descriptor table and artifact metadata
must expose a versioned contract for each of these roles:

| Role | Definition | Invocation | Result |
| --- | --- | --- | --- |
| `zr.compile.procMacro` | `comptime fn(input: TokenStream): TokenStream` | `name!(...)` | replacement token stream |
| `zr.compile.procMacroAttribute` | `comptime fn(args: TokenStream, item: TokenStream): TokenStream` | `#name(...)#` | replacement for the complete item |
| `zr.compile.procMacroDerive("Name")` | `comptime fn(item: TokenStream): TokenStream` | `#derive(Name)#` | complete target plus legal additions |

`zr.compile.token` is the compile-only module for opaque `TokenStream`,
`TokenTree`, `Group`, `Ident`, `Punct`, `Literal`, `Span`, and `SyntaxContext`.
`zr.compile.syntax` is the compile-only module for the M2 immutable syntax
views and `parse*`, `appendMembers`, `replace`, and `toTokens` operations.
`zr.compile.emit(TokenStream): void` is a `zr.compile` comptime operation,
valid only in module-scope comptime execution, and enters the normal syntax
expansion pipeline once.

Every token/syntax value is a native opaque handle with a private internal type
tag and owning arena/provider reference. A user-created object with matching
fields, a copied hidden field, or an arbitrary native pointer must fail the
validator. Public constructors return only handles produced by the descriptor
provider. The handle carries retain/release (or GC root) operations; copying a
`SZrTypeValue` retains the handle, and frame/arena destruction releases it.
Token trees preserve delimiters, exact literal/identifier spelling, punctuation
joint/alone spacing, source origin, and hygiene context. `Display`/string output
is for diagnostics only and is never re-lexed as compiled input.

### Declaration attributes and raw capture

The three role attributes are real schemas in `attribute_contract.*`, with
canonical owner module, CompileTool phase, target kind, retention, repeatability,
contract hash, and function-signature metadata. Their invocation arguments are
captured as a direct token stream before ordinary typed-attribute binding. The
capture records delimiter kind, exact token origins, and the source attribute
index; an empty argument list is distinct from a missing delimiter.

`procMacroHelper` is a declaration metadata role for a helper attribute name
owned by a registered derive. A helper declaration records its derive owner and
helper name. A helper occurrence is visible only while that derive is expanding
the matching target/member, is consumed exactly once, and is an error if no
matching derive consumes it. Helpers cannot become globally visible attributes
through a copied object or a different derive.

Attribute collection is lexical and deterministic. Non-repeatable attributes
are rejected on the second occurrence. Built-in and role attributes are marked
consumed at their owning phase; an unconsumed role/helper attribute is an
error, not silently dropped. Attribute macros run top-to-bottom, each receiving
the current complete item and replacing it. Derives run in listed order, each
receiving the previous merged complete target. A failed expansion leaves the
previous detached tree and artifact unchanged.

### Derive invariants

The input and returned token streams are parsed as the complete target type.
Before the first derive, assign immutable identities to every existing member
and preserve its source span, order, name, visibility, type, signature, and
body/initializer identity. A derive may append legal fields, methods,
properties, or metadata to the owning AST collection. It may not remove,
rename, reorder, mutate, change a signature/visibility/type, or create an
external implementation block. Invalid additions, collisions, duplicate
helpers, and invalid target kinds are diagnosed before publication.

## quote, quote_spanned, and `ToTokens`

`quote! { ... }` constructs a token stream directly. The template grammar is
token-level and does not parse an interpolated value's display text:

* `#value` invokes the canonical `ToTokens` conversion; interpolated tokens
  retain their original span/context.
* `##` emits one literal `#` token, allowing generated ZR attributes without
  confusing interpolation with `#...#` syntax.
* `#(body)*`, `#(body)+`, and `#(body)?` repeat a body. A single token or
  delimiter immediately before `*`/`+` is a separator (`#(body),*`, for
  example). Nested repetitions are evaluated inside-out. All iterables in one
  repetition must have the same count; `+` requires a non-empty count and `?`
  permits at most one. Count mismatch, missing interpolation, or an invalid
  repetition source is a located compile-time error.
* Template punctuation keeps joint/alone spacing and delimiters. Generated
  template tokens use call-site hygiene and origin by default.

`quote_spanned!(span, { ... })` has the same grammar but assigns the explicit
`Span`/`SyntaxContext` to generated template tokens. It does not overwrite the
origin of interpolated tokens. Span handles are immutable and remain valid until
the returned stream is committed or released.

`ToTokens` is a native capability, not a user-spoofable method name. It is
implemented for all token types, immutable M2 syntax nodes, references/optional
values, strings, booleans, characters, and scalar numeric values. Primitive
values use call-site origin. A `ToTokens` failure identifies the source value
and quote invocation; no fallback to display/re-lex is permitted.

## Ownership, provider, and cache contract

At invocation, input streams and syntax views are retained in the compile-time
frame and in an expansion-owned token arena. A returned stream is promoted into
the detached expansion transaction before `ct_call_function` frees its frame
and execution scope. `ct_frame_set`, cache values, and decorator argument
storage must use the same retain/root hooks; borrowed source bytes or native
pointer values are never published. Cache entries serialize stable token/tree
data, spans, contexts, and contract versions, never arena addresses.

The compile-time function/provider record must carry role, contract version and
hash, owner module, derive/helper metadata, signature, and executable provider
identity. Binary artifacts must contain an executable compile-time section that
the same controlled evaluator can invoke, including all role metadata and
dependency/lock identity. A binary containing only the existing runtime
projection is rejected for a procedural role with a provider diagnostic. Source
and binary calls use the same `CompileTool` execution scope, phase, budgets,
visibility, transaction, and evaluator; source names and module paths cannot
select a special implementation. Artifact/cache identity includes provider
content and role metadata hashes.

## Diagnostics and transaction rules

Use stable `compiletool.proc_macro.*` diagnostics (with macro definition and
invocation spans where available) for: missing/wrong role; wrong function
signature or return type; visibility/provider/contract mismatch; unsupported
binary executable section; malformed raw attribute delimiters; unknown
interpolation or repetition/count mismatch; token budget, recursion, or heap
limits; parse leftovers; derive removal/rename/reorder/signature mutation,
illegal member, collision, external implementation, or helper-scope errors;
duplicate/non-repeatable or unconsumed attributes; and `emit` outside module
comptime scope. Every error aborts the detached expansion and leaves the last
valid AST/artifact published.

## Required M4 tests

The test names below are proposed additions; root owns registration and CMake.

1. Descriptor tests assert appended role IDs, module descriptors, exact
   signatures/effects/minimum phase, contract hashes, and rejection of ordinary
   object/native-pointer token spoofs.
2. A source proc macro covers nested delimiters, escaped literals, comments,
   compound punctuation, joint/alone spacing, origin/context, and direct
   `TokenStream` round trips without display/re-lex.
3. Attribute tests cover raw argument/item capture, empty-vs-missing delimiters,
   top-to-bottom replacement, duplicate/unknown/unconsumed attributes, and
   helper visibility/consumption for the matching derive only.
4. Derive tests cover listed-order accumulation, immutable existing-member
   comparison, legal append-only members, collision and external-impl errors,
   rollback, and preserved originals after a later derive fails.
5. Quote tests cover `#value`, `##`, nested repetitions with separators and
   `*`/`+`/`?`, count mismatch, all `ToTokens` categories, `quote_spanned!`,
   interpolated origin preservation, and deep-tree/budget failure.
6. `compile.emit` tests cover module-scope enforcement, deterministic emitted
   declaration ordering, a second expansion pass, and rollback on invalid
   emitted syntax.
7. GC/lifetime tests force collection and frame/provider teardown between return,
   transaction, and cache operations; no token/span/source handle may dangle.
8. Source/binary parity tests execute one provider through both artifact forms,
   compare tokens, spans, diagnostics, cache keys, and derive results, and
   reject a binary missing executable role data or with a mismatched contract.

## Shared-file dependency map

`compile_tool.h` and `compile_tool_descriptor.c` own role IDs, module/type and
callable schemas, effects, and contract hashes. `compile_tool_evaluator.c` owns
descriptor dispatch and opaque native handle validation. `compile_time_executor.*`
owns role signature checks, execution-scope entry, frame roots, quote/emit calls,
and promotion into expansion transactions. `attribute_contract.*` and parser
declaration code own role schemas, raw token capture, lexical ordering, and
consumed-state diagnostics. `compiler.h`/state own provider metadata, token
arenas, cache identity, and roots. `compile_statement.c` plus core function/I/O
artifact code own source/binary provider loading and executable role metadata.
AST/lexer/location modules own token input and source provenance; the declaration
patch/expansion transaction owns detached derive validation and publication.
Tests and CMake are shared integration surfaces owned by root.

## Promotion gate

Do not implement or advertise M4 production APIs until M1 and M2 have passed
their resource, source-lifetime, immutable-view, round-trip, and append-only
validation gates, and M3 has a deterministic expansion scheduler. This contract
must be reviewed against the broad macro plan and Rust references before any
role ID or artifact schema is frozen. No C-only facade, one smoke test, source
name special case, or display/re-lex shortcut satisfies this milestone.
