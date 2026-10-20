---
related_code:
  - tests/parser/test_aot_c_generic_struct_return_cases.h
  - tests/parser/test_aot_c_method_info_signature.c
  - tests/cli/test_cli_project_incremental.c
  - tests/harness/runtime_support.c
  - tests/cmake/aot-contract-targets.cmake
  - tests/cmake/debug-metadata-aot-registration.cmake
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_frame.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_frame.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_return_layout.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_return_layout.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.h
implementation_files:
  - tests/parser/test_aot_c_generic_struct_return_cases.h
  - tests/parser/test_aot_c_method_info_signature.c
  - tests/harness/runtime_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_frame.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_return_layout.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.c
plan_sources:
  - "user: 2026-10-16 add a pure-memory CLI Stamp AOT ExecIR regression with failure-only diagnostics"
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
tests:
  - tests/parser/test_aot_c_generic_struct_return_cases.h
  - tests/parser/test_aot_c_method_info_signature.c
  - tests/cli/test_cli_project_incremental.c
doc_type: testing-guide
---

# AOT Generic Struct Return

## Purpose And Boundary

The pure-memory regression isolates source compilation and legacy AOT ExecIR
module construction beneath the CLI project-output path. Its input is the exact
`Stamp` program from
[the CLI full-AOT project case](../../tests/cli/test_cli_project_incremental.c),
whose compile assertion is in
`test_cli_compile_emit_aot_c_writes_full_aot_project_c_source`.
That assertion covers several output stages and does not identify a particular
backend rejection by itself.

[The focused case](../../tests/parser/test_aot_c_generic_struct_return_cases.h)
expects `backend_aot_exec_ir_build_module()` to return TRUE for the compiled
program. It neither changes production validation nor substitutes synthetic
types, frame slots, or return metadata to obtain that result. A module-build
PASS would establish only this prerequisite, not successful CLI AOT C emission.

## Compilation And Ownership

The input declares `Stamp` with an integer field and constructor, `RefA`, and
`stamp<T>` constrained to class arguments. It constructs and returns a local
`Stamp`, calls `stamp<RefA>`, and returns the resulting field. The decoded source
is 299 bytes, SHA-256
`F9604E15C5F6A65A1F15FAC8B1A32F2935222CA79F5A7AD9C3465EE207E231B2`.

The test creates an initialized state through
[runtime_support.c](../../tests/harness/runtime_support.c), allocates a logical
source-name string, and calls the public `ZrParser_Source_Compile()` API from
[compiler.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler.c).
It saves `state->global->emitCompileTimeRuntimeSupport`, sets it to TRUE for the
compile call, and restores it immediately after the call returns, including an
ordinary NULL compilation result. This matches the temporary compiler option
used by [the CLI compiler](../../zr_vm_cli/src/zr_vm_cli/compiler/compiler.c).
The source name is not a fixture-file path that the test creates or reads.

The harness state does not perform the complete CLI project and standard-module
bootstrap. State, source-name, and source-compilation failures therefore have
explicit `fixture prerequisite:` reasons and are not evidence of a return-layout
behavioral RED. No VM execution API, C writer, generated-path helper, or fixture
file writer is invoked by the focused case.

## Module Construction And Diagnostics

[backend_aot_exec_ir.c](../../zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c)
performs the first real module build. Only if it returns FALSE does the test
emit diagnostics to stderr. It does not inspect a failed partial module.

The diagnostic uses the real
[function-table builder](../../zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.c),
which includes both function-valued constants and child functions, with the
backend's normal definition-identity deduplication. This includes constructor
functions that a child-only search could miss. Each table entry reports its
flat index, name, source lines, raw callable-return flag and complete type-ref
fields, and every raw frame-slot row. Each RETURN_TYPED reports its SemIR and
execution indices, source slot, and type-table index. Available source type
refs include base and nominal type, static C type and layout ID, nullability,
ownership, array state, and element type. Missing tables and out-of-range type
indices are reported without dereferencing them.

After that entry's raw observations, a separate post-failure probe invokes the
real [frame builder](../../zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_frame.c).
It invokes the real
[direct-inline-return projection](../../zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_return_layout.c)
only when the frame build succeeds and the raw callable-return flag is a
canonical FALSE or TRUE. As production does, the probe copies the callable
return type and sets its known flag only for an exact TRUE source flag.
It does not manufacture a type, layout, or known value.

The output distinguishes `frameBuilt`, `returnProbed`, and `returnProjected`,
then reports the resulting known flags and layout ID. These are independent
helper calls after the original failure, not a trace proving the first rejecting
branch of that original module build. A skipped return probe is not a failed
return projection. The probe cannot replace a successful first module build.

Probe frames and the diagnostic function table are released before leaving the
diagnostic. The case releases the module, compiled root, and state before its
final Unity assertion can jump out. Ordinary prerequisite failures also destroy
an already-created state before reporting failure. This cleanup order does not
claim recovery from a fatal process crash or nonlocal compiler panic.

## Registration And Selector

The existing target is `zr_vm_aot_c_method_info_signature_test`, defined in
[aot-contract-targets.cmake](../../tests/cmake/aot-contract-targets.cmake), with
CTest name `aot_c_method_info_signature` registered in
[debug-metadata-aot-registration.cmake](../../tests/cmake/debug-metadata-aot-registration.cmake).
[The main translation unit](../../tests/parser/test_aot_c_method_info_signature.c)
includes the new header and registers
`test_aot_exec_ir_builds_cli_generic_struct_return_fixture` outside Unix guards.
The older Unix-only cases retain their original guards.

Exactly one argument, `--generic-struct-return-only`, selects only this new case
and ends Unity immediately afterward. The default path preserves the original
case order and appends the new case once; unknown argument forms still use that
default path. The source consequently contains two RUN_TEST call sites for the
same case in mutually exclusive paths, not two executions in either path.
The selector avoids unrelated C-writing fixtures during focused diagnosis; it
does not reduce or replace later default-suite and full-matrix requirements.

## Frozen Inputs And Current Status

The frozen test header is SHA-256
`2846F50E9271B0FEE2359ADDA4C69E89921A57F6E66E9042E1D7FFA8B5658C6D`;
the main translation unit is
`10075435105DD363071E713B4D1E99B562A52951E6B0A23E2BF1B5564B8B6B24`.
Static RAM-only checks established the exact CLI source-literal identity and
that removing the bounded main edits restores its prior SHA-256. They provide
no native execution or compiler credit.

Focused native receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r10-guard-green-aot-probe-r1-receipt.json`,
SHA-256 `B689AF9C4E1F43011B83F285B1CE2C0AB33F988A6D79959B41661852B5D884D5`,
records the normal build exiting zero and probe PID 42480 exiting one:
one Unity case, one failure, zero ignored. Public source compilation succeeded;
the actual module build returned FALSE. Post-failure production function-table
probes built root and constructor frames and projected their return layouts,
but `stamp` (flat index 2) returned FALSE from frame construction, so its return
projection was not run. Its RETURN_TYPED source is stack slot 2, a real inline
Stamp slot (size/alignment 8, layout ID 0); the failing frame condition still
requires diagnosis. Do not relax the return projector based on this result.

Source inventory 4,791, HEAD, six support/tool inputs and three tested binaries
were unchanged; all owned child processes reached natural terminal states.
The focused controller's zero exit means observation completed, not native
behavior PASS. No production repair, full CLI output success, complete Windows
matrix, GCC/Clang acceptance, or M1 acceptance is established by this document.
The broader M1-M6 scope in the linked plan remains unchanged.
