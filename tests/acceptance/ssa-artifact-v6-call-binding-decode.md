# SSA Artifact Call Binding Row Decode

## Scope

- Preserve the contract decoder's detailed validation status for call-binding artifact rows.
- Report an invalid metadata token as `ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN`; keep other malformed contracts as `ZR_ARTIFACT_STATUS_INVALID_SECTION`.
- Keep the public `ZrCore_CallBinding_DecodeContract` boolean result and its clear-on-failure output behavior.
- Affected layers: core artifact row decoding and parser artifact tests.

## Baseline

- The test-only MSVC run reproduced the regression: the new reader assertion expected `ILLEGAL_TOKEN` (status 12) but the old implementation returned `INVALID_SECTION` (status 10) at `tests/parser/test_call_binding_artifact.c:106`.
- Root reported the focused artifact test executable had seven cases and exactly this one failure; the other six passed. The configured build completed successfully.
- No repository-wide baseline failures were evaluated for this focused change.

## Test Inventory

- Focused unit target: `zr_vm_call_binding_artifact_test` / CTest name `call_binding_artifact`.
- Malformed-token boundary: replace the encoded signature token with the row's valid member token; check status, section, row, byte offset, and cleared output row.
- Non-token control: clear `bindingKind`; check `INVALID_SECTION`, exact location, and cleared output row.
- Public API controls: valid contract decodes successfully; invalid token and invalid binding kind return false and clear the entire output contract.
- Existing coverage in the target includes row roundtrip, version and reserved-byte errors, truncation, section mismatch, projection, duplicate callsites, forbidden artifact kinds, and canonical consumer integration.
- Focused execution was performed with MSVC. GCC and Clang were not run in this slice.

## Tooling Evidence

- Before the production fix, root ran the following test-only build from PowerShell, with all outputs in the D cache:

  ```text
  cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && cmake --build D:\tmp\zr_vm\ssa-artifact-v6-msvc --target zr_vm_call_binding_artifact_test zr_vm_task_frame_runtime_test zr_vm_execution_add_stack_relocation_test -j 2'
  D:\tmp\zr_vm\ssa-artifact-v6-msvc\bin\zr_vm_call_binding_artifact_test.exe
  ```

  The build completed 27/27 steps, exit 0. The artifact test returned exit 1 with exactly the expected RED: `expected 12 (ILLEGAL_TOKEN), got 10 (INVALID_SECTION)` at `tests/parser/test_call_binding_artifact.c:106`; the other six tests passed.
- Post-fix MSVC build command, run from PowerShell:

  ```text
  cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && cmake --build D:\tmp\zr_vm\ssa-artifact-v6-msvc --target zr_vm_call_binding_artifact_test zr_vm_task_frame_runtime_test -j 2'
  ```

  Result: 19/19 build steps succeeded; exit code 0.
- Direct test command:

  ```text
  D:\tmp\zr_vm\ssa-artifact-v6-msvc\bin\zr_vm_call_binding_artifact_test.exe
  ```

  Result: exit code 0, 8 tests passed, 0 failed.
- CTest command:

  ```text
  ctest --test-dir D:\tmp\zr_vm\ssa-artifact-v6-msvc -R '^call_binding_artifact$' --output-on-failure --no-tests=error
  ```

  Result: 1/1 passed; test time 0.56 seconds, total time 0.73 seconds.
- GCC and Clang commands and outputs were not run or assessed in this slice.

## Results

- RED confirms the new test distinguishes the old misclassification from the required writer-compatible token diagnostic.
- The private status helper preserves token classification, and the public boolean decoder retained its output contract across the added success and failure controls.
- `git diff --check` passed for the changed source, test, and documentation files; Git emitted only its existing LF-to-CRLF normalization warnings.

## Acceptance Decision

- Accepted for the focused MSVC validation: the target built successfully, the direct test passed all eight cases, and the CTest entry passed.
- GCC and Clang were not run in this slice; no result or cross-toolchain claim is made for them.
