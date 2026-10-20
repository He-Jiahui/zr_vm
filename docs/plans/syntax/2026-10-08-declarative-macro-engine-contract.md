---
status: proposed
doc_type: milestone-detail
milestone: M3
depends_on:
  - M1 token/input/hygiene/budget/control flow
  - M2 structured syntax and member merging
related_code:
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
reference_sources:
  - lua/rust/compiler/rustc_expand/src/mbe
  - lua/rust/compiler/rustc_resolve/src/build_reduced_graph.rs
  - lua/rust/tests/ui/macros
implementation_files: []
tests: []
owner: root
scope: declarative-macro-m3-contract-only
---

# ZR Declarative Macro Engine Contract (M3)

This document is the implementation and test contract for ZR `macro_rules!`.
It is intentionally limited to the M3 declaration-macro engine. It does not
implement production code, change the lexer token IDs, or extend enum/union
grammar. The parser will reserve append-only AST IDs for macro definition and
invocation placeholders; those IDs are temporary syntax carriers, not new
language declarations. M1 and M2 own the generic token input, immutable syntax
views, and hygiene/resource primitives on which this contract depends. M3 is
accepted only when every normative rule and the negative matrix below has
executable coverage.

## 1. Accepted surface

The source form is:

```zr
macro_rules! name {
    (matcher) => {transcriber};
    [matcher] => [transcriber];
}

name!(arguments);
name![arguments];
name!{arguments}
```

The definition body is a delimited token tree containing one or more ordered
arms. Each arm has a delimited matcher, `ZR_TK_FAT_ARROW`, a delimited
transcriber, and an optional semicolon. Parenthesis, bracket, and brace call
delimiters are all valid and are significant when they occur in a matcher. The
macro name may be a ZR module-qualified macro path once the macro namespace
resolver is available.

The only accepted fragment specifiers are `tt`, `ident`, `literal`, `expr`,
`stmt`, `item`, `ty`, `pat`, `path`, `block`, `meta`, and `vis`. `lifetime`,
edition aliases, metavariable expressions, token concatenation, and Rust-only
special variables are rejected as unsupported M3 syntax. M4 procedural roles,
attributes, derives, quote, and emit are separate milestones.

Repetition uses `*`, `+`, and `?`. A separator is one token tree immediately
before `*` or `+`; `?` never accepts a separator. Repetitions can nest and can
be adjacent when their FIRST sets are disjoint. Definitions and invocations
are structured data; display text is never re-lexed or used as an expansion
transport.

Initial parsing produces a macro-definition placeholder and a context-specific
macro-invocation placeholder wherever a call is legal. BuildFacts sees these
placeholders while it selects active/inactive comptime branches; SyntaxExpansion
then replaces placeholders only in the active tree. Expansion outputs are parsed
using the existing declaration/member entrypoints. In particular, an `item`
transcriber can produce only declarations already accepted by ZR, and no
external `impl` form is introduced. The placeholders do not add enum/union
grammar or a new semantic declaration kind.

## 2. Evidence ledger

The following Rust sources are normative evidence for the split of concerns;
they are not a requirement to copy Rust edition or hygiene behavior wholesale.

