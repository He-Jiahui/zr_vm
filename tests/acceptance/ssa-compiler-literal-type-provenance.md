# SSA 01.02: canonical source type for literal constants

## Red/green contract

`test_pre_semantic_ir.c` first asserted that the source literal CONSTANT,
its result value, and its temporary Place initialization share a non-invalid
TypeId. The MSVC 19.44 baseline failed `Expected Not-Equal` at the CONSTANT
type assertion (1/11 failed). The producer previously recorded a definition
and compiler constant-pool index, but left its type unresolved until the
destination local's `CONVERT`.

The compiler now registers the literal's runtime value category through the
existing canonical inferred-type graph. The resulting source TypeId is used
when adding the value and emitting CONSTANT, then reused by its temporary
Place. The destination local keeps its declared TypeId and its existing
CONVERT; no post-ExecBC type inference is involved.

## Validation and boundary (2026-09-18)

- Fresh MSVC target rebuild after integer and string fixture updates:
  `zr_vm_pre_semantic_ir_test.exe` 11/11 passed. The fixture checks that the
  string source TypeId has a `REFERENCE` semantic type record, not `VALUE`,
  and that its recorded pool index addresses an actual string constant even
  when compiler-internal constants occupy intermediate indices.
- WSL GCC and WSL Clang 14 from `D:/zr-ssa-verify-871bc234/wsl-gcc` and
  `D:/zr-ssa-verify-871bc234/wsl-clang`: final-source incremental builds
  exited 0, direct `zr_vm_pre_semantic_ir_test` runs each passed 11/11.
- Adjacent MSVC Span target rebuilt and remains 14/16 with the same two
  `GET_MEMBER` runtime failures seen before this type edit; the full target
  is not accepted.
- This fixture exercises integer and string literals and verifies type
  identity across source instruction/value/temporary. Other literal
  categories share the producer but are not individually asserted here.
- Computed expressions, overwrite of an existing destination Place, CFG/phi,
  and standalone constant payload remain outside this slice. This is a
  source-value-only change: ownership/lease balance is not exercised, and
  OOM/cancellation/partial-initialization handling is not newly accepted.
  Two earlier WSL GCC/Clang attempts failed at VM startup with host
  `CreateVm/0x800705b4` timeouts; after a successful `uname` probe, both
  final-source builds and focused runs passed. No broader Linux suite or
  sanitizer acceptance is inferred.
