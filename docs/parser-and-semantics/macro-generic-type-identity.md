---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lookup.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lookup.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_nominal_prototype.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_prototype.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_prototype.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_substitution.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_substitution.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_adapter.c
  - tests/parser/test_syntax_hygiene_generic_prototypes.c
  - tests/parser/test_syntax_hygiene_generic_definitions.c
  - tests/parser/test_syntax_hygiene_compiler_prototype_lookup.c
  - tests/cmake/macro-foundation-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lookup.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lookup.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_nominal_prototype.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_prototype.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_prototype.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_substitution.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_substitution.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/parser/test_syntax_hygiene_compiler_prototype_lookup.c
  - tests/parser/test_syntax_hygiene_generic_prototypes.c
  - tests/parser/test_syntax_hygiene_generic_definitions.c
  - tests/parser/test_syntax_hygiene_nominal_prototypes.c
  - tests/parser/test_nominal_declaration_publication.c
  - tests/parser/test_generic_constraints.c
  - tests/parser/test_generic_instantiation.c
  - tests/parser/test_canonical_type_graph.c
  - tests/parser/test_canonical_consumers.c
doc_type: module-detail
---

# Macro Generic Type Identity

M1 must preserve an identifier's declaration identity after a generic type is
specialized. `Parcel<int>` from a definition-site declaration and the same text
from a Root declaration can describe different types. Even with a common Root
`Parcel<T>`, two `Widget` arguments can have different nominal definition tokens.
The display spelling cannot select or cache either specialization.

## Support Boundaries

The normal path is declaration parsing, contextual TYPE binding, canonical
generic-definition registration, structured annotation conversion, open
prototype selection, closed prototype materialization, and compiler consumers.
Each layer must work before an upper-layer construction or member test can
establish the complete behavior.

The initial prototype fixture uses real generic union declarations, whose
publication path already preserves the contextual nominal binding. It first
checks ordinary Root and mixed-site ITEM behavior. A second fixture isolates
struct canonical-definition registration using empty source declarations and
their actual bound nominal IDs. This avoids attributing producer failures to
the materializer or working around them in constructors.

## Specialization Key

The materializer uses the canonical generic instance identity: the selected
nominal definition plus the complete ordered argument list, including recursive
type identities and integer const values. A closed prototype stores the scalar
`genericInstanceTypeId` in its compiler context. This ID moves with the prototype
when the array grows and does not require a separate allocation or destructor.

The key is the generic instance itself. A union's substituted payload projection
is a separate semantic operation. Empty or unused parameters must still remain
in the specialization key. Names remain available for diagnostics, imported
module lookup and the legacy Root wrapper, but do not merge distinct structured
instances.

Nonzero nominal definition tokens resolve through the actual TYPE symbol and
its source declaration. A failed marked lookup cannot select a same-spelling
Root definition. Closed source prototypes borrow the selected open declaration
under the compiler's existing AST lifetime. Imported metadata prototypes retain
their null declaration pointer because the temporary import AST is freed.

Root name-only queries must also exclude a Root definition's specialization
when an argument contains a non-Root nominal identity. Otherwise inserting
`Parcel<Widget>` with a generated `Widget` first would change the meaning of a
later Root name query. Structured queries use the exact instance key in either
insertion order.

Open const arguments use the enclosing declaration's real symbol ID and the
parameter ordinal. A function's child type environment owns its display-name
bindings. The enclosing Root environment must not retain an open template name.
Direct and recursively nested arguments must carry the same const owner identity.

Source struct canonical-definition metadata uses the contextual declaration
binding's nominal ID and symbol ID. It snapshots these scalars before interning
or table growth. Looking up a same-spelling Root nominal or the first TYPE symbol
would attach generic ownership and capabilities to the wrong declaration.
Class, struct and constructor constraints likewise resolve the actual structured
argument before inspecting prototype capabilities.

The `AST_TYPE` branch of prototype-target resolution retains this structured
selection until it returns the prototype. A generic constructor target parsed
as a type AST must return the same declaration and instance key as annotation
conversion. This support handoff is distinct from later constructor emission,
which still has name-only metadata lookups at this checkpoint.

## Ownership And Compatibility

Generic substitution is private shared support for call inference and prototype
materialization. The extracted support preserves structured nominal tokens,
ownership qualifiers, recursive arguments and existing constraints. The generic
prototype module owns selection and materialization; ordinary nominal lookup
keeps its existing non-generic behavior.

Canonical IDs are local to the owning semantic context. Callers retain scalar
IDs and reacquire nodes after interning or array growth. This slice does not
define stable binary macro identities. The existing public inferred-type
representation is unchanged.

## Validation Checkpoint

The first ten native cases establish actual RED: 2 PASS, 8 FAIL, zero ignored,
after a successful MSVC build. Root and mixed-site prerequisites pass. Marked
open selection, distinct definition caches and same-spelling nominal arguments
fail. Immutable logs are
`E:/cargo-targets/zr_vm/reports/m1-msvc-generic-prototypes-red-r1-test.log` and
`m1-msvc-generic-prototypes-red-r2-test.log`; the latter corrects fixture integer
narrowing warnings without changing its assertions.