| Contract concern | Rust evidence | Required ZR consequence |
| --- | --- | --- |
| Token-tree and repetition model | `lua/rust/compiler/rustc_expand/src/mbe.rs:21-90` (`Delimited`, `SequenceRepetition`, `KleeneOp`, first-class metavariables) | Keep recursive groups and repetition nodes in an owned MBE tree. Delimiters are structural, not reconstructed from text. |
| Quoting a matcher/body | `lua/rust/compiler/rustc_expand/src/mbe/quoted.rs:15-17,40-151,200-307` | Parse `$name`, `$name:kind`, and `$(...)sep*+?` from token trees. Keep LHS declarations distinct from RHS references. |
| Separator and `?` validation | `quoted.rs:392-410` | Reject a separator before `?` at definition time, with the separator and operator spans. |
| Flattened NFA locations | `lua/rust/compiler/rustc_expand/src/mbe/macro_parser.rs:90-130,167-240` | Compile recursive matchers to non-recursive locations with explicit sequence start/end, separator, metavariable index, and EOF. |
| Nested capture shape | `macro_parser.rs:243-389` | Capture each binder as `MatchedSingle` or a nested `MatchedSeq` forest whose depth and lengths mirror matcher repetitions. |
| Literal token equality | `macro_parser.rs:391-420` | Compare token kind and identifier/literal semantic value; never compare source text or hygiene context. |
| NFA transitions and black-box parse | `macro_parser.rs:456-590,615-709` | Maintain current/next/black-box position sets; epsilon-enter/skip/repeat transitions; parse a nonterminal with a cloned cursor and commit a whole fragment. |
| Ambiguity diagnostics | `macro_parser.rs:729-760` and `diagnostics.rs:80-170` | Multiple successful end positions in an arm are local ambiguity, not an arbitrary first parse. Track furthest failure for useful diagnostics. |
| Ordered arms | `lua/rust/compiler/rustc_expand/src/mbe/macro_rules.rs:509-571` | Try arms in source order and use the first arm with one complete match. Do not rank arms by specificity. |
| Definition checks | `macro_rules.rs:679-800,850-1020` and `macro_check.rs:200-350` | Validate duplicate/free variables, empty matcher repetitions, RHS repetition depth, and malformed quoted trees before registration. |
| FIRST/follow sets | `macro_rules.rs:1037-1180,1333-1574,1555-1565` | Compute FIRST and possibly-empty suffixes recursively; reject fragment followers outside the ZR follow table. |
| RHS repetition lockstep | `lua/rust/compiler/rustc_expand/src/mbe/transcribe.rs:143-210,291-384,776-814` | Use a repeat-frame stack; all metavariables under one RHS repetition must have one equal iteration length. |
| RHS lookup and opacity | `transcribe.rs:344-390,699-735,386-590` | Descend capture forests by repeat indices; report still-repeating/missing variables. Non-`tt` captures remain opaque when forwarded. |
| Macro definition scope | `lua/rust/compiler/rustc_resolve/src/build_reduced_graph.rs:1226-1239,1271-1341` | Keep a dedicated macro namespace, parent macro scope, definition module, visibility/export flag, and import binding. |
| Scope traversal and generated definitions | `build_reduced_graph.rs:1374-1442` | Register a definition while walking its lexical scope, create an invocation scope, and visit generated items in their parent module. |

Representative tests in `lua/rust/tests/ui/macros` provide the acceptance
shape: `macro-nt-list.rs` (delimiter significance),
`macro-seq-followed-by-seq.rs` (adjacent disjoint repetitions),
`local-ambiguity-multiple-parsing-options.rs` (NFA ambiguity),
`macro-followed-by-seq-bad.rs` and `macro-first-set.rs` (follow sets),
`macro-interpolation.rs` and `macro-match-nonterminal.rs` (fragment opacity),
`macro-stmt.rs`/`macro-tt-matchers.rs` (statement and tt boundaries),
`macro-of-higher-order.rs`/`macro-nested_expr.rs` (generated definitions),
`macro-use-scope.rs`, `defined-later-issue-121061.rs`, and
`macro-shadowing.rs` (source order, imports, and shadowing),
`macro-export-inner-module.rs`, `use-macro-self.rs`, and `macro-crate-use.rs`
(export/import), and `issue-84632-eager-expansion-recursion-limit.rs` plus
`macro-deep_expansion.rs` (fixed point and recursion limits).

## 3. Placeholder AST and phase boundary

The first parse must not expand or resolve a macro. It parses the complete
source, including calls in branches that may later be inactive, into the
existing AST plus append-only placeholder IDs. The following IDs are appended
after the current `EZrAstNodeType` tail; their numeric values must never be
inserted between existing IDs:

```text
ZR_AST_MACRO_DEFINITION
ZR_AST_MACRO_INVOCATION_EXPRESSION
ZR_AST_MACRO_INVOCATION_STATEMENT
ZR_AST_MACRO_INVOCATION_ITEM
ZR_AST_MACRO_INVOCATION_MEMBER
ZR_AST_MACRO_INVOCATION_TYPE
ZR_AST_MACRO_INVOCATION_PATTERN
ZR_AST_MACRO_INVOCATION_PATH
ZR_AST_MACRO_INVOCATION_BLOCK
ZR_AST_MACRO_INVOCATION_META
```

The implementation may use one shared union payload with a context enum, but
the node kind remains explicit so existing AST walkers can reject or preserve a
placeholder without guessing its grammar position. The shared payload is:

```text
MacroDefinition {
    name token, body TokenTree, visibility, source range,
    lexical order, module/scope placeholder, definition origin
}

MacroInvocation {
    macro path TokenTree, argument group TokenTree, delimiter,
    expected fragment/context, source range, lexical order,
    parent scope placeholder, call origin
}
```

`macro_rules!` is recognized by the top-level/statement/member parser before
ordinary identifier dispatch and stored as `MacroDefinition`. A `name!group`
call stores its path and argument group without flattening the group or
constructing an AST from speculative text. A macro invocation in a type or
pattern position is represented by the corresponding type/pattern ID; path,
block, and meta positions use their dedicated IDs. Member placeholders are
spliced into struct/class/interface member arrays by SyntaxExpansion. Item and
statement replacements may return multiple nodes; expression/type/pattern/path/
block/meta replacements return one node or typed fragment as required by the
parent.

