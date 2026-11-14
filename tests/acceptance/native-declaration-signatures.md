---
related_code:
  - tests/language_server/test_native_declaration_signatures.c
  - tests/language_server/test_native_system_network_parameters.h
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.c
implementation_files:
  - tests/cmake/native-declaration-signatures-project-hook.cmake
  - tests/cmake/native-declaration-signatures-tests.cmake
plan_sources:
  - user: 2026-11-14 expose native parameter names/types in ZR previews and use ZR inheritance syntax
  - docs/parser-and-semantics/native-declaration-signatures.md
tests:
  - tests/language_server/test_native_declaration_signatures.c
  - tests/language_server/test_native_system_network_parameters.h
  - tests/language_server/test_lsp_interface.c
  - tests/fixtures/projects/native_math_export_probe/src/main.zr
doc_type: testing-guide
---

# Native declaration signature acceptance

## Scope

Register native parameter names and types in math, system and network modules;
project them into the ZR editor preview; correct Array's capacity parameter,
the ZR `fn` keyword, class inheritance and explicitly registered object types.

## Baseline

The reported Matrix4x4 preview contained arity comments instead of parameters.
Math descriptors omitted their parameter arrays. Inheritance projection emitted
`extends` / `implements`. The parser consumes a colon type list on classes and
interfaces, and does not accept an inheritance clause on structs.
Tensor's constructor hint said `fillValue: float` although the callback reads an
array. Array's constructor reused an index parameter descriptor.

## Test inventory

- Descriptor traversal checks each finite callable's count, names and types.
- Matrix components are checked in callback argument order, including both
  constructor arity ranges.
- Preview assertions cover scalar functions, matrix factories and operators,
  Tensor construction, Array construction, TCP/UDP, console and environment,
  and preserve optional arguments and zero-argument forms.
- Parser checks cover class inheritance and interface metadata on struct headers.
- Descriptor navigation selects the exact Matrix4x4 method and Array constructor.
- Missing and explicitly unresolved types remain distinct from a registered
  object contract.
- Existing LSP navigation and native math runtime probes cover surrounding
  behavior. The math probe covers zero-argument `init`, zero-argument `new`,
  a full typed component list, and module export dispatch.

## Tooling evidence

Run the focused CTest with the normal test targets enabled or register it by
configuring with `-DBUILD_TESTS=OFF` and
`-DCMAKE_PROJECT_ZR_VM_INCLUDE=<absolute checkout>/tests/cmake/native-declaration-signatures-project-hook.cmake`.
With the latter configuration, build `zr_vm_native_declaration_signatures_test`
and `zr_vm_cli_executable`, then run
`ctest --test-dir <build> --output-on-failure -R '^native_declaration_signatures$'`.
Run `zr_vm_cli` with `tests/fixtures/projects/native_math_export_probe/native_math_export_probe.zrp`
and check for exit code zero and output `9`.

Builds used the actual checkout at `E:/Git/zr_vm`, which contains concurrent
edits. Outputs belong to
`E:/cargo-targets/zr_vm/{build,reports,tests,tmp}/native-declaration-signatures-20261115`.
The report manifest records source identity, process exits and cleanup.

## Results and acceptance decision

The focused GCC, Clang and MSVC checks each exercised 150 callables without a
failure. Each toolchain also ran the native math project successfully. The MSVC
stdio language server returned `zr.math` and `zr.container` through
`zr/nativeDeclarationDocument`; both strings matched the delivered `.zr` files
byte for byte. See the `*-delivery-test.log` and `msvc-stdio-delivery-preview.log` files in
the task report. Before the fixes, the descriptor and preview regression failed
and the newly typed constructors caused the zero-argument `init` probe to fail.

The existing LSP interface and semantic-parity checks passed the native
descriptor rendering, native preview ranges and console signature cases. The
wider standalone LSP runners on the working checkout still reported failures:
13 in the interface runner, 4 in semantic parity and 49 in project features.
Failures include binary fixture preparation, cross-snapshot native identity,
project indexing and descriptor plugin completion. The plugin runner failed at
completion before it reached the declaration range checks. This accepts the
focused native preview, parser and runtime coverage; it does not attest to the
full LSP suite.
