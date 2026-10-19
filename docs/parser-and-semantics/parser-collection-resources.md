---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/ast.c
  - zr_vm_parser/src/zr_vm_parser/parser_ownership.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_resources.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_template_strings.c
  - zr_vm_parser/src/zr_vm_parser/lexer_cursor.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class_internal.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class_callables.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_union.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_switch_patterns.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_primary_members.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_construction.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_postfix_call.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_switch.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_call_arguments.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property_migration.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property_migration.h
  - tests/parser/test_parser_metadata_budget.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/ast.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_resources.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_template_strings.c
  - zr_vm_parser/src/zr_vm_parser/lexer_cursor.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class_callables.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_union.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_switch_patterns.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_primary_members.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_construction.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_postfix_call.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_call_arguments.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_switch.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property_migration.c
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/parser/test_ast_array_budget.c
  - tests/parser/test_parser_array_budget.c
  - tests/parser/test_parser_collection_budget.c
  - tests/parser/test_parser_metadata_budget.c
  - tests/parser/test_parser_primary_budget.c
  - tests/parser/test_syntax_hygiene.c
doc_type: module-detail
---

# Parser Collection Resources

The token parser charges owned AST storage to the same budget used by the
current macro expansion. An empty collection still owns a vector header and
backing capacity. Its storage cannot escape accounting simply because no
children have been appended yet.

`parser_ast_array_new` and `parser_ast_array_add` obtain the live budget from
the lexer's shared AST owner. The native `NewBudget` and `TryAddBudget` routines
check capacity arithmetic, reserve actual storage before allocating, and commit
the reservation before publishing the new array or element. A failed reservation
or native allocation leaves the existing vector unchanged and sets a fatal
parser error. Vector growth charges the complete new backing allocation while
the previous allocation is still live.

## Collection Ownership

An append transfers a candidate AST to the vector only on success. On failure,
the parser frees the candidate separately and recursively frees the vector's
previously adopted elements. `ZrParser_AstNodeArray_Free` frees pointer storage
only; owning parser failure paths use `free_ast_node_array_with_elements`.

The primary-chain helper has a different ownership boundary:
`append_primary_member` consumes both valid input trees on entry. Its dedicated
`parser_primary_members.c` module releases the base, previously adopted members,
and incoming member when wrapper allocation, initial member storage, or growth
fails. It returns NULL instead of publishing a partial base. Postfix call,
member, index, and switch-pattern callers propagate that result without freeing
the consumed base a second time.

`parser_construction.c` owns construct-target parsing and the `new`, `own`, and
`ref` forms. The node factory adopts target/arguments only on success; callers
deep-free those inputs on failure. Call metadata cleanup is shared with explicit
generic calls through `parser_call_metadata_free`. Speculative switch parsing
may restore its cursor after a syntax mismatch, but a restored fatal quota error
consumes the trial input and terminates the enclosing switch parse.

`parser_switch.c` owns branch assembly. A pending value/block remains local
until its case wrapper is allocated; a wrapper remains local until appended.
Every failed parse or reservation deep-frees pending and previously adopted
branches together. A second default is rejected before replacing the first.

Generic closing-angle consumption uses the same token-source split contract
for text and native input. `SplitCurrentPunct` leaves the first piece current;
`consume_type_closing_angle` advances once after splitting to consume that
piece and leave the second `>` current. This preserves the remaining piece's
span and cached lookahead without rewinding source bytes.

Declaration parsers also free resources which have not yet reached a declaration
AST: the name node, generic declaration, owned types, variadic parameter node,
initializer, and body. Class methods and meta functions live in
`parser_class_callables.c`, where a single failure path frees their signatures,
bodies, and super arguments. The shared class modifier helper remains private
in `parser_class_internal.h`.

| Parser surface | Budgeted collections |
| --- | --- |
| Class and struct | Decorators, inherits, members, parameters, super arguments |
| Interface | Inherits, members, method and meta parameters |
| Property | Accessors and leading decorators |
| Literal | Array elements, object properties, template segments |
| Enum and union | Members, variants, tuple and named payload fields |
| Extern and generator | Declarations, parameters, generator statements |
| Switch move pattern | Tuple bindings and named payload properties |

Successful nodes own their arrays and children through the existing AST
destructor. Parser teardown releases the lexer's lease; retained nodes keep the
shared owner alive independently. Budget usage records work performed and is
not reduced when temporary parser storage is freed.

## Template Interpolation

The template literal parser captures its token's syntax context and origin before
advancing the outer lexer. Both values are passed through segment parsing to the
private embedded-source initializer. The embedded parser shares the outer owner
and live budget, so AST nodes and string materialization consume that budget and
keep the inherited context table alive. Freeing the embedded parser releases its
lease without detaching the outer parser's live budget.

The interpolation source copy is charged before allocation. Failed expression
parsing, a lexical error, or unconsumed trailing tokens releases the partial
expression AST. A failed segment append releases the new segment and all prior
segments; a failed template root allocation also releases the entire segment
array.

## Validation

