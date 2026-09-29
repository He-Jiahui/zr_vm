---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_native_call_binding.h
  - tests/parser/test_compiler_w2_performance_quickening.c
  - tests/parser/test_compiler_w2_quickening_array_add.inc
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/04-generated-fusion.md
tests:
  - tests/parser/test_compiler_w2_performance_quickening.c
  - tests/parser/test_compiler_w2_quickening_array_add.inc
  - tests/parser/test_span_core.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_call_binding_artifact.c
doc_type: acceptance-record
status: accepted
---

# Typed Array<int>.add Quickening

## Scope

This subtask closes one 03.04 generated-fusion gap: the late quickening pass
recognizes a typed `Array<int>.add` call emitted directly as
`KNOWN_NATIVE_MEMBER_CALL`. It rewrites only a same-site `MEMBER_GET` cache for
member `add` whose validated native provider contract is `DIRECT`/`CALL`, whose
module relocation targets that member entry, and whose receiver/value slots
are `Array<int>` and `int`.

The replacement `SUPER_ARRAY_ADD_INT` consumes the receiver and value slots
directly. Because it no longer uses the member-call cache, the compiler retires
that cache's binding contract and relocation before callsite finalization. A
guard miss leaves the original call and ordinary slot tracking intact.

## RED Evidence

Before the focused matcher change, the W2 quickening suite reported 19 tests
passed and one failure. The target test executed successfully and produced
`51`, but the function retained `KNOWN_NATIVE_MEMBER_CALL` and emitted no
`SUPER_ARRAY_ADD_INT`. An A/B run with the original member-slot classifier
reported 18/20 and the same Array add failure, showing that the matcher gap was
independent of the classifier correction.

Opcode inspection showed that this compiler route emitted a direct fused
`KNOWN_NATIVE_MEMBER_CALL` with a `MEMBER_GET` cache for `add`, a two-argument
count, a typed `Array<int>` receiver slot, and an `int` value slot. Earlier
matcher widening and pass-order changes still missed the opcode form. A first
direct rewrite attempt left the old direct module relocation attached to the
replaced instruction; final callsite linking rejected it with
`ZR_CALL_BINDING_INVALID_RELOCATION`. The accepted path checks the provider
contract and retires the binding and relocation only when it rewrites that
exact cache site.

## Verification

Build outputs were confined to the existing D cache
`/mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc`.

```sh
wsl.exe --exec cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc --target zr_vm_compiler_w2_performance_quickening_test -j 4
wsl.exe --exec cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc --target zr_vm_call_binding_artifact_test zr_vm_span_core_test -j 4
wsl.exe --exec cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc --target zr_vm_call_binding_pipeline_test -j 4
wsl.exe --exec /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc/bin/zr_vm_compiler_w2_performance_quickening_test
wsl.exe --exec ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc --output-on-failure -R '^(call_binding_pipeline|call_binding_artifact|parser_span_inline_receiver)$'
```

All three GCC builds exited 0. The direct quickening suite passed 20/20,
including the `Array<int>.add` result `51`, superinstruction, retired-binding,
and receiver-setup assertions. CTest passed all three registered gates: call
binding pipeline, call-binding artifact projection, and Span inline receiver.
The quickening Unity executable is run directly because it has no CTest
registration.

The root agent independently verified the current matcher with MSVC: its direct
quickening suite passed 20/20, and the registered `call_binding_pipeline` and
`call_binding_artifact` tests passed 2/2.

## File Boundary

`compiler_quickening.c` is over 11,000 lines and has several responsibilities.
This small matcher remains in the existing Array-int sweep because its
decisions depend on private member-cache, slot-alias, type-kind, and CFG
helpers from that sweep. The next useful extraction is the complete Array-int
late-specialization pass plus its helper family; a one-branch split would
recreate dependencies on the same file-local analysis state.

The Array add regression and its two specialized assertion helpers live in
`tests/parser/test_compiler_w2_quickening_array_add.inc`. This reduces the
1,591-line W2 quickening test implementation file by moving the separate
fixture behind its own semantic include boundary.

This acceptance is limited to the Array add subtask. SSA 03.04 remains open.
