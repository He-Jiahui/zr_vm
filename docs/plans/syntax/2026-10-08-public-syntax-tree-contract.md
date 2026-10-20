---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/include/zr_vm_parser/location.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/include/zr_vm_parser/syntax_contract.h
  - zr_vm_parser/include/zr_vm_parser/syntax_context.h
  - zr_vm_parser/include/zr_vm_parser/token_stream.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser_ownership.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_union.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c
  - zr_vm_parser/src/zr_vm_parser/parser/ast.c
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_syntax_tree.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental/incremental_syntax_reparse.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/syntax.h
  - zr_vm_parser/src/zr_vm_parser/syntax.c
  - zr_vm_parser/src/zr_vm_parser/syntax_clone.c
  - zr_vm_parser/src/zr_vm_parser/syntax_origin.c
  - zr_vm_parser/src/zr_vm_parser/syntax_validate.c
  - zr_vm_parser/src/zr_vm_parser/syntax_format.c
  - zr_vm_parser/src/zr_vm_parser/syntax_tokens.c
plan_sources:
  - user: 2026-10-07 M2 public immutable zr.compile.syntax views, parsing, derivation, replacement, token round trips, and comment-preserving formatting
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - docs/plans/syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md
  - lua/roslyn/src/Compilers/Core/Portable/Syntax/SyntaxNode.cs
  - lua/roslyn/src/Compilers/Core/Portable/Syntax/SyntaxNodeExtensions.cs
  - lua/roslyn/src/Compilers/CSharp/Portable/Syntax/SyntaxFactory.cs
  - lua/roslyn/src/Compilers/CSharp/Portable/Syntax/SyntaxNormalizer.cs
tests:
  - tests/parser/test_syntax_tree_contract.c
  - tests/parser/test_syntax_tree_tokens.c
  - tests/parser/test_syntax_tree_members.c
  - tests/parser/test_syntax_tree_replacement.c
  - tests/compileTime/test_compile_syntax_contract.c
  - tests/compileTime/test_compile_syntax_rollback.c
  - tests/compileTime/test_compile_syntax_formatting.c
doc_type: milestone-detail
---

# Public Immutable Syntax Tree Contract

## Status and gate

This document is the M2 design and test specification for `zr.compile.syntax`.
It is intentionally design-only until the root agent accepts the M1
`token_stream.h` contract. No M2 production implementation may be merged before
that gate. The document records the parser facts that constrain the facade and
the exact public C boundary that the implementation should target. The language
surface shown below is the projection consumed by compile-only code; the C API
is the ownership and failure boundary used by the compiler host.

The design follows the macro plan's central rule: macro authors receive an
immutable syntax snapshot, and an expansion publishes a detached replacement
snapshot only after all validation succeeds. A mutable `SZrAstNode *` is never
stored in a public view, and an old snapshot is never modified by append or
replacement.

## Goals and non-goals

M2 must provide:

1. An immutable `SyntaxNode` view for every current parser AST kind, including
   typed child and member collections, source spans, origin, and stable identity.
2. Full-input `parseType` and category-aware `parseMembers` over the M1 lossless
   token stream. Parse failures are transactional and cannot publish a partial
   tree.
3. Persistent `replace` and append-only `appendMembers` operations. A derived
   type contains the complete original type followed by legal additions inside
   that type's existing AST collection.
4. `toTokens` and source formatting that preserve original token and comment text,
   use four spaces for generated indentation, and emit LF line endings.
5. Validation and tests for every supported member collection, identity/origin
   rules, malformed input, wrong-container additions, collisions, allocation
   failure, and rollback.

M2 does not provide:

- a writable AST view, compiler pointer, runtime object, native pointer, or source
  buffer borrowed from `SZrParserState`;
- an `impl` block or an alternate member container for enum or union declarations;
- a replacement operation that silently removes, renames, reorders, or changes
  the signature of an original member during derive;
- a round-trippable replacement for the existing diagnostic `.zrs` syntax-tree
  writer; that writer remains a debug projection;
- semantic binding, type checking, layout, or code generation. Those consumers
  receive the published parsed AST after M2 validation.

## Existing parser facts

### Owning AST is not a public view

`zr_vm_parser/include/zr_vm_parser/ast.h:18-165` defines the stable
`EZrAstNodeType` values. `SZrAstNode` at `ast.h:1047-1176` stores `type`, a
borrowed `SZrFileRange`, a shared `SZrAstOwnership *`, and a large union of
mutable pointers. There is no node-level parent link, node id, token/trivia
list, source-byte snapshot, or origin field; hygiene is carried only by some
nested identifier payloads rather than by every node.
`SZrAstNodeArray` at `ast.h:176-187` owns only a pointer buffer. The owning tree,
not the array, recursively frees its elements.

`ZrParser_AstNodeArray_Add` at `ast.h:1197-1212` retains a `void` compatibility
signature; a failed allocation therefore does not report whether the node was
accepted. M2 builders must use a checked private vector (or the M1 budgeted
`ZrParser_AstNodeArray_TryAddBudget` boundary once that gate is accepted), and
must not treat a call to the legacy `Add` as a commit acknowledgement. Every
array constructor/append path must check count-plus-one, pointer-byte
multiplication, and capacity growth before reserving memory. The budgeted
constructor reserves the container and backing pointer bytes as one transaction;
the budgeted append reserves replacement backing bytes, commits before swapping
the pointer, and leaves pointer, capacity, count, and the unadopted node
unchanged on failure. A public immutable builder must never publish a candidate
through an unchecked append.

`ZrParser_AstNodeArray_Free` releases only the pointer buffer and its container;
it deliberately does not recursively destroy the pointed-to AST nodes. The
owning AST destructor must therefore free each adopted node exactly once and
must retain ownership of a node until the append transaction commits. Candidate
clone/derive rollback must free detached arrays and nodes in reverse ownership
order, while borrowed view arrays must never call the owning-node destructor.
This distinction needs an explicit audit for every array field in the AST union,
including arrays nested inside parameters, types, properties, and union payloads.