BuildFacts is the first consumer after parsing. Its active-tree contract is:

1. `ZrParser_CompileTime_PrepareBuildFactsInCompilerState` (the existing hook at
   `compiler/compile_time_executor.c:856-864`) evaluates build facts and marks
   the active branch/subtree. It must visit a macro placeholder as inert syntax,
   never resolve or expand it, and must be able to leave a placeholder inside an
   inactive branch without a `macro_not_found` error.
2. SyntaxExpansion receives only the active-tree view plus source-order scope
   metadata. It resolves and replaces placeholders through the direct token
   source contract in Section 4.2, then publishes a detached macro-free tree.
3. Signature, semantic Expansion, Layout, and LateCheck consume the published
   tree. Existing semantic `EXPANSION` (currently entered at
   `compiler.c:776`) must not be used for MBE matching.

`ZrParser_Ast_Free`, syntax writers, and any AST walker must preserve/free the
owned TokenTree in a placeholder even when BuildFacts drops that branch. A
placeholder has no value/type semantics until SyntaxExpansion replaces it.

## 4. Existing ZR entrypoints and required parser hooks

### 4.1 Current constraints

`zr_vm_parser/include/zr_vm_parser/lexer.h:14-130` assigns stable IDs to
single-character and reserved tokens. `SZrToken` at `lexer.h:134-150` contains
only an `EZrToken`, semantic payload, and lex-error fields. `SZrLexState` at
`lexer.h:152-188` borrows source text, owns a temporary scan buffer, and has one
lookahead token. The lexer already combines `->`, `=>`, `>>`, and assignment
operators into distinct token IDs (`lexer.c:90-120,778-851`). Macro token trees
must preserve those IDs plus an owned spelling/semantic payload and a source
range; calling `TokenToString` is insufficient because its single-character
buffer is transient.

`zr_vm_parser/include/zr_vm_parser/parser.h:26-75` exposes whole-script,
top-level-statement, and expression parsing, but no token-stream input. The
internal declarations in
`zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h:16-59,256-440` expose
cursor save/restore and these reusable entrypoints:

| Fragment | Existing entrypoint/adapter | Boundary contract |
| --- | --- | --- |
| `tt` | token-tree cursor, no AST parse | Consume exactly one token or one complete group. |
| `ident` | `parse_identifier` (`parser_internal.h:256-263`, implementation `parser_literals.c:515`) | Consume one identifier accepted by ZR (including existing contextual `test` behavior only where parser accepts it). |
| `literal` | `parse_literal` (`parser_internal.h:256`, `parser_literals.c:381`) | Consume one boolean/integer/float/string/template/char/null/infinity/NaN literal with its semantic value. |
| `expr` | `parse_expression` (`parser_internal.h:330-356`) | Consume one complete expression; leave the caller's delimiter/follow token. |
| `stmt` | `parse_statement` (`parser_internal.h:432-438`) | Consume one existing statement. A semicolon is part of the statement only where the current statement parser consumes it. |
| `item` | `parse_top_level_statement` (`parser_internal.h:436-440`, `parser_statements.c:1832`) | Consume one existing top-level declaration/module/extern item; reject expression-only results. |
| `ty` | `parse_type`/`parse_type_no_generic` (`parser_internal.h:358-366`, `parser_types.c:688+`) | Consume one type, including current ownership/generic/tuple rules; preserve `>`/`>>` splitting behavior. |
| `pat` | Existing identifier/destructuring/switch helpers: `parse_identifier`, `parse_destructuring_object`, `parse_destructuring_array` (`parser_types.c:1271-1467`) and switch pattern probes (`parser_switch_patterns.c`) | Add a category adapter over patterns already accepted by ZR. Do not invent a new pattern grammar. |
| `path` | Shared path adapter built from identifier/member/generic parsing (`parse_identifier`, `parse_member_access`, `parse_generic_type`) | Consume a qualified path only; do not silently accept a general expression. |
| `block` | `parse_block` (`parser_internal.h:402-404`, `parser_statements.c:250-304`) | Require one `{...}` block and leave the following token. |
| `meta` | Existing `parse_decorator_expression` (`parser_internal.h:374-377`, `parser_types.c:1299-1325`) and `parse_meta_identifier` (`parser_types.c:1271-1297`) | Consume only the current decorator/meta forms. Its currently unused destructor path must be repaired in M2 before M3 uses it. |
| `vis` | `parse_access_modifier` (`parser_internal.h:382`, `parser_types.c:1469-1484`) | Parse `pub`, `pri`, or `pro`; allow an empty capture where the matcher permits visibility. |

