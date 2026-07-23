# Syntax 05 M3 typed property access acceptance

## Accepted contract

- Source reads, writes, initialization, virtual/interface/inherited dispatch, and static access use
  the visible PropertySymbol and its linked getter/setter/init accessor identities.
- Compound assignment captures the receiver once and evaluates getter, RHS, operator, and setter
  exactly once in that order. Missing accessor halves are rejected before a partial write.
- Getter receivers are readonly; setters are writable; init accessors remain construction-only;
  static accessors have no receiver. Inline-struct accessors retain frame slot/Place provenance so a
  setter writes back to the source value.
- Getter/RHS/setter exceptions preserve normal call-info and cleanup order. `ref` and
  `ref readonly` property results remain the explicit Syntax 05 M4 boundary.
- Executable IO source patch 36 serializes `vmEntryClearStackSizePlusOne`; source and loaded property
  functions preserve stack boundary, member-entry identity, instruction bytes, and result.

## Focused evidence

- `zr_vm_property_access_lowering_test`: GCC, Clang, and MSVC each pass `22 Tests 0 Failures` with
  real process exit `0`; the final focused binary was run twice on every toolchain.
- The focused cases cover direct get/set, exact property/accessor metadata, `+=` and `*=`, one-time
  receiver/RHS evaluation, owned receiver cleanup, inline struct writeback, readonly/init/static
  boundaries, virtual/interface/inherited dispatch, source/binary roundtrip, and getter/RHS/setter
  exception ordering.

## Parent and runtime matrix

The frozen GCC and Clang snapshots pass, with real process exits:

- property M1 `16/16`, property M2 `21/21`, receiver boundary `28/28`;
- canonical consumers `16/16`, semantic query `27/27`, parser `75/75`, literal surface `57/57`;
- compiler integration `127/127`, known-native object call `61/61`, debug metadata `4/4`, decorator
  pipeline `4/4`, type-layout metadata `9/9`, artifact schema `14/14`, and AOT C call contracts `8/8`.

MSVC passes the same focused and parent property/parser/canonical/query matrix, compiler integration,
debug, decorator, layout, artifact, and AOT targets with zero Unity failures. Its independent
`zr_vm_object_call_known_native_fast_path_test` still reaches the pre-existing Debug assertion in
`function.c` during the stack-root set-by-index case (process `0x80000003`); this milestone does not
claim that unrelated baseline as GREEN or modify that runtime boundary. GCC and Clang prove the full
61-case known-native target.

## CLI and ownership audit

`classes_properties.zrp` now uses both instance and static compound property assignments. GCC,
Clang, and MSVC each exit `0` and print exactly `40`. The final promotion requires byte-identical M3
source/test/fixture inputs, `git diff --check`, an empty shared index before staging, and zero
parser/core/AOT/CMake/LSP paths outside the declared M3 ownership set.