The same rule applies to every candidate clone and derive transaction: reserve
the complete vector before publishing it, retain the old count and source-order
ids, and destroy every detached child that was not transferred when a later
allocation or validation step fails. A shallow copy of `SZrAstNode` is not a
clone: its union contains owning child pointers, `SZrType` allocations,
parameter/generic arrays, and an `SZrAstOwnership` reference. The clone layer
must either recursively copy those payloads into a fresh owner or share only
immutable records whose lifetime is explicitly retained by the document.

The current destructor switch is not yet a complete proof for the public
facade. `SZrUnpackLiteral.element` is an owned AST child but has no matching
`ZR_AST_UNPACK_LITERAL` free case, and `parser_types.c` constructs a
`ZR_AST_META_IDENTIFIER` whose owned `name` is called out as a destructor TODO.
Before advertising views for every concrete AST kind, the clone/free audit must
close those cases (or explicitly classify any enum-only wire values as
unsupported standalone nodes) and add allocation-failure coverage for each
member collection.

`ZrParser_Ast_Free` in
`zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c` recursively frees
embedded identifier and parameter owners by recovering the enclosing node with
`offsetof`. A shallow copy is therefore invalid: it aliases nested owners and
causes double frees or use-after-free. M2 requires a complete, type-dispatched
deep clone layer before a persistent snapshot can be published. There is no
existing AST clone API.

`zr_vm_parser/include/zr_vm_parser/syntax_contract.h` is a semantic callable
contract helper. It normalizes parameter passing and receiver effects; it is not
the syntax facade and must remain a separate header. The new public header is
`zr_vm_parser/include/zr_vm_parser/syntax.h`.

### Parser entry points and internal hooks

`zr_vm_parser/include/zr_vm_parser/parser.h:29-75` exposes whole-script parsing,
one top-level statement, and one expression. `ZrParser_State_Init` borrows the
source bytes and `ZrParser_State_Free` releases only the lexer. A normal parser
error may still return a recovering AST, and `parse_script` clears `hasError`
between recovery attempts. The M2 adapter therefore cannot inspect only the
final flags: it must install an isolated diagnostic sink or error counter, fail
if any recovery diagnostic or fatal flag was observed, and verify end-of-input
itself before publishing the snapshot.

The reusable internal hooks are declared in
`zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h`:

- `parse_type` and `parse_type_no_generic` at lines 364-366;
- `parse_parameter` and `parse_parameter_list` at lines 384-386;
- struct field, method, and meta parsers at lines 462-466;
- struct and class declaration parsers at lines 470-472;
- interface field, method, property, meta, and declaration parsers at lines
  474-482;
- enum member and declaration parsers at lines 484-486;
- union variant and declaration parsers at lines 488-490;
- class field, method, property, getter, setter, and meta parsers at lines
  502-517.

These functions consume a live `SZrParserState`; they are not safe public APIs
until wrapped by a source-owning snapshot and a full-consumption check. The M2
adapter should keep the static helpers private and add narrow public wrappers,
rather than exporting `parser_internal.h`.

`parser_types.c` owns the type grammar, including ownership/reference qualifiers,
arrays, generic arguments, tuple types, function types, and array-size
constraints. It returns an owning `SZrType *`, so the clone/free layer must cover
all `SZrType` children and `arraySizeExpression` nodes.

`parser_struct.c` and `parser_class.c` already classify members in source order.
The struct loop dispatches fields, methods, meta functions, and unified property
declarations. The class loop dispatches fields, methods, old split property forms,
meta functions, and unified property declarations. The class classifier probes
the parser and may allocate decorators while probing; M2 must parse detached
input once and must not reuse that probe as an immutable validation primitive.

`parser_interface.c` parses interface field declarations, method signatures,
property signatures, and meta signatures. `parser_extern.c` parses enum
declarations and enum members. `parser_union.c` parses only unit, tuple, and
struct union variants; tuple and struct payloads are `ZR_AST_PARAMETER` nodes.
No parser file defines an implementation-block AST kind.

### Source, token, and trivia gaps

`SZrFileRange` in `location.h:11-25` stores byte offsets and line/column values
but only borrows a source-name VM string. It cannot keep source bytes alive.
`SZrLexState` in `lexer.h:152-188` borrows source bytes and has only current and
one-token lookahead state. `lexer.c`'s `skip_whitespace_and_comments` consumes
both whitespace and `//` or `/* */` comments without retaining them. Consequently
the current AST cannot reproduce comments or original spacing.

M1 `token_stream.h` is the required lossless source boundary. It must retain an
owned source snapshot, token spelling, punctuation assembly, leading/trailing
trivia, byte spans, line mapping, source provenance, and hygiene context. The
M2 header below accepts only `const SZrTokenStream *` or creates one through an
explicit source-copy constructor; it never stores pointers into `SZrLexState`.

`writer_syntax_tree.c` writes a diagnostic tree with two-space indentation,
selected field values, and line/column data. It omits token spelling and trivia,
and its `.zrs` output is explicitly a debug snapshot rather than a reloadable
source representation. `toTokens` must be implemented in a dedicated syntax
serializer over the M1 token stream, not by extending that writer.

The language-server incremental reparser at
`zr_vm_language_server/src/zr_vm_language_server/incremental/incremental_syntax_reparse.c`
mutates a script member pointer in place after an equal-length edit. It is useful
as a parser reuse reference, but it cannot supply persistent identity, source
ownership, or atomic replacement for this API.

## Complete current syntax-kind inventory

The public kind value is a stable projection of `EZrAstNodeType`; removed wire
values remain reserved and are never reused. The view must expose all of the
following kinds, even when a kind is only reachable as a nested child. A kind
view may expose no writable fields, but it must still report its kind, span,
origin, identity, and all existing children.

