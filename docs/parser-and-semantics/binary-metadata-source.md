---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/module_init_analysis.h
  - zr_vm_parser/src/zr_vm_parser/compiler/module_init_binary_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/module/lsp_module_metadata.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/module_init_binary_metadata.c
plan_sources:
  - docs/plans/lsp/optimize/2026-09-07-plan01-task06-sub09-binary-metadata-source.md
tests:
  - tests/parser/test_binary_metadata_source.c
  - tests/language_server/test_lsp_cast_operand_facts.c
doc_type: module-detail
---

# Binary Metadata Source

Binary import analysis consumes the serialized `.zro` source graph. A neighboring
`.zri` file is a human-readable intermediate artifact and cannot replace, amend,
or supply missing binary semantic facts. This applies to compiler module-init
summaries, imported type inference and LSP module metadata.

## Entry and Ownership

`ZrParser_ModuleInitAnalysis_TryLoadBinaryMetadataSourceFromIo` borrows an `SZrIo`,
copies its cursor fields and uses the supplied state with `ZrCore_Io_ReadSourceNew`.
The underlying callback or file handle can advance; the copy does not create an
independent stream. The caller still closes the input exactly once. An input
with no read callback and no buffered bytes, or invalid adapter arguments, returns
false and clears an available output pointer. An empty callback-backed stream
still reaches the core decoder and retains its existing error behavior.

A successful result is a decoded `SZrIoSource` owned by the caller. The matching
`ZrParser_ModuleInitAnalysis_FreeBinaryMetadataSource` delegates to the core source
destructor. The global state must remain alive until release. See
[IO source lifetime](../module-system/io-source-lifetime.md) for nested native
storage, GC-managed values and runtime-copy ownership.

No filename probing or text decoder runs on this path. The former path-based
sidecar API and synthetic source marker are removed because they had no separate
consumer. Binary schema decoding, including older-version behavior and malformed
input handling, remains the responsibility of the existing core reader.

## Semantic Contract

The imported graph preserves the artifact's typed exports, metadata/signature
tokens and hashes, signature heap, module hash, declaration coordinates and child
function metadata. In particular, default arguments and callable children are not
reconstructed from displayed signatures. A current or stale `.zri` has no effect
on these fields.

The old substitution could successfully load a summary with zero identity fields
and `.zri` line numbers. Canonical LSP queries then rejected the unresolved target
even when the real `.zro` held a valid declaration. The fix is at artifact loading;
query-time identity checks remain required.

## Validation

The parser regression compiles a provider with a default argument, writes a real
binary and tests absent, current and stale intermediate files. It compares export
identity, signature and source coordinates to the producer and checks that child
parameter metadata survives loading. The LSP regression uses plain and cast calls
and checks canonical external identity and definition locations across the same
three artifact conditions. Toolchain results are recorded in the linked plan
record: GCC, Clang ASan/UBSan and MSVC pass all seven cases plus three existing
IO lifetime cases. Both focused Valgrind executables leave zero bytes and report
zero errors. The full Clang protocol run reaches the separate peak-memory limit;
it is not counted as a passing full sanitizer gate.
