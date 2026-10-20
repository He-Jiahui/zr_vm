---
related_code:
  - zr_vm_language_server_extension/test/debugLaunchTokenHandoff.test.js
  - zr_vm_language_server_extension/src/debug/cliLauncher.ts
  - zr_vm_language_server_extension/tsconfig.json
implementation_files:
  - zr_vm_language_server_extension/test/debugLaunchTokenHandoff.test.js
  - zr_vm_language_server_extension/src/debug/cliLauncher.ts
plan_sources:
  - user: 2026-10-12 Keep debug CLI handshake credentials private while forwarding complete stdout lines
  - .codex/plans/20261012-audit-fix-01a13019.md
tests:
  - zr_vm_language_server_extension/test/debugLaunchTokenHandoff.test.js
doc_type: testing-guide
---

# VS Code Extension Node Tests

The extension's Node tests load compiled TypeScript from `out/` by default.
To keep generated JavaScript outside the source checkout, compile the extension
to a managed output directory and set `ZR_VM_EXTENSION_TEST_OUTPUT_ROOT` when
running the test that depends on compiled sources:

```powershell
npx tsc -p tsconfig.json --outDir E:/cargo-targets/zr_vm/build/extension-tests/extension
$env:ZR_VM_EXTENSION_TEST_OUTPUT_ROOT = 'E:/cargo-targets/zr_vm/build/extension-tests/extension'
node --test test/debugLaunchTokenHandoff.test.js
```

The override points at the compiler output root, which contains `debug/cliLauncher.js`.
Without it, the test keeps using the repository's existing `out/` directory.
The test's fake CLI source uses escaped `\n` sequences inside JavaScript string
literals. The generated script therefore writes newline characters at runtime;
its fixture source does not contain accidental literal line breaks inside a
quoted string. The debug handoff regression also checks that a token split over
stdout chunks stays private, complete ordinary lines are forwarded as soon as
their newline arrives, and a final ordinary fragment without a newline is
flushed when the child stdout stream ends or closes.
