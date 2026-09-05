---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/lsp_capability_registry.h
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - zr_vm_language_server/stdio/stdio_initialize.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - zr_vm_language_server/stdio/stdio_initialize.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/05-native-web-capability-parity.md
tests:
  - tests/language_server/test_lsp_capability_registry.c
  - tests/language_server/stdio_protocol_inventory.js
  - tests/language_server/stdio_resolve_capabilities_smoke.js
  - tests/language_server/stdio_smoke.js
  - zr_vm_language_server_extension/test/serverCapabilities.test.js
doc_type: milestone-record
---

# Identity-Only Resolve Capability Withdrawal

## 状态与产出记录

| Started | Completed | Status | Completed items | Evidence |
| --- | --- | --- | --- | --- |
| 2026-09-05 16:24 +08:00 | 2026-09-05 16:56 +08:00 | completed | Runtime resolve masks; four native identity methods withdrawn; Web identity resolvers withdrawn; complete initial payloads and native material resolve preserved; tests, module docs and independent reviews complete. | GCC/Clang/MSVC focused GREEN and extension compile/unit/noEmit below. |

## Contract

Workspace symbols, inlay hints, document links and code lenses return their
complete current payload in the initial response. They must not advertise
identity-only resolution. Their obsolete native and Web resolve handlers and
registrations are removed; an explicit request to a withdrawn method receives
MethodNotFound.

Native code-action resolution retains document snapshot revalidation. The Web
worker has no equivalent resolver and therefore returns its existing complete
initial actions without advertising resolve. Completion and code-action resolve
support is described per runtime, independently of the base provider mask.

The registry remains runtime-neutral. It must reject invalid resolve masks,
identity-only publication and resolve support outside a provider's runtime
coverage. Withdrawing resolve must not remove the base provider.

## Source Identity And Validation

The baseline is `c95e53871aa38a884d80a23873ef9251d81f71d9`, exported with
the exact committed gitlink revisions. The initial native RED overlays only
`test_lsp_capability_registry.c` and `stdio_protocol_inventory.js`. Other
sessions' semantic, runtime, FFI and benchmark overlays are excluded.

- Windows export: `.codex/lsp-optimize-validation/source`.
- WSL export/build: `/home/hejiahui/.codex-builds/lsp-optimize-20260905-root/source`
  and `gcc`.
- MSVC build: `.codex/lsp-optimize-validation/msvc`, Debug static.
- Initial extension test attempt: missing generated `out/*.js` modules.
- Initial compile attempt: missing installed TypeScript/VS Code dependencies;
  restored with the existing lockfile through `npm ci` before behavioral RED.
- GCC, Clang and MSVC registry RED: exactly 14 failures for four identity-only
  descriptors and the incorrectly classified native material code-action resolver.
- GCC and MSVC initialize RED: all four identity-only resolvers are advertised.
- GCC retired-method RED: all four unsupported resolve requests return
  `result: {}` instead of `-32601 Method not found`.
- Web callback RED: 7 tests, 3 pass and 4 expected failures for document-link,
  code-lens, code-action publication and retained identity handler registration.
- GCC broad baseline: lifecycle, protocol conformance and workspace folders
  pass. Full smoke fails on generic completion detail before later resolver
  checks; diagnostic-fix smoke lacks possibly_uninitialized_read. Both precede
  this production edit and remain open in the integrated baseline crosswalk.

## Completed Items And GREEN Evidence

- Registry metadata is immutable process-lifetime data. `resolveRuntimeMask`
  must be a subset of base coverage, empty when resolve is absent and nonempty
  for material resolve. Native completion/codeAction do not imply WASM resolve.
- Four native echo handlers, dispatch routes, method constants and obsolete
  position-encoding exemptions are removed. Initial symbol/link/lens/hint data
  remains complete, and all withdrawn methods return the exact `-32601` envelope.
- Native code actions preserve version-zero provenance and validate current,
  stale and refreshed snapshots. Stale actions lose edits and acquire a disabled
  reason; they are not JSON-RPC errors. Web has no snapshot resolver and no
  longer advertises or registers its identity code-action resolver.
- The focused native fixture uses the existing shared protocol client and has
  its own CTest registration. The full smoke no longer asserts echo responses.