The expression primary dispatcher in `parser_expression_primary.c:1100-1155`
does not recognize `name!group`; member/call chaining starts at
`parser_expression_primary.c:828-1098`. Statement and top-level dispatch are
at `parser_statements.c:1511-1740` and `1832+`, and expression statements
require a semicolon at `parser_statements.c:312-335`. These are the expression,
statement, and item placeholder insertion points. A call is recognized before
ordinary identifier/member parsing and produces the context-specific node from
Section 3, rather than expanding in place.

Member lists must preserve a placeholder slot in the existing struct/class/
interface member arrays (`parser_struct.c`, `parser_class.c`, and
`parser_interface.c` member dispatchers). The type parser (`parse_type`),
destructuring/using/switch pattern probes, member-name/path probes, and
decorator/meta parser receive the same early recognition hook. BuildFacts walks
all of these nodes as inert syntax; the active-tree scheduler later invokes the
matching fragment adapter and performs a checked node/array replacement.

The member ownership map is already explicit in
`parser_internal.h:462-517`: `parse_struct_field`/`parse_struct_method`,
`parse_class_field`/`parse_class_method`/`parse_class_property`/property
accessors/meta functions, `parse_interface_field_declaration`/
`parse_interface_method_signature`/`parse_interface_property_signature`/
`parse_interface_meta_signature`, and `parse_property_declaration`. Their
implementations live in `parser_struct.c`, `parser_class.c`,
`parser_interface.c`, and `parser_property.c`. A member-context macro output is
validated by the same owner-specific entrypoint; it is not routed through a
top-level declaration parser. `parse_enum_member` and `parse_union_variant`
remain existing grammar consumers only, and no M3 member macro may add a new
enum/union variant shape.

### 4.2 Token-source bridge

M1 must provide one parser token-source interface with two implementations:

```c
typedef struct SZrParserTokenCursor SZrParserTokenCursor;
typedef struct SZrParserTokenSnapshot SZrParserTokenSnapshot;
typedef TZrBool (*TZrParserTokenNext)(SZrParserTokenCursor *cursor);
typedef TZrBool (*TZrParserTokenLookahead)(const SZrParserTokenCursor *cursor,
                                           TZrSize distance,
                                           SZrMacroToken *out);
typedef TZrBool (*TZrParserTokenSave)(const SZrParserTokenCursor *cursor,
                                      SZrParserTokenSnapshot *out);
typedef void (*TZrParserTokenRestore)(SZrParserTokenCursor *cursor,
                                      const SZrParserTokenSnapshot *snapshot);
```

The existing lexer becomes an adapter; an owned macro `TokenStream` becomes the
second adapter. Parser code must call cursor operations rather than directly
assuming `ps->lexer`, so a black-box fragment parse can clone/restore a cursor.
The snapshot includes group depth, child index, current/lookahead token, source
range, diagnostic state, and the current fragment boundary. The existing
`SZrParserCursor` (`parser_internal.h:18-40`) is lexer-specific and cannot be
used for a macro tree without this extension.

The macro token stream owns every token payload (including identifier spelling,
literal spelling/semantic value, source range, origin, and spacing) and every
group. It may reuse the stable `EZrToken` IDs but must never borrow the lexer's
temporary buffer or VM strings without an explicit retained ownership handle.
Lexing creates groups by balanced delimiter tokens; malformed delimiter input
is diagnosed before MBE matching. `ZR_TK_EOS` is a cursor sentinel and is not
stored as a user token.

No macro operation converts a token tree to source text. Parsing a fragment
consumes the macro cursor directly and returns a structured result:

```c
typedef struct SZrMacroFragment {
    EZrMacroFragmentKind kind;
    SZrMacroTokenTree *raw;       /* immutable source tree for forwarding */
    SZrAstNode *node;             /* owned AST when this kind has a parser */
    SZrType *type;                /* owned type for ty, otherwise null */
    TZrBool opaque;               /* true for parsed non-tt forwarding */
    SZrFileRange location;
} SZrMacroFragment;
```

`raw` and `node` have one owner in the capture arena. A failed NFA branch
discards its arena checkpoint; a successful arm transfers its capture arena to
the transcriber. The parser adapter must prove progress and complete one
fragment, then leave a legal follow token or the enclosing group close.

Placeholder replacement uses the same interface in the opposite direction:
`ZrParser_Macro_ParsePlaceholder(parent, context, output_cursor)` consumes the
transcribed TokenStream directly and returns an owned AST node, typed fragment,
or ordered node array. The scheduler replaces the placeholder slot only after
this parse succeeds. It never serializes the output node, prints it, or creates
a new lexer from display text. Member/item/statement replacement is a checked
array splice; scalar replacement transfers one AST/type owner. The old node is
retained until the new owner is validated so a failed expansion can roll back.