| Category | Current `EZrAstNodeType` values |
| --- | --- |
| Roots and declarations | `ZR_AST_SCRIPT`, `ZR_AST_MODULE_DECLARATION`, `ZR_AST_STRUCT_DECLARATION`, `ZR_AST_CLASS_DECLARATION`, `ZR_AST_INTERFACE_DECLARATION`, `ZR_AST_ENUM_DECLARATION`, `ZR_AST_FUNCTION_DECLARATION`, `ZR_AST_VARIABLE_DECLARATION`, `ZR_AST_COMPILE_TIME_DECLARATION`, `ZR_AST_EXTERN_BLOCK`, `ZR_AST_EXTERN_FUNCTION_DECLARATION`, `ZR_AST_EXTERN_DELEGATE_DECLARATION` |
| Struct members | `ZR_AST_STRUCT_FIELD`, `ZR_AST_STRUCT_METHOD`, `ZR_AST_STRUCT_META_FUNCTION` |
| Class members | `ZR_AST_CLASS_FIELD`, `ZR_AST_CLASS_METHOD`, `ZR_AST_CLASS_PROPERTY`, `ZR_AST_CLASS_META_FUNCTION` |
| Interface members | `ZR_AST_INTERFACE_FIELD_DECLARATION`, `ZR_AST_INTERFACE_METHOD_SIGNATURE`, `ZR_AST_INTERFACE_PROPERTY_SIGNATURE`, `ZR_AST_INTERFACE_META_SIGNATURE` |
| Enum and union | `ZR_AST_ENUM_MEMBER`, `ZR_AST_UNION_DECLARATION`, `ZR_AST_UNION_VARIANT` |
| Unified properties | `ZR_AST_PROPERTY_GET`, `ZR_AST_PROPERTY_SET`, `ZR_AST_PROPERTY_DECLARATION`, `ZR_AST_PROPERTY_ACCESSOR` |
| Expressions | `ZR_AST_ASSIGNMENT_EXPRESSION`, `ZR_AST_BINARY_EXPRESSION`, `ZR_AST_LOGICAL_EXPRESSION`, `ZR_AST_CONDITIONAL_EXPRESSION`, `ZR_AST_UNARY_EXPRESSION`, `ZR_AST_TYPE_CAST_EXPRESSION`, `ZR_AST_LAMBDA_EXPRESSION`, `ZR_AST_IF_EXPRESSION`, `ZR_AST_SWITCH_EXPRESSION`, `ZR_AST_FUNCTION_CALL`, `ZR_AST_MEMBER_EXPRESSION`, `ZR_AST_PRIMARY_EXPRESSION`, `ZR_AST_IMPORT_EXPRESSION`, `ZR_AST_TYPE_QUERY_EXPRESSION`, `ZR_AST_TYPE_LITERAL_EXPRESSION`, `ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION`, `ZR_AST_CONSTRUCT_EXPRESSION`, `ZR_AST_STRUCT_INIT_EXPRESSION`, `ZR_AST_AWAIT_EXPRESSION`, `ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION`, `ZR_AST_SPREAD_ARGUMENT` |
| Literals | `ZR_AST_BOOLEAN_LITERAL`, `ZR_AST_INTEGER_LITERAL`, `ZR_AST_FLOAT_LITERAL`, `ZR_AST_STRING_LITERAL`, `ZR_AST_TEMPLATE_STRING_LITERAL`, `ZR_AST_CHAR_LITERAL`, `ZR_AST_NULL_LITERAL`, `ZR_AST_IDENTIFIER_LITERAL`, `ZR_AST_INTERPOLATED_SEGMENT` |
| Compound literals | `ZR_AST_ARRAY_LITERAL`, `ZR_AST_OBJECT_LITERAL`, `ZR_AST_KEY_VALUE_PAIR`, `ZR_AST_UNPACK_LITERAL`, `ZR_AST_GENERATOR_EXPRESSION` |
| Statements and loops | `ZR_AST_BLOCK`, `ZR_AST_EXPRESSION_STATEMENT`, `ZR_AST_USING_STATEMENT`, `ZR_AST_RETURN_STATEMENT`, `ZR_AST_BREAK_CONTINUE_STATEMENT`, `ZR_AST_THROW_STATEMENT`, `ZR_AST_OUT_STATEMENT`, `ZR_AST_CATCH_CLAUSE`, `ZR_AST_TRY_CATCH_FINALLY_STATEMENT`, `ZR_AST_WHILE_LOOP`, `ZR_AST_FOR_LOOP`, `ZR_AST_FOREACH_LOOP`, `ZR_AST_YIELD_STATEMENT` |
| Switch | `ZR_AST_SWITCH_CASE`, `ZR_AST_SWITCH_DEFAULT` |
| Types and parameters | `ZR_AST_TYPE`, `ZR_AST_FUNCTION_TYPE`, `ZR_AST_GENERIC_TYPE`, `ZR_AST_TUPLE_TYPE`, `ZR_AST_GENERIC_DECLARATION`, `ZR_AST_PARAMETER`, `ZR_AST_PARAMETER_LIST` |
| Destructuring and metadata | `ZR_AST_DESTRUCTURING_OBJECT`, `ZR_AST_DESTRUCTURING_ARRAY`, `ZR_AST_DECORATOR_EXPRESSION`, `ZR_AST_META_IDENTIFIER`, `ZR_AST_ACCESS_MODIFIER` |

The inventory includes `ZR_AST_PROPERTY_DECLARATION` and
`ZR_AST_PROPERTY_ACCESSOR` in addition to the legacy `ZR_AST_CLASS_PROPERTY`,
`ZR_AST_PROPERTY_GET`, and `ZR_AST_PROPERTY_SET`. A view must report the exact
stored kind; it must not normalize old and unified property forms into one
synthetic kind. Removed intermediate/test wire values remain reserved in the
underlying enum and are not exposed as constructible kinds.

## Immutable snapshot and identity model

### Owning document

The implementation owns one `SZrSyntaxDocument` per immutable root. It contains:

- an owned byte copy of the source input, including the original newline bytes;
- an M1 immutable `SZrTokenStream` retaining every code token and trivia segment;
- a deep-cloned AST arena whose nodes are private and never returned as mutable
  pointers;
