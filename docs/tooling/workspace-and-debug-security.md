---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_workspace.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_workspace.h
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_uri.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_lib_network/include/zr_vm_lib_network/network.h
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network.c
  - zr_vm_language_server_extension/src/debug/cliLauncher.ts
  - zr_vm_language_server_extension/src/debug/dapSession.ts
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_workspace.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_lib_network/src/zr_vm_lib_network/network/network.c
  - zr_vm_language_server_extension/src/debug/cliLauncher.ts
  - zr_vm_language_server_extension/src/debug/dapSession.ts
plan_sources:
  - user: 2026-10-12 audit fixes for workspace containment and CLI debug authentication
tests:
  - tests/language_server/test_lsp_workspace_path_security.c
  - tests/debug/test_debug_token_generation.c
  - zr_vm_language_server_extension/test/debugLaunchTokenHandoff.test.js
doc_type: workflow-detail
---

# Workspace path and debug launch security

## Workspace path ownership

LSP workspace roots are client supplied file URIs. `FileToNativePath` decodes each URI to a native absolute path, while watched-file notifications can cause the server to inspect or index the referenced file. A textual prefix is not a containment check: dot segments can leave a root, sibling names can share its prefix, and a path inside a root can traverse a symbolic link to an outside directory.

`ZrLanguageServer_LspWorkspace_IsPathWithinRoot` normalizes separator spelling, resolves existing filesystem links, and resolves missing paths against their nearest existing parent before collapsing remaining dot segments. This order preserves filesystem behavior for a symlink followed by `..`. Windows paths are converted from UTF-8 to UTF-16 for filesystem access and ordinal case-insensitive comparison; UTF-8 bytes are never case-folded individually. The helper then compares canonical paths at a directory boundary. Both workspace file-event filtering and root-removal ownership checks use this function. A file-event URI that cannot be safely converted or canonicalized is rejected.

The targeted regression covers parent traversal, prefix-sharing siblings, normalized in-root paths, Windows case and separator variants, Unicode case variants, and symlink traversal escapes on POSIX and Windows where symlink creation is permitted.

## CLI debug authentication

The CLI generates a fresh 256-bit token from the operating system cryptographic random source for every embedded debug-agent launch. Windows uses CNG through `BCryptGenRandom`; POSIX uses `/dev/urandom`. If secure random generation fails, debug startup fails closed. The CLI supplies this token as the agent's expected authentication token.

When the extension launches the CLI, the CLI writes a single `debug_endpoint=… debug_token=…` handshake line to stdout. The extension reads that line from the child process pipe, passes the token to `zrdbg/1 initialize`, and suppresses the handshake from DAP console output. It drops buffered startup output after parsing the handshake and stops the owned CLI if endpoint discovery or authentication fails. Startup diagnostics redact the token. Attach sessions remain configured with their explicitly supplied endpoint and token.

The network helper returns 64 lowercase hexadecimal characters and rejects undersized output buffers. The regression test checks output shape, unique successive tokens, and buffer validation. The extension test verifies that the launch result contains the token while ordinary CLI output remains visible and the token does not enter DAP output.

`ZrNetwork_TokenMatches` continues to support embedders that intentionally configure an agent without authentication. The CLI launch path does not use that mode: it always supplies a generated non-empty token.
