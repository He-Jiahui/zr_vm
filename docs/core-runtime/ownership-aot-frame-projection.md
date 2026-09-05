---
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_frame.c
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_frame.c
plan_sources:
  - docs/plans/astra/syntax/ownership-object-member-separation.md
tests:
  - tests/parser/test_aot_receiver_guard_shared_library.c
doc_type: module-detail
---

# AOT Frame Projection For Ownership Validation

`DIRECT_VALUE` is an execution-derived frame-slot hint, not a canonical frame
flag. The binary writer already excludes it. AOT frame validation and projection
must exclude the same bit, while retaining checks on every other flag.

Before this correction, all ten receiver/ownership C and LLVM product cases
failed while building execution IR: GDB observed flags32 against canonical
mask31. The generated file was never reached. After correction, GCC11.4 Debug
static directly passes the original eight product cases, including all five
ownership intrinsics, intrinsic-named members, optional/direct receiver access,
and scalar overwrite. Two newly added abrupt-cleanup parity cases advance to
execution and expose a separate pending-return restoration failure.

The projection change neither adds a flag to serialized artifacts nor changes
the artifact ABI. Windows product tests remain explicitly Unix-only and their
ignored results are not accepted as generated-code execution evidence.