- an immutable node table mapping stable ids to parent slot, kind, AST payload,
  token span, and source origin;
- a lineage id, root generation, and allocator/accounting metadata.

The document is reference counted. `ZrParser_SyntaxDocument_Release` is the only
destruction operation; releasing a parser state or the caller's source buffer
does not invalidate a document or any view copied from it. A view is a small
handle containing the document reference, lineage id, root generation, and
`TZrSyntaxNodeId`. Child access returns another retained handle or a read-only
borrow valid for the duration of the call; it never returns an AST address.

The node table is the owner of the public identity. Raw addresses, array indexes,
and source offsets are not identities because arrays can be rebuilt and generated
text has no original offset.

### Stable ids and origins

Original nodes receive deterministic ids during import, using a document lineage
id plus preorder ordinal. The ordinal is only an allocation seed; the published
id must remain stable when a derived tree reuses an unchanged node. A derived
snapshot copies all unchanged ids, and allocates new ids from its lineage counter
for generated or replaced nodes. The same node id in two snapshots means the
same logical original node, not the same memory address.

Every node and token has an origin record:

```text
OriginKind = Source | Generated | Replaced
SourceOrigin     = sourceId, byteSpan, lineSpan
GeneratedOrigin  = callSiteSpan, definitionSpan?, parentNodeId, expansionOrdinal
ReplacedOrigin   = replacementCallSite, replacedNodeId, sourceId?
```

Source origins refer to the owned source snapshot and retain exact byte spans.
Generated tokens have no fake source offset; their span is a synthetic insertion
span and their call-site/definition provenance is explicit. Hygiene and syntax
context are copied from M1 token trees, never inferred from identifier spelling.

An operation that accepts a node handle checks lineage, generation, and parent
membership. A stale handle returns `ZR_SYNTAX_ERROR_STALE_NODE` rather than
silently targeting a node with the same array position. Reading an old snapshot
remains valid; only using its handle as a mutation target for a different
snapshot is rejected.

### Read-only fields and collections

`SZrSyntaxNodeView` exposes scalar fields through value-returning accessors and
children through `SZrSyntaxNodeListView`. A list has a count and an index accessor;
there is no `Add`, `Remove`, capacity, mutable slot, or pointer-to-element API.
Nested identifiers, types, parameters, decorators, expressions, bodies,
accessors, generic declarations, and union payload fields are all returned as
views. Names and literal spellings are immutable byte/string slices owned by the
document or token stream.

The facade does not expose the internal `SZrAstNode` union. Compiler code that
needs the legacy AST receives a controlled, read-only projection only after the
snapshot commit; it must not retain a mutable pointer across a syntax operation.

## Proposed `syntax.h` C API

The following is the exact M2 boundary. Names in `token_stream.h` are the M1
names expected by this contract; if M1 settles on an equivalent spelling, only
the adapter typedefs in `syntax.h` should change.

