---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/module_init_analysis.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - tests/parser/test_ssa_source_script_entry_tokens.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
plan_sources:
  - .codex/plans/20261004-source-aot-handoff-design-2005.md
tests:
  - tests/parser/test_ssa_source_script_entry_tokens.c
  - docs/acceptance/ssa-source-script-entry-tokens.md
doc_type: module-detail
status: accepted-focused-green-script-entry-token-gate
---

# Source Script Entry Tokens

This finite gate connects an eligible SCRIPT entry to the existing module
metadata token table. It uses the existing `MODULE`, `MEMBER_DEF`, and
`SIGNATURE` table kinds and the existing `METHOD_SIG` signature node. There is
no `METHOD_DEF` table enum. An entry marker value of 6 is represented by a Core
macro; the gate does not add a Core object field or change the Core ABI.

## Admission and finalization

The candidate is a versionless, zero-export, pure ordinary SCRIPT entry whose
actual validated CFG proves a no-argument i64 return on every reachable path.
The callable-return proof remains based on real CFG entry reachability,
acyclic finite paths, real return operands and their canonical i64 defining
instructions. Unsupported return paths remain ordinary successful compilation
without an entry definition.

The intended integration runs the callable proof inside source-module
finalization after module metadata attachments have been prepared, and before
metadata token refresh and summary hashing. It replaces the earlier outer
publication hook rather than executing the proof twice. This ensures token
construction sees the actual entry callable metadata.

The module identity comes from the actual current module key. The entry retains
that name through its existing `functionName` field and the normal GC write
barrier. It does not retain a pointer into the temporary AST or semantic
context. The admitted entry receives a module-owned `MEMBER_DEF` and its paired
`SIGNATURE` row, in addition to a module definition and module signature.

## Signature and hash contract

The entry uses the existing no-argument method signature format: a
`METHOD_SIG` root, version 1, zero generic arity, a primitive i64 return, and
zero parameters. Entity and signature rows must agree on ownership, related
tokens, signature blob offset and length, and signature hash. Tokens must have
nonzero RIDs, resolve through the existing lookup APIs, and the signature blob
must pass the existing validator.

The module ABI hash is a new versioned hash over module name bytes and entry
signature bytes. It is distinct from the signature-blob hash, the Core legacy
callable hash and any context-local canonical hash. Those values must not be
substituted for each other. This metadata records a finite compilation result;
it does not establish executable token dispatch or publish a retained IR.

## Test and lifetime coverage

The focused fixture makes twelve public source compile calls across eight
cases. Four eligible LT/GT comparison scripts must publish a unique module
record and one module-owned entry definition with a paired valid i64 method
signature. Each positive case roots the first returned function while
compiling a second source and then rechecks the first function's row count,
heap length, pointers and hashes. This checks ownership after temporary
compilation state has been freed.

Four guards cover fallthrough, mixed i64/bool returns, an operandless return
and implicit return. Their compilation must succeed while declining the
entry definition. Existing local bindings may legitimately publish other
metadata such as `TYPE_SPEC`; guard acceptance does not require deleting
unrelated valid metadata.

The actual RED V2 receipt records four exact missing-`MODULE` failures and four
passing guards. Focused GREEN V8 rebuilt the direct target and passed all eight
cases with zero failures and no UBSan diagnostic. Its CTest log is
`E:\cargo-targets\zr_vm\reports\ssa-20261005-01a0fe2b\script-entry-green-v8\ctest.log`;
the build used the existing direct validation tree under
`E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2`.
No source snapshot was created.

## Limits and security scope

This gate does not execute tokens, bridge ZRP artifacts, generate AOT output,
retain semantic IR, publish frame metadata, or claim the full SSA plan is
complete. It adds no global rollback guarantee and has no OOM-injection
coverage. Validation is confined to local ordinary parser compilation and
metadata inspection; network, FFI, provider, capability, hotpatch and security
boundary probing are outside the validation scope.