- Spec compliance and independent code quality review found no remaining
  actionable issue. The one stale-action documentation error was corrected.

| Validation | Configuration | Result |
| --- | --- | --- |
| GCC focused CTest | GCC 11.4.0, Debug shared, WSL Node 12.22.9 | 7/7 pass, 8.43 s |
| Clang focused CTest | Clang 14.0.0, Debug shared, WSL Node 12.22.9 | Resolver/registry/inventory/lifecycle/sync/workspace all pass; first protocol run times out in cancel-known case; isolated unchanged protocol rerun 1/1 pass, 3.85 s |
| MSVC focused CTest | MSVC 19.44.35228, Debug static, Windows Node 22.13.1 | 7/7 pass, 20.61 s |
| Extension compile | `npm run compile` | exit 0, desktop TS and actual browser/worker bundles |
| Extension unit | `npm run test:unit` | 38/38 pass, includes 7 actual worker callback cases |
| Extension configured noEmit | `npx tsc -p . --noEmit` | exit 0 |
| Scoped whitespace check | `git diff --check -- <owned paths>` | exit 0; repository CRLF conversion advisories only |

Common native build targets:

```text
cmake --build <build-dir> --target zr_vm_language_server_stdio zr_vm_language_server_lsp_capability_registry_test zr_vm_language_server_stdio_server_lifecycle_test --parallel 6
ctest --test-dir <build-dir> --output-on-failure -R ^language_server_(lsp_capability_registry|stdio_(protocol_inventory|resolve_capabilities_smoke|protocol_conformance|document_sync_conformance|server_lifecycle|workspace_folders_smoke))$
```

The exact build directories are the WSL `gcc` and `clang` siblings described
above and the Windows `msvc` directory. PowerShell invokes WSL through
`wsl.exe --exec` to avoid the user's shell interpreting regex globs. MSVC uses
`Invoke-VsDevCommand.ps1` from the installed `using-vsdevcmd` skill.

The extension commands run inside `zr_vm_language_server_extension` after
`npm ci --ignore-scripts --no-audit --no-fund` installs the existing lockfile.
The configured tsconfig excludes the worker. An additional strict ES2020,
CommonJS, Node10-resolution, webworker-lib TypeScript `createProgram` check
compares the current worker with an in-memory `git show HEAD:<worker-path>`
CompilerHost overlay: both have the same 17 code/file/message diagnostics,
with zero introduced diagnostics. Full worker strict-check closure belongs to
Plan 05 and is not implied by the configured noEmit result.

The cancel-known protocol fixture starts its 3000 ms timer immediately after
opening 2048 classes. A timing probe on the unchanged Clang binary observes
diagnostics at 1236.39 ms and cancellation at 1236.67 ms after process start;
open-analysis time consumes the response timeout. This supports a load-sensitive
test hypothesis but does not establish the cause of the first timeout. Preserve
the timeout as a Plan 01 revalidation item; do not relax cancellation budgets.

After the fix, the two broad GCC semantic tests fail at the same assertions as
before: normalized closed generic completion detail and missing
possibly_uninitialized_read. Responsibility remains Plan 03 compiler/query and
consumer convergence, followed by Plan 04 protocol acceptance.

## Source Version And Outputs

Validation uses committed baseline `c95e53871aa38a884d80a23873ef9251d81f71d9`
plus exactly this leaf's 17 code/test paths. No concurrent semantic/runtime
overlay is included. The commit containing this record owns those paths, module
documentation, original plan checkbox changes and index links. Its identity can
be resolved with `git log -1 --format=%H -- <this-record>` and will be linked in
the execution crosswalk; final global acceptance must use the final committed
source version after all required leaves close.

Outputs are the source/test paths in this frontmatter, the focused smoke CTest
registration in `tests/CMakeLists.txt`,
`docs/cli-and-tooling/lsp-capability-resolve-contract.md`, its category-index link,
and the focused validation section in `lsp-stdio-validation.md`.

## Remaining Gates

This leaf is complete. Plan 00's alias/empty-provider/version/runtime capability
claims, full semantic baseline, Web strict type errors and the cancellation
timeout remain open. Actual WASM/browser/editor parity and sanitizer/performance
matrices remain later-stage gates. This leaf does not promote Plans 01-06.
