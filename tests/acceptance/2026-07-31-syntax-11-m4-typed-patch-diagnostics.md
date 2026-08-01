# Syntax 11 M4 typed Patch diagnostics acceptance

Date: 2026-07-31

Scope: the published non-empty `DeclarationPatch.diagnostics` operation only.
This record does not promote Gate 11 M4 or M5 as a whole.

## Contract

- `zr.compile.declaration.CompileDiagnostic` is a compile-tool-only typed value.
- Its constructible fields are exactly `isError: bool`, `message: string`, and
  `target: SymbolId`.
- Both `Patch.target` and diagnostic `target` must be nonzero 32-bit canonical
  SymbolIds. The diagnostic target must be the containing Patch target and the
  message must be non-empty.
- The complete diagnostic array is charged to the deterministic comptime
  diagnostic budget.
- All entries are decoded before emission. Warnings do not reject the Patch;
  any error rejects it before generated members enter the append/rebind path.
- INFO/WARNING/ERROR/FATAL diagnostics retain their corresponding core log
  severity; warning text is not projected through an error-level sink.
- The constructor shape participates in the canonical declaration provider
  contract hash (`fnv1a64:108584f1fed0e07e`).

## Reference evidence

- JDK `JavacMessager.printMessage` keeps processing diagnostics structured by
  severity and target position; `MessagerBasics.java` directly distinguishes a
  nonfatal warning from a final error.
- Roslyn `DiagnosticDescriptor` and `Diagnostic` provide typed
  severity/message/location identity. `GeneratorContexts.cs` and
  `IncrementalContexts.cs` expose diagnostics through the generator contract,
  while `GeneratorDriverTests.cs` verifies them separately from the output
  compilation.
- ZR intentionally exposes neither writable AST nor arbitrary generated source:
  diagnostics remain typed Patch data tied to one canonical SymbolId.

Repository paths:

- `lua/jdk/src/jdk.compiler/share/classes/com/sun/tools/javac/processing/JavacMessager.java`
- `lua/jdk/test/langtools/tools/javac/processing/messager/MessagerBasics.java`
- `lua/roslyn/src/Compilers/Core/Portable/Diagnostic/DiagnosticDescriptor.cs`
- `lua/roslyn/src/Compilers/Core/Portable/Diagnostic/Diagnostic.cs`
- `lua/roslyn/src/Compilers/Core/Portable/SourceGeneration/GeneratorContexts.cs`
- `lua/roslyn/src/Compilers/Core/Portable/SourceGeneration/IncrementalContexts.cs`
- `lua/roslyn/src/Compilers/CSharp/Test/Semantic/SourceGeneration/GeneratorDriverTests.cs`

## TDD evidence

1. Warning RED: the new integration case was the only failure in the 38-case
   executable because `CompileDiagnostic` was not constructible (`Expected
   Non-NULL`).
2. Warning GREEN: the complete executable passed 38/38 and emitted the expected
   compile-time warning while the generated field completed ordinary rebind.
3. Error RED: with error severity temporarily unimplemented, the new case was
   the only failure in 39 cases (`Expected NULL`) and compiled the Patch.
4. Error GREEN: the complete executable passed 39/39; the error rejected the
   Patch before the executor's generated-member append loop.
5. Descriptor RED/GREEN: the canonical constructor assertion failed 1/3 before
   the schema/hash update, then the attribute/descriptor executable passed 3/3.
6. Validator RED/GREEN: the direct Patch contract accepted an empty diagnostic
   message before the fix (one failure in four cases), then passed 4/4 with
   message and target validation shared by the executor.
7. Constructor RED/GREEN: four invalid constructor shapes initially produced
   values. The executable then passed 40/40 after exact bool/string/SymbolId and
   non-empty-message checks were added at the constructor boundary.
8. Budget-order RED/GREEN: an over-budget array initially failed during decode
   instead of reporting the diagnostic resource. After moving budget
   consumption before allocation and decode, the runtime contract passed its
   generic order case and the published default boundary (1024 accepted, 1025
   rejected with zero usage).
9. SymbolId narrowing RED/GREEN: `Patch.target = target.symbolId + 2^32`
   compiled successfully before the fix (one failure in 42 cases). Constructor
   and executor range checks now reject the alias and the executable passes
   42/42.
10. Severity RED/GREEN: a typed Patch warning reached the log callback at error
    level before the sink fix (one failure in 10 cases). The runtime contract
    now observes `ZR_LOG_LEVEL_WARNING` and passes 10/10.
11. Commit-order characterization: a direct compiler-state test processes a
    Patch containing one generated field and one error diagnostic, checks the
    exact error message, and proves the generated field symbol was never
    registered. This is separate from the still-open multi-add rollback work.

## Fresh WSL GCC and Clang commands

The source and build were isolated under `/tmp` from unrelated worktree debug
changes. The focused build used Ubuntu 22.04, GCC 11.4, CMake Debug, and Ninja.

```text
cmake --build /tmp/zr-vm-gate11-review-build \
  --target zr_vm_compile_time_test zr_vm_attribute_contract_test \
  zr_vm_comptime_contract_test zr_vm_comptime_runtime_contract_test \
  zr_vm_declaration_transform_contract_test -j2
LD_LIBRARY_PATH=/tmp/zr-vm-gate11-review-build/lib \
  /tmp/zr-vm-gate11-review-build/bin/zr_vm_attribute_contract_test
LD_LIBRARY_PATH=/tmp/zr-vm-gate11-review-build/lib \
  /tmp/zr-vm-gate11-review-build/bin/zr_vm_compile_time_test
LD_LIBRARY_PATH=/tmp/zr-vm-gate11-review-build/lib \
  /tmp/zr-vm-gate11-review-build/bin/zr_vm_comptime_contract_test
LD_LIBRARY_PATH=/tmp/zr-vm-gate11-review-build/lib \
  /tmp/zr-vm-gate11-review-build/bin/zr_vm_comptime_runtime_contract_test
LD_LIBRARY_PATH=/tmp/zr-vm-gate11-review-build/lib \
  /tmp/zr-vm-gate11-review-build/bin/zr_vm_declaration_transform_contract_test
```

Result: 61/61 across compile-time execution 42/42, attribute/descriptor 3/3,
comptime contract 2/2, comptime runtime 10/10, and declaration transform 4/4.
This is a focused assertion count, not the historical status-record count.

The same five targets and executable invocations were repeated from
`/tmp/zr-vm-gate11-review-clang-build` with Clang 14.0 and also passed 61/61. Clang
reported only the existing unused static test functions in
`test_compile_time_execution.c`; no production file changed by this slice
introduced a compiler warning.

## Remaining Gate 11 blockers

- non-empty `attributeAdds` (`interfaceAdds` is accepted separately in
  `2026-07-31-syntax-11-m4-interface-adds.md`);
- `GeneratedType`, `GeneratedMethod`, and `GeneratedProperty` execution paths;
- generated source maps and transactional rollback across multi-add failure;
- artifact, reflection, LSP, formatter, build dependency and migration consumers;
- removal of the legacy runtime decorator path.