```c
#ifndef ZR_VM_PARSER_SYNTAX_H
#define ZR_VM_PARSER_SYNTAX_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/token_stream.h"

typedef struct SZrSyntaxDocument SZrSyntaxDocument;
typedef struct SZrSyntaxNodeHandle SZrSyntaxNodeHandle;
typedef struct SZrSyntaxDiagnosticList SZrSyntaxDiagnosticList;
typedef struct SZrSyntaxNodeListView SZrSyntaxNodeListView;

typedef TZrUInt64 TZrSyntaxDocumentId;
typedef TZrUInt64 TZrSyntaxNodeId;

typedef enum EZrSyntaxOriginKind {
    ZR_SYNTAX_ORIGIN_SOURCE = 0,
    ZR_SYNTAX_ORIGIN_GENERATED = 1,
    ZR_SYNTAX_ORIGIN_REPLACED = 2
} EZrSyntaxOriginKind;

typedef enum EZrSyntaxParseCategory {
    ZR_SYNTAX_PARSE_TYPE = 0,
    ZR_SYNTAX_PARSE_MEMBERS = 1,
    ZR_SYNTAX_PARSE_EXPRESSION = 2,
    ZR_SYNTAX_PARSE_STATEMENT = 3,
    ZR_SYNTAX_PARSE_ITEM = 4
} EZrSyntaxParseCategory;

typedef enum EZrSyntaxContainerKind {
    ZR_SYNTAX_CONTAINER_STRUCT = 0,
    ZR_SYNTAX_CONTAINER_CLASS = 1,
    ZR_SYNTAX_CONTAINER_INTERFACE = 2,
    ZR_SYNTAX_CONTAINER_ENUM = 3,
    ZR_SYNTAX_CONTAINER_UNION = 4
} EZrSyntaxContainerKind;

typedef enum EZrSyntaxReplacePolicy {
    ZR_SYNTAX_REPLACE_CHILD = 0,
    ZR_SYNTAX_REPLACE_DECLARATION = 1,
    ZR_SYNTAX_REPLACE_DERIVE_APPEND_ONLY = 2
} EZrSyntaxReplacePolicy;

typedef enum EZrSyntaxError {
    ZR_SYNTAX_OK = 0,
    ZR_SYNTAX_ERROR_ARGUMENT,
    ZR_SYNTAX_ERROR_OOM,
    ZR_SYNTAX_ERROR_LEX,
    ZR_SYNTAX_ERROR_PARSE,
    ZR_SYNTAX_ERROR_TRAILING_INPUT,
    ZR_SYNTAX_ERROR_DIAGNOSTIC,
    ZR_SYNTAX_ERROR_WRONG_CATEGORY,
    ZR_SYNTAX_ERROR_WRONG_CONTAINER,
    ZR_SYNTAX_ERROR_ILLEGAL_MEMBER,
    ZR_SYNTAX_ERROR_COLLISION,
    ZR_SYNTAX_ERROR_SIGNATURE_CHANGE,
    ZR_SYNTAX_ERROR_MEMBER_REORDER,
    ZR_SYNTAX_ERROR_ORIGINAL_REMOVED,
    ZR_SYNTAX_ERROR_FOREIGN_NODE,
    ZR_SYNTAX_ERROR_STALE_NODE,
    ZR_SYNTAX_ERROR_INVALID_ORIGIN,
    ZR_SYNTAX_ERROR_UNSUPPORTED_KIND,
    ZR_SYNTAX_ERROR_LIMIT
} EZrSyntaxError;

typedef struct SZrSyntaxParseOptions {
    TZrBool requireFullInput;       /* M2 always sets this true for public APIs. */
    TZrBool preserveSourceText;     /* required for toTokens and origins. */
    TZrBool allowRecovery;          /* false for transform/derive input. */
    TZrBool enableLegacyMigration;  /* false for production syntax transforms. */
    EZrSyntaxContainerKind memberContainer;
} SZrSyntaxParseOptions;

typedef struct SZrSyntaxSourceSpan {
    TZrSyntaxDocumentId documentId;
    TZrSize startByte;
    TZrSize endByte;
    TZrInt32 startLine;
    TZrInt32 startColumn;
    TZrInt32 endLine;
    TZrInt32 endColumn;
} SZrSyntaxSourceSpan;

typedef struct SZrSyntaxOrigin {
    EZrSyntaxOriginKind kind;
    SZrSyntaxSourceSpan source;
    SZrSyntaxSourceSpan callSite;
    SZrSyntaxSourceSpan definition;
    TZrSyntaxNodeId parentId;
    TZrUInt64 expansionOrdinal;
    SZrSyntaxContext syntaxContext;
} SZrSyntaxOrigin;

typedef struct SZrSyntaxNodeInfo {
    TZrSyntaxNodeId id;
    EZrAstNodeType kind;
    TZrUInt64 generation;
    SZrSyntaxSourceSpan span;
    SZrSyntaxSourceSpan fullSpan;
    SZrSyntaxOrigin origin;
} SZrSyntaxNodeInfo;

typedef struct SZrSyntaxMemberAppendOptions {
    EZrSyntaxReplacePolicy policy;  /* must be DERIVE_APPEND_ONLY for derive. */
    TZrBool rejectCollisions;
    TZrBool preserveOriginalOrder;
    TZrBool normalizeGeneratedTrivia;
} SZrSyntaxMemberAppendOptions;

/* Document ownership and immutable root access. */
ZR_PARSER_API SZrSyntaxDocument *ZrParser_SyntaxDocument_Retain(
        SZrSyntaxDocument *document);
ZR_PARSER_API void ZrParser_SyntaxDocument_Release(
        SZrSyntaxDocument *document);
ZR_PARSER_API SZrSyntaxDocumentId ZrParser_SyntaxDocument_Id(
        const SZrSyntaxDocument *document);
ZR_PARSER_API TZrUInt64 ZrParser_SyntaxDocument_Generation(
        const SZrSyntaxDocument *document);
ZR_PARSER_API TZrBool ZrParser_SyntaxDocument_Root(
        const SZrSyntaxDocument *document,
        SZrSyntaxNodeHandle **outRoot);

/* Handles and list views are opaque retained objects; callers never stack-allocate
 * an incomplete struct or borrow an AST address. */
ZR_PARSER_API SZrSyntaxNodeHandle *ZrParser_SyntaxNode_Retain(
        SZrSyntaxNodeHandle *node);
ZR_PARSER_API void ZrParser_SyntaxNode_Release(
        SZrSyntaxNodeHandle *node);
ZR_PARSER_API SZrSyntaxNodeListView *ZrParser_SyntaxNodeList_Retain(
        SZrSyntaxNodeListView *list);
ZR_PARSER_API void ZrParser_SyntaxNodeList_Release(
        SZrSyntaxNodeListView *list);

/* Full-input parsing over an immutable M1 token stream. */
ZR_PARSER_API EZrSyntaxError ZrParser_Syntax_ParseType(
        SZrState *state,
        const SZrTokenStream *input,
        const SZrSyntaxParseOptions *options,
        SZrSyntaxDocument **outDocument,
        SZrSyntaxNodeHandle **outType,
        SZrSyntaxDiagnosticList **outDiagnostics);
ZR_PARSER_API EZrSyntaxError ZrParser_Syntax_ParseMembers(
        SZrState *state,
        const SZrTokenStream *input,
        EZrSyntaxContainerKind containerKind,
        const SZrSyntaxParseOptions *options,
        SZrSyntaxDocument **outDocument,
        SZrSyntaxNodeListView **outMembers,
        SZrSyntaxDiagnosticList **outDiagnostics);
ZR_PARSER_API EZrSyntaxError ZrParser_Syntax_ParseCategory(
        SZrState *state,
        const SZrTokenStream *input,
        EZrSyntaxParseCategory category,
        const SZrSyntaxParseOptions *options,
        SZrSyntaxDocument **outDocument,
        SZrSyntaxNodeHandle **outNode,
        SZrSyntaxDiagnosticList **outDiagnostics);

/* Read-only node and child/member access. */
ZR_PARSER_API TZrBool ZrParser_SyntaxNode_Info(
        const SZrSyntaxNodeHandle *node,
        SZrSyntaxNodeInfo *outInfo);
ZR_PARSER_API TZrBool ZrParser_SyntaxNode_Children(
        const SZrSyntaxNodeHandle *node,
        SZrSyntaxNodeListView **outChildren);
ZR_PARSER_API TZrBool ZrParser_SyntaxNode_Members(
        const SZrSyntaxNodeHandle *node,
        SZrSyntaxNodeListView **outMembers);
ZR_PARSER_API TZrSize ZrParser_SyntaxNodeList_Count(
        const SZrSyntaxNodeListView *list);
ZR_PARSER_API TZrBool ZrParser_SyntaxNodeList_At(
        const SZrSyntaxNodeListView *list,
        TZrSize index,
        SZrSyntaxNodeHandle **outNode);
ZR_PARSER_API TZrBool ZrParser_SyntaxNode_IsSameLogicalNode(
        const SZrSyntaxNodeHandle *left,
        const SZrSyntaxNodeHandle *right);

/* Persistent edits. All output documents are detached and atomically published. */
ZR_PARSER_API EZrSyntaxError ZrParser_SyntaxNode_Replace(
        SZrState *state,
        const SZrSyntaxNodeHandle *oldNode,
        const SZrSyntaxNodeHandle *replacement,
        EZrSyntaxReplacePolicy policy,
        SZrSyntaxDocument **outDocument,
        SZrSyntaxNodeHandle **outRoot,
        SZrSyntaxDiagnosticList **outDiagnostics);
ZR_PARSER_API EZrSyntaxError ZrParser_SyntaxNode_AppendMembers(
        SZrState *state,
        const SZrSyntaxNodeHandle *targetType,
        const SZrSyntaxNodeListView *additions,
        const SZrSyntaxMemberAppendOptions *options,
        SZrSyntaxDocument **outDocument,
        SZrSyntaxNodeHandle **outRoot,
        SZrSyntaxDiagnosticList **outDiagnostics);

/* Lossless tokens and deterministic source formatting. */
ZR_PARSER_API EZrSyntaxError ZrParser_SyntaxNode_ToTokens(
        const SZrSyntaxNodeHandle *node,
        SZrTokenStream **outTokens);
ZR_PARSER_API EZrSyntaxError ZrParser_SyntaxNode_ToSource(
        const SZrSyntaxNodeHandle *node,
        TZrBool normalizeWhitespace,
        SZrString **outSource);

ZR_PARSER_API const TZrChar *ZrParser_SyntaxError_Name(EZrSyntaxError error);
ZR_PARSER_API void ZrParser_SyntaxDiagnostics_Release(
        SZrState *state,
        SZrSyntaxDiagnosticList *diagnostics);

#endif
```

