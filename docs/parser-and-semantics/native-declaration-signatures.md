---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.c
  - zr_vm_lib_math/src/zr_vm_lib_math/scalar/scalar_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix3x3_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix4x4_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/quaternion/quaternion_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor_registry.c
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_system/src/zr_vm_lib_system/console/console_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/env/env_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/exception/exception_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/process/process_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/vm/vm_registry.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/tcp_registry.c
  - zr_vm_lib_network/src/zr_vm_lib_network/registry/udp_registry.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type_definition.c
implementation_files:
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.c
plan_sources:
  - user: 2026-11-14 native C registrations must expose parameter names and types in ZR preview files; implements is not ZR syntax
  - docs/zr_language_specification.md
tests:
  - tests/language_server/test_native_declaration_signatures.c
  - tests/language_server/test_native_system_network_parameters.h
  - tests/language_server/test_lsp_virtual_declaration_projection_cases.h
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_native_type_member_identity_cases.h
  - tests/language_server/test_lsp_project_features.c
  - tests/cmake/native-declaration-signatures-tests.cmake
  - tests/fixtures/projects/native_math_export_probe/src/main.zr
  - tests/acceptance/native-declaration-signatures.md
doc_type: module-detail
---

# Native declarations: registered signatures and ZR preview text

Native modules are implemented in C, but their editor declarations describe the
public ZR API. The preview at `zr-decompiled:/<module>.zr` takes parameter names
and types from each callable's `ZrLibParameterDescriptor` array. Argument counts
alone do not provide a signature: `minArgumentCount` and `maxArgumentCount`
describe dispatch arity and must remain separate from the parameter table.

For example, the Matrix4x4 factory registration supplies three value parameters,
`x: float`, `y: float` and `z: float`. Its generated preview is:

```zr
pub static fn translation(x: float, y: float, z: float): Matrix4x4;
```

The same projection path serves module functions, instance methods, static
methods, constructors and operator meta-methods. Each registration owns an
immutable parameter array with static storage duration. Receiver parameters are
implicit and are never inserted into the public argument list. The native
callback, dispatch flags and accepted argument counts remain the runtime
contract; adding a descriptor does not change a callback's argument positions.

## Registered modules

The math registries register every argument-bearing scalar function and every
argument-bearing method or meta-method of Vector2/3/4, Quaternion, Complex,
Matrix3x3/4x4 and Tensor. Matrix constructor parameters follow the callback's
row-major component order. Tensor takes `shape: array` and `data: array`;
its registration must not claim that the second argument is a float fill value.

System console, environment, process, VM export invocation and exception
registries provide structured parameter tables. TCP and UDP do the same for
module functions and socket methods. Their signature hints remain display
metadata; a hint is not a substitute for registering the parameters.

The Array constructor uses `capacity: int`, separately from the `index: int`
parameter of its indexing methods. When the documented minimum is exactly one
argument below the maximum, the preview labels that final parameter `/* optional */`.
For a zero-argument form and a longer parameter list, it mentions the zero-argument
form without promising that every intermediate count works. Each matrix accepts
its existing zero-argument `new` (identity) or a complete set of components.
Passing the complete component list to `init` binds to those named and typed
parameters. The native struct's existing zero-argument `init` still uses ZR's
default value initialization. Native import marks this form explicitly so adding
constructor metadata cannot hide it or supply invented native argument defaults.

## Syntax and explicit type contracts

ZR class and interface declarations use a colon followed by a comma-separated
list of base types. The projector combines the native base type and interface
descriptors in their registered order:

```zr
pub class Array<T>: zr.builtin.IArrayLike<T>, zr.iteration.Iterable<T> {
    pub fn add(value: T): null;
    pub @constructor(capacity: int /* optional */): Array<T>;
}
```

The ZR parser expects `{` after a struct header without constraints. For `Pair`
and other native structs, the preview preserves interface metadata in a comment
instead of emitting a clause the parser rejects. Ordinary functions and methods
carry the `fn` keyword; meta-methods retain their reserved `@constructor`,
`@getItem` and operator names.

An explicitly registered `object` type is a valid ZR contract and is rendered as
`object`. Matrix multiplication accepts either the matching matrix or vector
object, and retains its existing broad object contract. Span's backing source
also retains its registered object type. The renderer does not manufacture a
nominal type named after a union expression to make a preview appear more
precise. Absent, empty and explicitly unresolved `unknown` metadata continue to
render an identifiable missing-type placeholder.

Declaration identities and navigation ranges still come from the same text
builder. Adding `fn`, longer parameter lists or inheritance text therefore moves
the recorded positions with the actual generated text.

## Validation boundary

The focused regression traverses the actual descriptor tables and renders the
actual previews through `ZrLanguageServer_LspNativeDeclarationProjection_Build`,
via the editor's `ZrLanguageServer_LspVirtualDocuments_RenderDeclarationText`
entry point. It checks parameter tables, concrete signatures and declaration
navigation. It also parses the generated type headers with the ZR parser,
including class inheritance combinations and native struct interface comments.
The native extern preview wrapper presents metadata for the editor; it is not a
compilable FFI source file.

The existing LSP interface regressions verify descriptor identity and navigation
coordinates against the generated declarations. Toolchain evidence is recorded
in the linked acceptance document and the task artifact manifest.