### 4.3 Fragment ambiguity and opacity

`tt` captures one token tree and remains inspectable by literal matchers. All
parsed non-`tt` fragments (`expr`, `stmt`, `item`, `ty`, `pat`, `path`, `block`,
`meta`, `vis`, and `literal` when represented by its parsed node) are opaque
when forwarded to another macro. A downstream matcher may bind them again to
the same fragment kind, but may not inspect their internal literal tokens. The
raw tree is retained for transcribing and diagnostics, not for violating
opacity.

Transcription wraps an opaque capture in an internal `OpaqueFragment` token-tree
node carrying its AST/type and original group boundary. The parser treats this
node as one already-parsed nonterminal, preserving precedence (for example an
`expr` capture inserted into `($e)` cannot leak its binary operators into the
outer expression). `tt`, `ident`, and literal token captures are emitted as
their raw trees. This is the structured equivalent of the behavior tested by
`macro-interpolation.rs` and `macro-match-nonterminal.rs`.

## 4. Matcher contract

### 4.1 Quoted definition tree

`macro_definition` parses the definition body into:

```text
MacroDefinition
  name, definition_scope, module, visibility, export, origin
  rules[] (source order)
    lhs: MacroTree (matcher)
    rhs: MacroTree (transcriber)

MacroTree = Token | Group(delim, children)
          | MetaVarDecl(name, fragment_kind)
          | MetaVarRef(name)
          | Repeat(children, separator?, op)
```

The parser recognizes `$` only in matcher/transcriber token trees. A matcher
`$name` must be followed by `:kind`; an RHS `$name` is a reference. A sequence
must have non-empty body and exactly one trailing `*`, `+`, or `?`. A separator
is a single token tree and is not accepted with `?`.

Definition checks are performed before registry insertion:

* fragment kind is in the M3 list;
* each binder name is unique in the arm after normalized identifier comparison;
* every RHS reference is declared by the same arm;
* each RHS reference appears at a repetition depth that is a prefix of its
  matcher binder depth;
* no LHS repetition can match empty input (except an explicitly supported
  nullable `vis` capture, which still must make progress when enclosed by a
  repetition);
* no adjacent repetition has an indistinguishable FIRST choice;
* every binder has a legal follow set (Section 5).

### 4.2 Flattened NFA

Compile each LHS `MacroTree` to an immutable flat vector equivalent to Rust's
`MatcherLoc`:

```text
Token(kind, semantic)
GroupOpen(delim) / GroupClose(delim)
SequenceStart(op, first_after, first_child, binder_count, seq_depth)
SequenceSeparator(token)
SequenceEnd(op, first_child)
MetaVarDecl(name_id, kind, binder_id, seq_depth)
Eof
```

The input cursor is a tree cursor. `GroupOpen` requires the exact delimiter and
recursively matches children; a group's closing boundary is not an ordinary
token that a fragment may consume. Token equality compares token ID and
semantic identifier/literal value, preserving distinct compound operators and
never comparing origin context.

An NFA position is `(matcher_index, input_cursor_snapshot, capture_root)`. The
capture root is a persistent/arena-backed forest. Epsilon closure performs:

1. sequence start: install empty `MatchedSeq` slots, skip for `*`/`?`, and enter
   the body;
2. sequence end: finish the current iteration, optionally consume one
   separator, and either repeat or leave the sequence;
3. group open/close and ordinary matcher-location advances.

For a `MetaVarDecl`, clone the input cursor and ask the fragment adapter to
parse exactly one candidate. A successful candidate appends
`MatchedSingle(fragment)` at the current repetition depth and creates a
black-box successor. A failed candidate kills only that NFA branch. The adapter
must return all grammar-complete candidates if ZR parsing itself is ambiguous;
the NFA reports local ambiguity if more than one candidate reaches the same
arm EOF, instead of relying on parser recovery order.

Use per-arm fresh cursors and capture arenas. At invocation end, accept only one
successful `Eof` position. Zero successes produce a furthest-position failure;
more than one is `macro_local_ambiguity` with the competing fragment/binder
descriptions. Arm selection is first successful arm in source order, even if a
later arm appears more specific.

The matcher must enforce a progress guard for every repeated body. A body that
returns the same input cursor after a successful iteration is a definition
error, not a loop. Deduplicate epsilon positions by matcher index, input
cursor identity, and capture shape where possible; a budget is still required
for pathological nested repetitions.

### 4.3 Capture forest

The capture shape is:

```text
Capture = Single(SZrMacroFragment)
        | Sequence(vector<Capture>)
```

