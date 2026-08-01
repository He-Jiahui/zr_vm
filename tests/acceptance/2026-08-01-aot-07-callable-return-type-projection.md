# 2026-08-01 AOT 07 Callable Return TypeRef Projection

## Scope

This A7.2K sub-milestone copies a canonical callable return TypeRef into function-level ExecIR as a borrowed snapshot.
It migrates only MethodInfo return signatures and scalar-local direct-return proof. It does not change public ABI,
serialized metadata, reachability manifests, typed thunks, or aggregate return destinations.

## RED And Review Evidence

- A frozen current-HEAD tree with only the four test files overlaid failed compilation on the missing ExecIR fields and
  accessor, establishing schema-level RED before production edits.
- The first unreachable-owner fixture accidentally collapsed two anonymous children under the existing function-table
  equivalence rule. Distinct stable source positions fixed the fixture; its accepted baseline proves 3 functions become
  2 with one removed before the malformed flag is injected.
- Independent review found no production correctness issue and requested stronger authority evidence. The final Unix
  test builds an I64 snapshot, poisons raw metadata to BOOL, emits MethodInfo directly, and still requires I64. A second
  generated-product test sets presence false, poisons raw TypeRef to BOOL, and requires u64 inference and direct return.
- Final independent re-review reports `No findings`.

## Coverage Inventory

- Projects canonical true and preserves canonical false as unknown.
- Rejects noncanonical source presence flags before code stripping, including a proven removed owner.
- Treats TypeRef string pointers as borrowed for the source function graph lifetime.
- Proves sidecar-known/raw-poisoned and sidecar-unknown/raw-populated isolation.
- Uses only the sidecar in MethodInfo and scalar-local direct-return consumers.
- Preserves static bool/u64/f64 return inference when the sidecar is unknown.
- Leaves typed thunks, TypeLayout tokens, aggregate destination, and nested callable return ABI open.

## Tooling Evidence

Frozen effective source is committed HEAD `c3c4d45127c5468ddca4de90600850b392b49b2d` plus the exact A7.2K
four-production/four-test overlay:

- WSL source: `/home/hejiahui/codex-validation/zr_vm-aot12-20260801-a72k-red-c3c4d45-r1`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260801-a72k-c3c4d45-r1`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228.0 x64 Debug

## Results

- WSL GCC and Clang each pass MethodInfo 8/0, return contracts 1/0, SemIR 10/0, load-const scalar 1/0,
  code stripping 37/0, generic typed-call 19/0, generic sharing 9/0, debug metadata 6/0, value-SemIR 8/0,
  typed-call contracts 4/0, typed scalar 1/0, and call shared-library smoke 5/0.
- Windows MSVC passes MethodInfo 7/0, return 1/0, SemIR 10/0, code stripping 37/0, generic sharing 9/0,
  debug metadata 6/0, value-SemIR 8/0, and typed-call contracts 4/0. Generic typed-call has 19 total with four expected
  Unix-only ignores; load-const scalar, typed scalar, and call smoke are expected Unix-only ignores with zero failures.
- MSVC retains the existing temporary-directory MSB8029 warning. A CRLF-only false negative in an existing multiline
  source contract was normalized from the same frozen WSL source before the final 1/0 run.
- SHA-256 matches for all eight controlled implementation/test files across main, the frozen WSL tree, and the frozen
  Windows tree. `git diff --check` passes on the exact milestone path set.

## Acceptance Decision

Accepted at `2026-08-01 12:58:49 +08:00` as AOT 07 A7.2K callable-return TypeRef internal projection and
MethodInfo/scalar-local consumption. Aggregate return destination, A7.2, AOT 07, AOT 12, and the broader AOT 07-12
goal remain active.