The initial five collection quota cases were observed failing in the root's
current-checkout MSVC direct replay before this migration: empty arrays,
objects, structs, classes, and interfaces returned ASTs without paying their
remaining collection storage. Class and struct fixtures explicitly include
the already-metered capacity of two in their leading decorator vectors, which
prevents an unrelated AST-shell rejection from satisfying the test.

The native marked-template fixture also observed loss of the interpolation
identifier's context before repair. The current fresh MSVC foundation replay
passes the collection and marked-template contracts. M1 acceptance and the
current GCC/Clang matrix remain open; see the validation ledger for exact runs.

Call-name and marker arrays charge their headers, initial backing storage, and
growth through the parser owner. A failed metadata reservation is fatal and
returns no partially populated metadata to the caller; all adopted argument
ASTs and metadata arrays are released together. Type shells and generic
declaration shells use the same owner allocator, so a quota failure cannot
silently produce a partial type description.

Legacy-property migration collects entries lazily through the declaration's
owner, then charges its handled flags and replacement text during publication.
Handled-flag or replacement reservations fail closed: the adopted legacy ASTs
are still released, no diagnostic fix is published, and the parser records the
shared heap quota as fatal.

`test_parser_metadata_budget.c` contains eight quota cases. Each first
parses its fixture with an unrestricted heap allowance and checks the returned
shape. Native call, type, and generic fixtures decode the entire token stream
before limiting the shared ownership ledger to initialize owner/root capacity.
Cursor replay still materializes token strings and charges that work, so those
quota cases prove whole-path rejection/cleanup rather than identifying every
rejection as metadata storage. Two additional differential cases parse the
same source with and without returned argument names. Their exact heap delta
equals the name-array header plus initial backing storage, and, for a fifth
argument, the complete new backing allocation. Both parses perform the same
token decoding, which removes that cost from the difference.

The root's fresh MSVC replay observed seven failing metadata quota cases before
the storage correction; the generic-shell fixture was already passing. Two
remaining failures exposed native-token decode quota errors being recovered as
syntax errors. The argument-list boundary now checks the shared exceeded
resource before/after expression parsing and before returning its outputs;
fatal failure clears metadata and deep-frees adopted arguments. The subsequent
ten-case metadata replay passes on MSVC, including both exact heap deltas.

Legacy-property publication uses the text parser because migration edits copy
the original spelling. The fixture first verifies one structured migration fix,
then constrains publication after the property AST and temporary entry have
already been adopted. Zero remaining heap targets handled flags; an allowance
of one flag targets the replacement buffer. Both must stop publication with a
fatal heap rejection and release the adopted property tree.

A forwarding allocator observes native array and scratch allocations during
each parse without injecting allocation failure. After result cleanup and
parser teardown, no observed block or ownership root may remain.

`test_parser_primary_budget.c` directly exercises `append_primary_member` with
new, empty-member, and already-populated primary nodes. The fixture first
constructs both input subtrees without a limit, then caps the shared owner heap
at its current usage so wrapper-node, member-array, and member-growth
reservations fail deterministically. It records the forwarding allocator's
live native blocks and GC roots before fallback cleanup; the failure contract
requires a NULL result, fatal resource diagnostic, and cleanup of both adopted
inputs. The target is registered as `macro_parser_primary_budget`. Real parser
cases cover direct/optional calls, construction, generic calls, members, indices,
and switch trials. Unlimited parses check shape and full token consumption;
node quotas include discarded parser probes as well as published nodes.
Switch continuation cases reject default-wrapper allocation, malformed later
case headers/bodies, and duplicate defaults after earlier branches have been
adopted. Their RED replay observed six retained allocations for a rejected
default wrapper and twelve for each malformed continuation. Both input modes
also reproduced nested generic constructor failure and a consumed `>` whose
span incorrectly remained at byte zero. After the parser advances the split
piece, all 30 primary cases pass on fresh MSVC, including plain/generic/nested
constructors, spaced closing angles, three-level nesting, nested calls, and
exact closing-piece spans with repeated lookahead.

The full parser replay then exposed ordinary top-level calls being speculatively
passed to the function-declaration parser after the declaration-shape check had
already rejected them. The missing `fn` diagnostic is fatal, so cursor rollback
correctly preserved it and invalidated otherwise valid import/member chains and
explicit generic calls. That redundant declaration trial is removed; actual
keywordless declaration shapes still reach the existing migration diagnostic.
Source and TokenStream complete-script companions require successful parsing,
full consumption and cleanup. Separate call, function and destructured-import
cases cover their lower support paths. The resulting primary suite passes all
37 cases, and the existing parser suite passes all 74 cases on MSVC.

The observer initializes reusable GC-domain root-table capacity with 64 temporary
handles before tracking parser storage. The longest combined script grows past
the original single-handle warmup; its retained 384-byte domain table is VM
infrastructure, not parser-owned storage. The observer still requires zero
observed parser blocks and the exact active-root baseline after cleanup, and
reports remaining allocation sizes when a check fails.