`SZrSyntaxNodeHandle` and `SZrSyntaxNodeListView` are opaque handles, not public
struct layouts. The `SZrTokenStream` retain/release and token-view functions
remain owned by M1 (`ZrParser_TokenStream_Retain/Release`, `Count/At`, and
`ZrParser_TokenTree_*` accessors). Each token context is the full
`SZrSyntaxContext { table, markId }`, not a bare mark id. `ToTokens` returns a
retained immutable stream; callers must not obtain a writable token buffer or
mutate source text through it.

The source overload exposed to the compile-only language is a small adapter over
these C functions:

```text
syntax.parseType(input: TokenStream | string): TypeSyntax
syntax.parseMembers(input: TokenStream | string,
                    container: SyntaxContainerKind): ReadonlyList<SyntaxNode>
SyntaxNode.replace(oldNode, replacement, policy): SyntaxNode
SyntaxNode.appendMembers(additions): SyntaxNode
SyntaxNode.toTokens(): TokenStream
SyntaxNode.toSource(normalizeWhitespace = true): string
```

String input is copied into a new M1 token source. TokenStream input retains the
existing source lineage and hygiene contexts. Both forms require complete input;
there is no public recovery mode for a transform.

## Parsing and full-consumption contract

`parseType` wraps `parse_type` with an isolated parser state. It accepts one type
grammar and succeeds only when the next non-trivia token is EOS. It covers named,
generic, tuple, function, ownership/reference, array, readonly/decorator pseudo,
and constrained array forms already accepted by `parser_types.c`.

`parseMembers` creates a detached member-list document and selects the parser
dispatch from `EZrSyntaxContainerKind`:

| Target container | Legal additions |
| --- | --- |
| struct | `ZR_AST_STRUCT_FIELD`, `ZR_AST_STRUCT_METHOD`, `ZR_AST_STRUCT_META_FUNCTION`, `ZR_AST_PROPERTY_DECLARATION` |
| class | `ZR_AST_CLASS_FIELD`, `ZR_AST_CLASS_METHOD`, `ZR_AST_CLASS_PROPERTY`, `ZR_AST_CLASS_META_FUNCTION`, `ZR_AST_PROPERTY_DECLARATION` |
| interface | `ZR_AST_INTERFACE_FIELD_DECLARATION`, `ZR_AST_INTERFACE_METHOD_SIGNATURE`, `ZR_AST_INTERFACE_PROPERTY_SIGNATURE`, `ZR_AST_INTERFACE_META_SIGNATURE` |
| enum | `ZR_AST_ENUM_MEMBER` only |
| union | `ZR_AST_UNION_VARIANT` only; variant kind is unit, tuple, or struct |

The first two rows allow the full member grammar already accepted by the parser,
including methods, properties, fields, and `@meta` functions. The interface row
contains signatures only. Enum and union never accept methods, properties, meta
functions, or an external implementation block. A union variant's tuple/struct
payload remains the current parameter grammar; M2 does not invent a new variant
body shape.

The parser adapter must reject:

- trailing code tokens after the requested category, including a second member or
  an unconsumed delimiter;
- `hasError`, `hasFatalError`, lexical errors, or removed legacy syntax when
  `enableLegacyMigration` is false;
- a member category not legal for the selected container;
- a recovered or partially allocated node list;
- a token stream whose source owner is released or whose spans are not contiguous
  and within the owned source bounds.

Diagnostics use the existing structured diagnostic style but are copied into an
owned `SZrSyntaxDiagnosticList`. Diagnostic locations reference the syntax
document's source id and byte spans, not a borrowed `SZrFileRange.source`.

## Append-only derivation and validation transaction

`appendMembers` builds a private candidate root by cloning the target document's
node table, token stream metadata, and AST arena. It then validates all additions
before publishing one new document. The original document is unchanged on every
failure, including allocation failure, invalid syntax, collision, or a later
addition failing after an earlier addition appeared valid.

Validation order is deterministic:

1. Confirm the target handle belongs to the source document, is a declaration
   node, and has a supported container kind.
2. Confirm every addition is from the same syntax lineage or is an explicitly
   detached generated node with a valid call-site origin and syntax context.
