---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/include/zr_vm_parser/semantic_source_metadata.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_source_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_values.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_value_children.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/include/zr_vm_parser/semantic_source_metadata.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_source_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_values.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_value_children.c
plan_sources:
  - user: 2026-09-26 approved LSP experience repair
  - .codex/plans/20260926-lsp-experience-repair.md
  - docs/zr_language_specification.md
tests:
  - tests/parser/test_reference_fact_emission.c
  - tests/parser/test_reference_field_fact_cases.h
  - tests/parser/test_expression_fragment_parser.c
  - tests/parser/test_compiler_features.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_source_metadata_cases.h
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_field_reference_cases.h
  - tests/acceptance/2026-09-26-lsp-experience-repair.md
doc_type: module-detail
---

# Member references and source metadata

The parser owns field reference facts, deterministic `using` cleanup records,
and template segment records. Compilation and semantic analysis consume these
same producers. LSP requests project the completed semantic context; they do
not recover missing identities by searching names or source ranges.

## Field references

`infer_primary_member_chain_type` resolves each member through the existing
prototype graph. Before an ordinary source field is published, the inference
layer checks its declaring owner and the current lexical type. Public fields
are visible; private fields require the same declaring type; protected fields
require the declaring type or a derived accessing type. Access through an
instance outside the declaring type also requires a receiver derived from the
accessing type. Only the first member in `super.value` receives the direct
inherited-receiver exemption. A later member in `super.other.value` is checked
against the immediate receiver's type. Closed
generic owners can match through their source declaration identity.
An ordinary field of a class type produces an instance value for the next
member in a chain. Only module type-member projections propagate a prototype
receiver; `box.child.value` therefore resolves as two ordinary field accesses.

`type_inference_member_facts.c` publishes the resolved source field or property
target. A field reuses `SZrTypeMemberInfo.symbolId` or the semantic symbol whose
AST is the exact field declaration. If compilation has not registered that
declaration yet, the producer registers it once. A class field's definition
range is its name token; the reference records its type and ownership. Existing
property symbol and value type IDs remain authoritative.
An existing field declaration whose exact type is unavailable retains its
invalid type ID. Resolving that field's identity never upgrades it to a guessed
`object` type; exact-type consumers continue to fail closed.

Reference publication uses AST node plus reference kind as its key. Repeated
inference cannot append an unresolved replacement for a resolved member fact.
Assignment infers the target through the same member chain, then copies the
resolved read target into the write fact. Compound assignment therefore keeps
both read and write facts, with the same declaration identity. Dynamic or
missing members retain unresolved facts; inaccessible fields retain errors.
An assignment propagates an error raised while inferring its target, so an
inaccessible write cannot be mistaken for successful expression inference.

## Shared source metadata

`semantic_source_metadata.c` is the common producer for compiler and analyzer
metadata. Its resource type helper first consults the active lexical type
environment, then uses shared expression inference for nonidentifier resources.
The cleanup builder accepts only `ZR_USING_GUARD_DROP`; pattern and plugin
guards retain their own compilation and analysis paths.

A cleanup record references the existing `SZrTypeBinding.symbolId` of an
identifier resource. It never creates a new symbol for the resource use and
never searches the analyzer's global symbol table by name. Shadowed resources
therefore have distinct cleanup targets. Nonidentifier resources can have an
invalid symbol ID without inventing a declaration.

Cleanup records preserve lexical declaration order, allocate a lifetime region,
use that region as their owner region, and retain ownership and builtin cleanup
kind. Unique, shared, and borrowed resources select drop; loaned resources select
return-loan metadata, with the compiler's existing rejection of mutable `ref`
resources unchanged. The existing close/destructor flags are preserved.

The template builder copies the AST segment sequence, including empty leading
and trailing static segments. Interpolations retain their expression AST rather
than a flattened string. Each traversal publishes the sequence once. Cached
analysis reuses its context; forced analysis creates a fresh context, so neither
route accumulates cleanup steps or template segments.

`semantic_analyzer_value_children.c` owns expression-child traversal in the
existing lexical/type environment. Arrays, object values and computed keys,
field initializers, parameter defaults, computed members, and constructor
arguments all reach the same template producer. Ordinary identifier property
names are not treated as evaluated expressions. The main dispatcher retains
scope ownership: lambda bodies enter the existing callable helper, which
registers their parameters before checking defaults and bodies. A shadowing
lambda resource therefore supplies its own canonical cleanup identity and
ownership instead of the enclosing function parameter's identity.

## Interpolation boundaries

The interpolation boundary scanner skips line and block comments outside
quoted strings before examining braces or quote delimiters. Braces and quotes
inside a comment cannot close an interpolation or start a string. The ordinary
expression parser still validates the extracted expression. An unterminated
comment that consumes the interpolation terminator fails boundary scanning.

The regression matrix checks both previously broken forms,
`${1 + /* } */ 2}` and `${1 /* } */ + 2}`, plus line comments and comment quotes.
Each must parse into exactly an empty string, the complete binary expression,
and an empty string.

## Reference evidence and validation

The field visibility model follows the shared owner and protected-receiver
rules in `lua/roslyn/src/Compilers/CSharp/Portable/Binder/Semantics/AccessCheck.cs`
(private and protected checks) and
`lua/jdk/src/jdk.compiler/share/classes/com/sun/tools/javac/comp/Resolve.java`.
The JDK `test/langtools/tools/javac/protectedAccess/ProtectedMemberAccess*.java`
tests provide positive and negative receiver cases. Static member access through
an expression retains Zr's existing behavior.

QuickJS's interpolation parser in `lua/QuickJS-master/quickjs.c` delegates to
ordinary token scanning, which skips comments before structural tokens.
`lua/cpython/Lib/test/test_fstring.py` supplies interpolation comment cases with
braces, quotes, and swallowed terminators. Zr keeps its own `${...}` syntax.

Validation proceeds from parser segment shape and shared reference facts to
analyzer metadata and LSP navigation/diagnostics. Tests cover inherited and
static targets, read and write identities, repeated inference, private owner
access, protected receiver boundaries, missing fields, shadowed resources,
ownership, lifetime/order, and cached versus forced analysis. The acceptance
record owns measured toolchain results and any remaining gates.

An isolated build of the pre-field source archive reproduces 22 compiler
integration failures and the one resource-value-parameter failure. Enforcing
private field access exposed one additional invalid layout fixture: `WidePoint`
read its default-private `a`, `e`, and `j` fields from outside the struct. Those
three fixture fields are explicitly public; constructor, byte-layout, and
scalar-temporary assertions are unchanged. The access check remains strict.