The first repair passes all 17 cache and annotation cases, including reverse
insertion, Root name queries, invalid identity, nested type arguments, integer
const arguments and 64-entry growth/reuse. The next native gate establishes
separate actual RED: two marked argument constraints fail while the Root
prerequisite passes; seven struct-definition cases fail while Root and mixed-site
prerequisites pass. These results are retained under
`m1-msvc-generic-support-r1`. Its five binary fixture writer failures are separate
working-directory setup failures, preserved in the same logs.

The two-path repair then passes all nine struct-definition cases and both marked
constraints. Fresh `m1-msvc-generic-support-r2` builds successfully and passes the
related 26 nominal prototype, 38 publication, 1 constraint, 3 instantiation,
125 inference, 19 canonical graph and 21 canonical consumer cases with zero
ignored. The corrected external per-target writable working directories also
pass all five previously failed writer prerequisites.

The expanded 25-case generic fixture has 23 PASS and two actual failures: nested
const forwarding in one owner and across two owners. Direct `Layer<N>` preserves
the real owner, ordinal, distinct caches and Root exclusion. The failed outer
`Parcel<Layer<N>>` cannot form its instance key because recursive canonicalization
does not yet receive the enclosing const bindings.

The recursive binding repair passes all 25 generic cases in fresh
`m1-msvc-generic-support-r3`, with build0 and the same seven related programs
passing. Two new duplicate-declaration cleanup cases establish actual RED in
the expanded 11-case struct fixture: both Root and marked rejection retain the
original publication but leak 15,000 bytes of unpublished prototype arrays.
An early source binding guard repairs this failure before prototype array
initialization. Both cleanup and recovery companions pass in r4. Its expanded
17-case struct fixture then establishes three typed-target handoff failures:
marked-only resolution fails, and either Root/marked declaration order returns
the wrong source AST, despite correct canonical and annotation prerequisites.

The typed-target branch now uses the structured finder. A test-only invalid
context companion stops r5 at compile time because it referenced an undeclared
private helper; no test executes and this is not behavioral evidence. Corrected
r6 naturally exits0 after build0: all nine programs pass 278 cases with zero
ignored, including generic25 and struct definitions/typed targets20. Independent
read-only review finds no further issue in these frozen repairs. Full replay
`m1-msvc-generic-support-final-r1` builds successfully and passes all 39 selected
foundation entries. The original remaining inventory has 108 PASS and 13 FAIL;
all 121 program exit codes and Unity footer tuples match
`m1-msvc-nominal-publication-r5`, with no missing footer. The aggregate still
fails the two diagnostic catalogue assertions (71/94 and 188/142). These failures
remain acceptance obligations. Constructor emission, complete M1 and artifact
acceptance remain open.

The next lower support boundary is the compiler's name-only prototype finder.
Its Root query must exclude opaque declarations and specializations with opaque
arguments, while an explicit Root local alias retains its stored structured
target. An alias can deliberately reference an opaque type. Compiler lookup
keeps its existing first-registration precedence, which differs from inference's
source-over-import preference. Corrected native
`m1-msvc-compiler-prototype-lookup-red-r3` establishes22cases10PASS/12FAIL with
zero ignored after build0: six compiler Root queries and six alias cases fail,
with all structured producer prerequisites passing. Compiler direct and alias
first-registration companions pass. The narrow private lookup repair passes
all twelve related programs, 352 cases and zero ignored after build0 in
`m1-msvc-compiler-prototype-lookup-green-r1`. Its full replay passes all 40
foundation entries. The original inventory remains108PASS/13FAIL: all121 exits,
Unity footer tuples and failed-test messages match generic-support-final-r1,
with only memory-pointer values normalized and no missing footer. Constructor
and member emission remain separate obligations; this is not M1 acceptance.
Its contracts and fixture ownership are detailed in
[compiler prototype lookup](macro-compiler-prototype-lookup.md).

## Reference Evidence

Rust keeps a generic instance's definition and argument list together in
`lua/rust/compiler/rustc_middle/src/ty/instance.rs`. Its context identity is
`(parent, expansion, transparency)` in
`lua/rust/compiler/rustc_span/src/hygiene.rs`. The tests
`lua/rust/tests/ui/hygiene/generic_params.rs` and
`lua/rust/tests/ui/const-generics/early/const-param-hygiene.rs` exercise type and
const parameter hygiene across generated and interpolated names.

Javac's `ClassType` stores its `TypeSymbol`, enclosing type and type arguments
separately from `toString()` in
`lua/JDK/src/jdk.compiler/share/classes/com/sun/tools/javac/code/Type.java`.
`lua/JDK/test/langtools/tools/javac/types/GenericTypeWellFormednessTest.java`
checks actual parameterized bounds. Mono compares the generic container and
ordered argument types in `lua/Mono/mono/metadata/metadata.c`, including
`mono_generic_inst_equal_full` and `_mono_metadata_generic_class_equal`;
`lua/Mono/mono/tests/generics-sharing.2.cs` supplies generic sharing regressions.
ZR uses its existing semantic type graph and source binding identities.