3. Confirm each addition is in the container matrix above. Enum and union reject
   all method/property/meta/impl forms before cloning into the candidate list.
4. Snapshot the original member count, source-order node ids, and a complete
   structural fingerprint for every original member. The fingerprint includes
   the stored node kind, name, generic parameters, parameter passing forms/types,
   return type, property accessors, field type, enum/union payload shape, body,
   initializer, decorators/meta data, and the exact owned token/trivia spelling
   wherever the token stream has it. A derive operation must preserve every one
   of these facts. A signature fingerprint may be used in addition for collision
   checks, but it is not sufficient to prove that an original member was
   preserved: unchanged bodies, initializers, comments, and trivia must compare
   equal too. Source-origin remapping in a derived document is not itself a
   change; compare owned structure and token/trivia bytes after normalizing only
   document-local ids.
5. Check duplicate names and callable/property collisions using the existing
   semantic category rules. Duplicate additions are rejected as a transaction;
   no “last one wins” behavior is allowed.
6. Append additions after the original member list. Existing node ids, signatures,
   token order, and member order must compare byte-for-byte/logically equal to the
   source snapshot. New nodes receive generated origins and fresh ids.
7. Insert generated tokens immediately before the target's closing delimiter,
   preserving all original token and trivia order. Generated indentation uses four
   spaces and generated line endings use LF.
8. Recompute the derived target span and full span, reindex the candidate node
   table, and publish only if every ownership and invariant check succeeds.

For a derive operation, `ZR_SYNTAX_REPLACE_DERIVE_APPEND_ONLY` is mandatory.
Replacing an original declaration, deleting a member, renaming a member, changing
an original signature, or reordering an original member returns a specific error.
An attribute macro may request `ZR_SYNTAX_REPLACE_DECLARATION`, but it still must
pass parent-category validation and must produce a complete legal declaration.
The generic child policy permits a persistent syntax edit for tooling, but the
compiler expansion pipeline must select the derive policy explicitly.

## Replacement semantics

`replace` is persistent: it clones the path from the root to `oldNode`, shares
unchanged node records and token/trivia segments where the M1 stream permits,
and returns a new root generation. The old root and all old handles remain valid.

The replacement must satisfy the old node's parent slot category. For example, a
type slot accepts `ZR_AST_TYPE`, a member slot accepts only the legal member set
for its container, and a union variant slot accepts `ZR_AST_UNION_VARIANT`. The
operation rejects a foreign-document node, a stale handle, an unsupported kind,
or a generated node lacking origin/hygiene data.

Unchanged descendants retain their logical ids and source origins. The replaced
node receives a new id and a `Replaced` origin pointing to the old id. Leading and
trailing trivia are carried from the old node unless the replacement explicitly
supplies trivia in its token stream. This mirrors Roslyn's replacement behavior:
structure is persistent, while trivia is an explicit part of the token boundary.

## Token and formatting ownership

### Lossless token round trips

M1 token streams must represent:

- every code token's stable kind, exact spelling, byte span, line/column span,
  syntax context, and source origin;
- leading and trailing whitespace and comments as first-class trivia segments;
- comments between delimiters, blank lines, comments adjacent to a member, and
  comments at EOF;
- synthetic tokens with generated origin and no fabricated source offset.

`toTokens` on an unchanged source node returns an equivalent stream whose code
and trivia text concatenates to the original node full span. `toTokens` on a
derived node retains the original segments in their original order and inserts
generated segments at the target delimiter boundary. Token spans are contiguous
within each source segment, non-overlapping, and bounded by the owning source.

### Four-space LF formatter

`toSource(normalizeWhitespace = true)` is a token-aware formatter, not an AST
diagnostic dump. It preserves comment text exactly, keeps original token order
and initialization/member order, and changes only whitespace/trivia required to
normalize the output. The canonical policy is:

- indentation depth is four ASCII spaces per declaration/block level;
- every generated or normalized line ending is LF (`\n`), regardless of input
  CRLF or CR;
- comment delimiters and comment bodies are byte-preserved;
- original source token spelling is preserved unless a replacement explicitly
  supplied a new token spelling;
- generated members use canonical spaces around grammar punctuation and receive
  a deterministic trailing newline before the target closing delimiter;
- formatting is idempotent: formatting an already formatted snapshot produces
  identical bytes and does not allocate new node identities.

Original comments remain in the same relative order with respect to original code
tokens. An append is anchored immediately before the existing closing delimiter;
trivia already associated with that delimiter remains before the delimiter, so no
comment text is dropped or silently attached to a different original token. The
exact generated-member/comment placement is covered by golden tests.

`normalizeWhitespace = false` returns the lossless concatenation of the selected
node's token/trivia stream. It is the preferred mode for round-trip assertions.

## Serializer boundary and semantic consumers

The syntax snapshot is an in-memory compiler contract. If a future artifact needs
to persist it, the schema must include a version, source bytes or source digest,
token/trivia records, node table, logical ids, origin records, and hygiene
contexts. Unknown kinds or schema versions fail closed. Native addresses and
borrowed VM strings are never serialized.

The existing `.zrs` writer remains a human-readable diagnostic projection. It must
not be used to implement `toTokens`, cache identity, or reparse. Semantic/compiler
consumers receive the committed AST only after append/replacement validation;
they must not observe a candidate tree during a failed transaction.

## Independent M2 test matrix

Every test below is independent of the macro matcher and expansion scheduler.
The root build owner should register the C tests only after M1 token-stream
fixtures are available.

### Parse and kind coverage

- Parse one fixture for every kind in the inventory, including nested types,
  parameters, expressions, statements, accessors, decorators, generic nodes,
  enum members, and all three union variant payload kinds. Assert exact kind,
  child count, source/full spans, and successful full consumption.
- Parse types covering generic nesting, tuples, function types, ownership and
  reference qualifiers, arrays and array-size constraints, readonly views, and
  malformed/truncated delimiters. Trailing code and recovery diagnostics must
  fail the API rather than return a partial document.
