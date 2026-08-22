---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/lsp_uri.h
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_uri.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_position.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/stdio/stdio_document_file.c
implementation_files:
  - zr_vm_language_server/include/zr_vm_language_server/lsp_uri.h
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_uri.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/stdio/stdio_document_file.c
plan_sources:
  - docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
tests:
  - tests/language_server/test_lsp_uri.c
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_document_sync_conformance.js
  - tests/acceptance/2026-08-23-lsp-uri-native-path.md
doc_type: module-detail
---

# LSP URI And Native Path Boundary

## Purpose

The language server has one conversion boundary between LSP document URIs and
the host filesystem. `lsp_uri` prevents individual project, navigation, and
stdio paths from interpreting URI text differently or passing virtual document
schemes to native file APIs.

## Public Contract

`ZrLanguageServer_LspUri_FileToNativePath` accepts only an absolute `file:`
URI. It treats the scheme case-insensitively, recognizes an empty or
`localhost` authority as local, decodes strict percent escapes once, and rejects raw query,
fragment, backslash, control characters, embedded NUL, encoded path separators,
relative paths, and invalid output buffers. On failure it leaves a supplied
buffer as an empty string.

`ZrLanguageServer_LspUri_FromNativePath` accepts only an absolute native path
and percent-encodes UTF-8 bytes. It emits `file:///` drive paths on Windows,
`file://host/share/...` for Windows UNC paths, and ordinary absolute POSIX file
URIs on Unix. A percent in a native filename is emitted as `%25`; it is never
treated as an already encoded URI fragment.

`ZrLanguageServer_LspUri_Equivalent` compares equal URI text directly. For two
valid file URIs it decodes and normalizes separators and dot segments before
comparison. Windows comparison is case-insensitive. Non-file virtual URIs do
not receive filesystem normalization and are equal only when their bytes match.

## Consumer Rules

Project discovery and navigation call the public API directly. The old local
path/URI wrappers were removed, so a module identity cannot acquire a second
normalization policy. The retained interface compatibility forwarding functions
delegate to this canonical implementation rather than keeping a codec copy.

`stdio_document_file.c` converts the URI before it reaches `fopen`. Virtual
schemes such as `vscode-test-web:` and `zr-decompiled:` therefore fail closed
at the I/O boundary and cannot be interpreted as local filenames.

## Edge Cases

- `file://localhost/...` is local; on Windows its path must still be a drive
  path or another absolute native form.
- A non-local authority is rejected on Unix and maps to a UNC path on Windows.
- `%2F` and `%5C` are rejected before decoding to preserve one native segment
  representation and avoid path-boundary ambiguity.
- URI fragment and query syntax is not part of a native file path. Encoded
  `#` and `?` remain valid filename bytes after exactly one decode.

## Validation

`test_lsp_uri.c` covers POSIX and Windows native paths, drive and UNC handling,
spaces, `#`, `%`, UTF-8 bytes, manually encoded input, scheme/authority case,
dot segments, virtual URIs, malformed escapes, raw query/fragment syntax,
encoded separators, and destination overflow.

The focused target and stdio server were rebuilt on GCC, Clang, and MSVC. Each
toolchain passed the URI matrix and the serial JSON-RPC protocol and document
synchronization smoke drivers. The exact evidence is recorded in
`docs/plans/lsp/optimize/2026-08-23-canonical-file-uri-paths.md`.
