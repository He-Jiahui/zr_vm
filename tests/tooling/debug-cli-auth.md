# CLI debug authentication smoke

Run against an already-built CLI and the active task's `tests/tmp` directory:

```powershell
python tests/tooling/test_debug_cli_auth.py `
  --cli E:/cargo-targets/zr_vm/build/audit-fix-01a13019/msvc-debug/bin/zr_vm_cli.exe `
  --tmp-dir E:/cargo-targets/zr_vm/tmp/audit-review-01a13048
```

The script launches three real CLI instances and checks `initialize` with the
generated token, no token, and an incorrect token. It prints case names only;
the token is read from the child's private stdout handshake and is never printed
by the test. Each child is terminated in `finally`. The script creates only the
fixed `debug-cli-auth-fixture` directory, refuses to reuse it if it already
exists, and removes that owned tree after the cases finish.

The C LSP URI consumer regression is compiled with the language-server test
sources and run by its CTest target; it creates a unique fixture under
`ZR_VM_TESTS_BINARY_DIR` and removes it in Unity `tearDown`.