- Parse members for every legal container row. Parse a method, property, field,
  and meta member for struct/class; signatures for interface; an enum member; and
  unit, tuple, and struct union variants. Pass each wrong member kind to every
  container and assert `ZR_SYNTAX_ERROR_WRONG_CONTAINER` or
  `ZR_SYNTAX_ERROR_ILLEGAL_MEMBER`.

### Ownership, immutability, and identity

- Free the original parser state, source buffer, and input token stream after
  parsing; all node names, spans, origins, and `toTokens` results remain valid.
- Repeated child/member access returns the same logical ids. Attempted mutation is
  impossible through the public headers; a compile-only negative fixture cannot
  call `Add`, `Remove`, or write a field.
- Append and replacement leave the old root byte-identical and retain all old
  handles. Unchanged original nodes keep ids and source origins; replaced and
  generated nodes receive new ids and explicit origins.
- Use an allocator-failure harness at every clone, vector growth, token insertion,
  diagnostic, and node-table allocation. Every failure leaves the source snapshot
  unchanged and releases all detached allocations.

### Append, collision, and rollback

- For struct/class, append field, method, property, and `@meta` additions in one
  call; assert originals first, additions after, all additions nested in the
  target collection, and semantic reparsing of the derived source succeeds.
- For interface, append each current signature kind and reject bodies or class
  members. For enum/union, append only current grammar members and reject methods,
  properties, meta functions, and `impl` text.
- Assert duplicate field names, duplicate method signatures, property/accessor
  collisions, duplicate enum names, and invalid union default-variant counts fail
  atomically. A failure in addition N must not publish additions 0 through N-1.
- Change each original method body, field initializer, decorator/meta payload,
  property accessor body, enum value, union payload, and attached comment in a
  candidate replacement. Derive validation must reject each change even when the
  name and signature remain identical, while allowing source-origin ids to be
  remapped by a successful detached clone.
- Attempt to alter an original signature, reorder originals, remove an original,
  or replace an original during derive. Each operation must return its specific
  validation error and preserve the prior snapshot.

### Replacement and provenance

- Replace a nested type, expression, parameter, member, and complete declaration
  with a category-compatible node. Assert persistent roots, unchanged sibling ids,
  carried trivia, and `Replaced` provenance.
- Reject a node from another document lineage, a stale generation handle, an
  incompatible parent slot, unknown kind, or generated node without call-site
  origin.
- Verify explicit attribute replacement is distinct from derive append-only mode,
  and that no replacement path creates an external implementation block.

### Tokens, comments, and formatting

- Round-trip comments before declarations, between modifiers, inside parameter and
  generic lists, before/after braces, between members, trailing at EOF, and in
  `//` and `/* */` forms. Assert exact comment bytes and original token order.
- Feed LF, CRLF, and CR sources. `toTokens` lossless mode preserves source bytes;
  normalized source emits LF only and four-space indentation. Assert formatting
  idempotence and no comment loss.
- Append into empty and non-empty types with comments immediately before the
  closing delimiter. Assert deterministic insertion and preserved comment text.
- Assert token spans are contiguous/non-overlapping, byte offsets map to stable
  line/columns, EOF is represented, and synthetic generated tokens have explicit
  origin rather than a fake source span.

### Stress and integration

- Exercise deep nested generic/type/expression trees, very large member lists,
  repeated append/replace generations, long comments, source larger than initial
  token/vector capacity, and all configured comptime resource limits.
- Reparse every derived `toTokens` stream through the normal parser and compare
  kind/signature/member-order facts. Then feed the committed tree into semantic
  signature and layout consumers to prove M2 does not bypass existing contracts.
- Run focused parser tests under GCC, Clang, and MSVC after the root agent's M1
  gate. Record allocation-failure, sanitizer, and line-ending results in the
  acceptance document.

## Reference evidence

Roslyn supplies the closest immutable syntax precedent. `SyntaxNode.cs` stores a
green immutable node and lazily creates red views; `FullSpan` includes trivia,
`Span` excludes it, and `CloneNodeAsRoot` detaches a persistent root. Its
replacement APIs in `SyntaxNodeExtensions.cs` and `CSharpSyntaxNode.cs` rebuild a
root instead of mutating the old tree. `SyntaxFactory.ParseTypeName`,
`ParseMemberDeclaration`, and related methods enforce category parsing and
consumption. `SyntaxNormalizer.cs` rewrites whitespace through tokens while
retaining comments. ZR deliberately uses LF instead of Roslyn's default CRLF and
keeps the existing ZR AST kinds rather than introducing a second green-tree
grammar.

Rust's proc-macro/token model in `lua/rust` supplies the complementary boundary:
tokens carry spans and hygiene, expansion receives a complete item, and generated
items are validated before publication. CPython's AST validation and JDK
annotation-processing generation rounds provide negative-test precedent for
category validation and detached generation. These references justify the
separation between lossless token ownership, immutable syntax views, and the
semantic compiler pipeline; they do not authorize Rust-only syntax or external
`impl` blocks in ZR.

## M2 acceptance checklist

- [ ] M1 `token_stream.h` names, ownership, spans, trivia, and hygiene semantics
      are accepted by the root agent.
- [ ] `syntax.h` matches that token API without exposing mutable AST pointers.
- [ ] Deep clone/free coverage exists for every current AST union member and
      `SZrType`/parameter child.
- [ ] Parse adapters reject diagnostics, recovery, trailing input, and wrong
      categories transactionally.
- [ ] Stable node ids and source/generated/replaced origins survive persistent
      operations and source/parser-state release.
- [ ] Append validation preserves every original signature and member order and
      rejects illegal enum/union additions and external implementation blocks.
- [ ] Replacement is persistent, category checked, provenance preserving, and
      separate from derive append-only policy.
- [ ] `toTokens` is lossless for original code/comments and derived output is
      reparsable; normalized output is four-space/LF and idempotent.
- [ ] All independent tests above pass under the required toolchains, including
      allocation-failure and rollback coverage.
- [ ] No M2 production commit is made before the root M1 gate and review.
