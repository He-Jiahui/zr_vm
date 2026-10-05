---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_parser/src/zr_vm_parser/compiler/module_init_analysis.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - tests/parser/test_ssa_source_script_entry_identity.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.h
plan_sources:
  - .codex/plans/20261004-source-aot-handoff-design-2005.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_ssa_source_script_entry_identity.c
  - docs/parser-and-semantics/ssa-source-script-entry-identity.md
  - docs/acceptance/ssa-source-range-identity.md
doc_type: acceptance
status: accepted-script-entry-identity-direct-green
---

# Source SCRIPT Entry Callable Identity Acceptance

## Finite scope and inventory

This record covers the pointer-free canonical FUNCTION witness on the actual
ordinary returned SCRIPT for a versionless, zero-export, pure script containing
exactly one explicit nonreference integer-literal return. It checks provenance
after `Source_Compile` releases temporary compiler state. Context-local IDs
remain provenance; the SCRIPT range represents an implicit callable origin.
The design document explains the actual semantic IR and canonical owner proof.
At this historical gate, parser positions were preserved with a point RETURN
range; complete statement spans and corrected token starts were outside its
acceptance. The subsequent [source range acceptance](ssa-source-range-identity.md)
records the current corrected SCRIPT/RETURN and CRLF ranges, with its own
13/13 direct parser gate and 8/8 consumers regression. It includes a current
13-case SCRIPT identity regression, while the receipts in this historical
record remain unchanged.

The focused fixture has thirteen Unity cases:

| Coverage | Cases |
| --- | --- |
| Initial publication | `return 9;` publishes ordinary i64 return metadata and the complete canonical witness |
| Independent compile lifetime | Keep `return 9;` rooted while compiling `return 8;`; compare all witness and existing return type fields |
| Malformed retry | Malformed grouped return yields NULL; same-name valid retry on the same state publishes a witness |
| Same-name sequence | Success, unsupported binary-return guard, success; prior result remains unchanged, middle result remains uncertified |
| Nine refusal guards | Bool, conditional both-return paths, implicit return, operandless return, binary return, multiple statements, local return, fallthrough, and a parameterized child declaration |

The complete passing path makes seventeen public compile calls: sixteen
successful and one expected malformed-source NULL. A failing RED assertion
can stop an individual sequence early, so this planned complete-path count is
not asserted as an observed RED execution count. Tests inspect metadata only
and do not execute generated code.

## Frozen diagnostic RED

The RED test commit is `a9b2307196b34a9dc8cd07d0f218effe0385a0cc`.
The immutable receipt is
`E:\cargo-targets\zr_vm\reports\ssa-20261005-01a0fe2b\script-identity-13-red-receipt.json`.
Its SHA-256 is
`c9dff042c9f4e8618ae0684e34cd1fb10e4bdeff8bff92ad6283aa1fad39a940`.
The frozen fixture SHA-256 is
`31f893c411a07e129d606bceac03b41cae8d9909421cae99396953bfe531d784`.

Build exited zero; focused CTest exited 8. The log reports
`13 Tests 4 Failures 0 Ignored`. All four positive cases failed precisely with
`FEATURE: script entry callable identity witness is absent`, after their
ordinary compile and existing i64 return-metadata preconditions. Nine guards
passed. The malformed-source diagnostic in the retry case is expected; the
valid retry reached the feature assertion. No UBSan diagnostic was emitted.

The build log SHA-256 is
`f14ec34ca881d701e670f3dde633bc1133855700eb94cfb672c90bd15daaa783`;
the CTest log `script-identity-13-red.log` SHA-256 is
`14baf1912392381dec3a0f8276e7fcec4665671bf86881489668022bf38055b1`.
Receipt, current frozen fixture, and CTest log hashes were read and verified
while preparing this record. RED establishes the intended missing feature;
it is not passing acceptance evidence.

## GREEN tooling and result

The direct build directory is
`E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2`.
The existing route uses Windows x64 clang-cl 19 Debug UBSan; the RED CTest log
records `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` and the real
`zr_vm_ssa_source_script_entry_identity_test.exe` executable in that build.
The checkout is compiled directly; the RED receipt records no source copy.

The current-source direct run built successfully (build exit 0) and ran
`ssa_source_script_entry_identity` with the typed-binding contract companion.
The SCRIPT identity fixture reported 13/13 PASS, 0 failures and 0 ignored.
The combined CTest exit was 8 solely because the independent
`ssa_typed_binding_contract` test had two reserved-flag RED failures; its two
passing witness tests and two expected reserved-flag failures are not part of
this gate. The identity test emitted no UBSan diagnostic under
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.

The recorded log is
`E:\cargo-targets\zr_vm\reports\ssa-20261005-01a0fe2b\script-identity-green-binding-flags-red.log`;
its SHA-256 is
`f308a3a97ad8f179a5c2a1ac5139f412719075673683259e635c3ca6fb2fe518`.
The final identity receipt is
`E:\\cargo-targets\\zr_vm\\reports\\ssa-20261005-01a0fe2b\\script-identity-green-receipt.json`;
its SHA-256 is
`bc281339a5cbf0039ad2248601a080450d0f46410623adf66e2cb0ebbbea193f`.
Both ordinary
source-test CMake and the direct driver register this fixture; only the direct
run above contributes this acceptance result.

## Decision and limits

Accepted for the bounded direct SCRIPT identity gate. Ordinary full top-level
CMake, WSL/Linux, native 32-bit, ASan,
LLVM/AOT, full parser coverage, and the full 47-item SSA plan remain unclaimed.
The fixture does not establish retained canonical graph ownership, frame or
artifact publication, serialization, token execution, AOT consumption, OOM
handling, or module-summary/cache rollback. Successful same-name retry and
field lifetime checks prove only their observed local contracts.

Validation uses local parser/Core compilation and metadata inspection.
Network, FFI, providers, external services, and security-boundary probes are
outside the run scope.