Each `Sequence` corresponds to one source repetition nesting level. A binder
inside two repetitions therefore has `Sequence[Sequence[Single(...)] ]` even
when an iteration is empty. The capture arena is immutable after arm success;
the transcriber only reads it. This shape is required for nested forms such as
`$($($x:ident),+);+` and for deterministic lockstep diagnostics.

## 5. FIRST, follow sets, and static ambiguity

The definition checker computes for every matcher suffix:

```text
First = set of literal token signatures and fragment start classes
MayEmpty = whether the suffix can consume no token tree
```

For a sequence, include the separator and the suffix FIRST when the body may be
empty. For adjacent repetitions, reject only overlapping choices; permit
disjoint choices such as `$(banana $a:ident)* $(orange $b:tt)*`, as covered by
`macro-seq-followed-by-seq.rs`.

The initial ZR follow table is deliberately conservative and must be encoded
as data, not scattered conditionals:

| Fragment | Allowed next token classes |
| --- | --- |
| `ident`, `literal`, `meta`, `tt`, `item`, `block` | Any token tree or enclosing group close. |
| `vis` | Any token that can start an existing ZR declaration after an empty or explicit access modifier; comma, semicolon, and group close. |
| `expr`, `stmt` | Comma, semicolon, group close, and `ZR_TK_FAT_ARROW` where the token is the macro arm boundary. |
| `pat` | Comma, semicolon, equals, `in`, `if`, group close, and `ZR_TK_FAT_ARROW` where a pattern arm uses it. The checker must be tightened to the actual ZR pattern parser as new pattern forms are accepted. |
| `ty`, `path` | Comma, semicolon, colon, equals, pipe, `ZR_TK_GREATER_THAN`, `ZR_TK_RIGHT_SHIFT`, group close, and declaration/body openers accepted by the current type parser. |

The checker walks every binder and every possible suffix, including suffixes
inside and after repetitions. A fragment that can be followed by an illegal
token is a definition error even when current examples happen to parse. The
diagnostic identifies the binder, fragment kind, follower, and a suggested
separator. The table is a ZR contract rather than an unconditional Rust copy;
the ZR parser's existing `>`/`>>` handling (`parser_types.c:489-513,
597-613`) is specifically included.

Runtime local ambiguity remains required even after follow checks: a parser can
produce two legal fragment lengths in a caller context. The diagnostic must
show the invocation range, arm range, input token index, and competing
continuations.

## 6. Transcriber contract

Transcription traverses the RHS iteratively with frames:

```text
Frame::Group(children, index, delimiter)
Frame::Repeat(children, index, separator, op, next_iteration)
RepeatContext { iteration, length }
```

For `$name`, descend the capture forest using the active repeat indices. If the
result is still a `Sequence`, report `macro_variable_still_repeating`; if the
name is absent, report `macro_unknown_variable` (or preserve a marked `$name`
only for the explicit nested-definition feature described below). For a
repeat, compute `lockstep_iter_size` across all references below that repeat:

* no references is `macro_repeat_without_capture`;
* one length is the iteration count;
* differing lengths are `macro_repeat_length_mismatch`;
* zero length is valid for `*`/`?`, invalid for `+`;
* separators occur only between emitted iterations.

The output is another owned `MacroTokenTree`, not text. Definition-origin
tokens receive a new expansion origin; caller-origin raw captures retain their
origin. Group delimiter kind and source range are retained for diagnostics.
Opaque fragments are emitted as described in Section 4.3. No concatenation,
stringification, or `$crate` behavior is implied by M3.

Nested generated definitions are supported by keeping unresolved `$` references
inside the generated macro definition's quoted tree until that definition is
registered. This is needed for the higher-order patterns in
`macro-of-higher-order.rs`; it is not a license to re-lex a printed expansion.

## 7. Macro namespace, scopes, export, and import

Macros have a namespace separate from values, types, members, and compile-time
callables. A `MacroBinding` contains:

```text
name, normalized_name, definition_id, arm set
definition_scope_id, parent_macro_scope_id, module_id
visibility (private/public), exported, origin
```

The M3 default is Rust-like textual scope:

* a local definition enters its current lexical macro scope at its declaration
  point and is visible to following source in that scope;
* a call before a local definition is `macro_not_found`, as in
  `defined-later-issue-121061.rs`;
* a nested definition shadows only in its nested scope; same-scope duplicate
  definitions and imported/local collisions are diagnostics, not a silent
  overwrite;
* `pub macro_rules!` (or the chosen existing ZR public modifier immediately
  before it) marks the binding exportable from its module;
* an explicit macro import resolves only the macro namespace and records the
  imported module path, source range, and visibility. Importing a value/type
  does not import a macro.

For source modules, export tables are built before expanding a dependent
module, and import cycles are diagnosed as `macro_import_cycle`. For artifacts,
the exported macro metadata must contain the immutable definition token tree,
fragment kinds, module identity, visibility, contract hash, and source mapping;
native pointers and AST addresses are forbidden. M3 need not implement binary
artifact serialization, but its registry API must leave this boundary explicit.

ZR does not yet have Rust `SyntaxContext` plumbing. M1/M2 must nevertheless
attach an `origin_id` and `scope_id` to each token. Until full hygiene is
accepted, the resolver policy is explicit mixed-site behavior: definition
tokens resolve in the macro definition scope, substituted caller tokens resolve
in the call scope, and generated identifiers do not silently capture a caller
binding merely because their spelling matches. The policy and origin travel
through AST identifiers, cloning, diagnostics, and cache keys. M3 must not hide
this gap by comparing or rewriting source strings.

## 8. Fixed-point expansion scheduler

The scheduler runs after BuildFacts and before existing compiler signature
predeclaration. The compiler entry currently prepares BuildFacts at
`compiler.c:1064` and `compile_time_executor.c:856-864`, then
`compile_script` enters signature collection before its semantic expansion pass
at `compiler.c:750-776`. M3 inserts `SyntaxExpansion` between those operations.
The initial AST still contains placeholders (Section 3), so inactive branches
are never looked up or expanded. The existing semantic/compiler expansion phase
remains unchanged and consumes a macro-free AST.

The scheduler owns a deterministic worklist:

```text
MacroExpansionRequest {
    invocation_id, macro_path, call_scope_id, module_id,
    delimiter, input_token_tree, expected_fragment_context,
    source_range, parent_expansion_id, depth, round
}

MacroExpansionResult {
    output_token_tree,
    generated_definitions,
    generated_invocations,
    parsed_nodes,
    origin_map,
    diagnostics
}
```

The required algorithm is:

1. Parse source into an AST containing macro-definition/invocation placeholders
   and retain each placeholder's direct TokenTree/source cursor. No request is
   resolved in this pass.
2. Run BuildFacts over the AST. Inactive subtrees are marked/dropped while
   retaining ownership cleanup for their placeholders. Active placeholders are
   the only scheduler inputs.
3. Walk active placeholders in source order, registering active definitions and
   enqueueing active calls with their expected context.
4. Resolve each request in its call scope's macro namespace. Match arms and
   transcribe an owned tree. A failed request publishes no partial output.
5. Parse the output directly with the expected fragment adapter. Item/statement
   output is inserted into the detached parent syntax collection; expression,
   type, pattern, path, block, and meta output replaces the invocation node.
6. Walk each output tree for macro definitions and calls. Generated definitions
   enter the lexical scope represented by the expansion site; generated calls
   are appended to the deterministic worklist after their parent request.
7. Continue until the queue is empty. Only then publish the macro-free AST to
   the ordinary signature/compiler pipeline.

The scheduler must enforce `max_expansion_rounds`, `max_expansion_depth`,
`max_invocations`, `max_generated_tokens`, and `max_generated_definitions`
using the M1 resource ledger. It detects an active recursion key
`(definition_id, call_scope_id, input_digest, expected_context)` and reports a
located `macro_expansion_cycle`; a finite expansion that exceeds a budget gets
`macro_expansion_limit` with the active chain. Queue order, arm order, and
module dependency order are part of reproducibility.

The scheduler is not the existing compile-time declaration transform engine.
It may reuse its round/budget diagnostic vocabulary, but matching,
transcription, and token ownership remain in the M3 modules. A generated item
must be parsed and validated before it is published; a later failure rolls back
that detached expansion and preserves the prior valid tree.

## 9. Ownership split and dependencies

The implementation should be split so no one file becomes the macro compiler:

| Owner | Responsibility | Dependencies |
| --- | --- | --- |
| `macro_token_stream.[ch]` | Owned token/group/span/origin, cursor, clone/free, semantic equality, digest | M1 token source, VM memory/string handles, `location.h` |
| `macro_definition.[ch]` | `macro_rules!` recognition, quoted tree, arm parsing, static checks entry | token stream, diagnostics |
| `macro_matcher.[ch]` | flat locations, FIRST/follow checker, NFA, capture arena, local ambiguity | fragment parser, token cursor, resource ledger |
| `macro_transcriber.[ch]` | RHS frames, repetition lockstep, opaque capture emission | capture arena, token stream, origin/hygiene |
| `macro_fragment_parser.[ch]` | token cursor adapter and all 12 fragment entrypoints | parser cursor bridge, existing recursive descent, AST/type ownership |
| `macro_scope.[ch]` | macro namespace, lexical/module scopes, import/export metadata, origin policy | module identity/import APIs, hygiene IDs |
| `macro_scheduler.[ch]` | post-BuildFacts/pre-signature worklist, fixed point, budgets, rollback, placeholder replacement | all above, parser/compiler boundary |
| parser integration | recognize definitions/calls before ordinary primary/member/call and create context-specific placeholders | `parser_expression_primary.c`, `parser_statements.c`, declaration/member/type/pattern parsers |
| compiler integration | preserve placeholders through BuildFacts, invoke SyntaxExpansion on the active tree before signature, consume macro-free AST | `compile_time_executor.c`, `compiler.c`, compile phases |

The matcher and transcriber must not call the compile-time evaluator or inspect
types/layout. Fragment parsers may build syntax ASTs, but semantic name/type
resolution waits for the normal compiler. Enum/union parser files are consumers
only; no grammar ownership moves to M3.

## 10. Diagnostics and test matrix

Each case must assert diagnostic code, primary range, and no partial AST
publication. The exact text may be localized, but the code and related spans
are stable.

### Definition diagnostics

* malformed `macro_rules!` name, missing body delimiter, arm arrow, RHS, or
  semicolon; empty arm set;
* unknown/missing fragment specifier, explicitly rejected `lifetime` or
  metavariable expression;
* empty repetition, missing Kleene operator, multiple operators, separator
  before `?`, and a nullable repeated body;
* duplicate binder, free RHS variable, binder-depth mismatch, RHS repetition
  with no capture, and adjacent overlapping FIRST sets;
* illegal follower for every restricted fragment (`expr`, `stmt`, `pat`, `ty`,
  `path`, `vis`) including nested and suffix-after-repetition cases.

### Invocation and matching diagnostics

* unknown/private/not-imported macro in an active placeholder, wrong namespace,
  source call before definition, duplicate or ambiguous local/imported name;
  an identical call in an inactive BuildFacts branch must not diagnose lookup;
* all arms fail (unexpected EOF, unexpected token, delimiter mismatch) with
  furthest matcher position and arm notes;
* local ambiguity from multiple NFA end positions and parser fragment-length
  ambiguity;
* fragment parse failure, incomplete consumption, statement-semicolon mismatch,
  item in expression context, invalid visibility/path/type/pattern boundary;
* forwarded opaque fragment used as a literal matcher, plus precedence tests
  showing opaque grouping is retained.

### Transcription and scheduler diagnostics

* unknown/still-repeating variable, wrong repetition depth, no-capture repeat,
  lockstep length mismatch, `+` with zero captures, and separator placement;
* generated malformed delimiter tree, generated output that is not a complete
  requested fragment, duplicate generated declaration;
* generated definition shadowing/import collision, macro import cycle, recursive
  expansion cycle, depth/round/token/invocation budget exhaustion;
* failure rollback, deterministic rerun, and cancellation/OOM cleanup.

### Positive and stress coverage

The executable matrix must include:

* all three invocation delimiters and delimiter-significant arms;
* every fragment kind, one arm per kind and mixed interpolation in an item;
* empty/one/many `*`, `+`, and `?` repetitions, separators, adjacent disjoint
  repetitions, and at least three nested levels with uneven lengths;
* arm ordering, shadowing, textual source order, public export/import, nested
  generated definitions, and fixed-point expansion;
* identifiers, contextual keywords, compound operators, `>>` type boundaries,
  strings/chars/templates, Unicode spelling, comments, and source ranges;
* 1,000-arm FIRST dispatch, deeply nested token groups, 10,000 generated calls,
  recursive cycles, resource-limit boundaries, and deterministic output hash.

Tests should be added as focused token/matcher/transcriber unit suites, parser
fragment suites, scheduler/scope suites, and compiler integration fixtures.
Rust UI examples are evidence and should be translated to ZR syntax and
diagnostic codes rather than copied as text.

## 11. Acceptance gates

M3 is not accepted until:

1. M1 and M2 acceptance records show owned token cursors, syntax nodes, origin
   IDs, budgets, and cleanup on failed branches.
2. The matcher/transcriber unit suite proves arm ordering, NFA ambiguity,
   repetitions, follow sets, capture shape, opacity, and lockstep output
   without invoking the compiler.
3. Parser integration proves every listed ZR fragment adapter consumes exactly
   one structured fragment and preserves source ranges; no text round-trip is
   present in the call path.
4. Placeholder/BuildFacts fixtures prove inactive calls remain inert while active
   calls expand; scheduler fixtures then prove pre-signature generated
   declarations, source-order visibility, export/import, generated definitions,
   fixed-point termination, rollback, and limits.
5. Existing parser/compiler tests remain green on GCC/Clang and MSVC. The
   acceptance note records commands, counts, diagnostics, and any baseline
   difference. No enum/union grammar or external-impl support was added.
