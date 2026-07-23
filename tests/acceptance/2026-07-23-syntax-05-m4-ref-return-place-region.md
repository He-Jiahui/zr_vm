# Syntax 05 M4 reference property Place/region acceptance

## Scope

- Adds `ref T` and `ref readonly T` property getter contracts from parser/AST through canonical
  binding, Place/LoanId/region facts, managed VM references, executable artifacts and C/LLVM AOT.
- Covers parser, compiler, pre-execution Semantic IR, execution SemIR, VM frame/object/index storage,
  artifact reload, AOT runtime helpers, CLI regression and module documentation.
- Defers LSP presentation, final PropertyDef reflection and legacy-property migration to M5; those
  consumers must use canonical property/accessor SymbolIds and reference TypeId without text fallback.

## Baseline

- The initial focused RED lacked explicit ref-return AST intent, ref-property shape/effect binding,
  context-sensitive Place lowering, a managed runtime reference, stable SemIR/AOT operations and
  artifact parity.
- During GREEN, `zr_vm_pre_semantic_ir_test` exposed a reusable-ValueId loan boundary defect. The final
  implementation finds the actual borrow instruction and masks only pre-creation instruction uses;
  it does not alter global Place propagation or branch joins.
- `zr_vm_aot_c_source_contracts_test` has six unrelated stale source-text expectations. Frozen M4 and
  clean HEAD both report `24 Tests 6 Failures`, exit 6, with identical failure lines.
- `zr_vm_execbc_aot_pipeline_test` retains unrelated generic AOT lowering failures followed by the
  existing `gc_mark.c:1374` assertion. Frozen M4 and clean HEAD have identical 23 FAIL/Assertion lines
  on GCC/Clang and both exit 134; MSVC reaches the same assertion boundary as `0x80000003`.

## Test Inventory

- Focused `zr_vm_property_ref_return_test` covers exact syntax/ranges/contracts, rejected property
  shapes, class/static/inline/ref-struct/index Places, reference identity, assignment/compound store,
  readonly/non-addressable/native/escape failures, receiver effects, override/interface invariance,
  owner-loan conflicts and last-use, virtual dispatch, artifact reload and source/reloaded SemIR.
- The focused AOT cases inspect C/LLVM helper lowering and compile generated `property_ref_return_aot.c`
  and `.ll` objects on Unix. Cold and second-run VM executions verify the same managed reference.
- The parent matrix contains 26 targets: property M1/M2/M3/M4, reference syntax/place/loan/receiver/
  escape/ref-struct/owner, canonical/query/compiler/parser/literal/pre-SemIR, TypeLayout, GC/domain,
  artifact schema, three AOT contract targets and known-native object dispatch.
- Negative boundaries include setter/init on ref properties, ordinary value return, readonly writes,
  non-addressable receivers, local/temporary escape, active owner move/drop/share, ref-access mismatch
  and unsupported native raw-pointer projection.

## Tooling Evidence

- Frozen input: `40f316fe78daed270d095c7b21152856ae51fed7 + 50 exact M4 paths` at
  `.codex/s05m4-final-src-r1`; SHA-256 comparison against the working tree reports mismatch 0.
- GCC 11.4.0 and Clang 14.0.0 used fresh Ninja Debug directories with
  `cmake -S <snapshot> -B <build> -G Ninja -DZR_VM_BUILD_TESTS=ON -DCMAKE_C_COMPILER=<tool>`.
- MSVC 19.44.35228.0 used the repository `using-vsdevcmd` environment and a fresh Ninja Debug
  directory. Every Windows test ran in a hidden process with separate stdout/stderr and a five-minute
  hard timeout; no accepted target timed out.
- All 28 targets were built on every toolchain. The 26 expected-green binaries were then run one by
  one from the frozen source root, preserving their real process exit. The two baseline targets were
  run separately and never included in the green count.
- CLI command on each toolchain: `zr_vm_cli tests/fixtures/projects/classes_properties/classes_properties.zrp`.

## Results

- Focused: GCC, Clang and MSVC each report `23 Tests 0 Failures 0 Ignored`, exit 0.
- Expected-green matrix: GCC `26/26`, Clang `26/26`, MSVC `26/26`; every process exits 0. Notable
  summaries include compiler integration `127/127`, parser `75/75`, literal `57/57`, pre-SemIR
  `11/11`, TypeLayout inline `38/38`, GC `66/66`, known-native dispatch `61/61`, artifact `14/14`.
- CLI: all three toolchains exit 0 and print exactly `40`.
- Baseline-only: source contracts remain `24/6`; ExecBC AOT remains the identical clean-HEAD failure
  line set and assertion. Neither result is described as GREEN.
- Exact audit: 50 intended files, missing 0, snapshot SHA-256 mismatch 0, forbidden foreign Syntax/
  LSP/build/log/generated paths 0; `git diff --check` reports no whitespace error.

## Acceptance Decision

- Accepted on 2026-07-23 21:00 +08:00. All M4-specific and relevant parent gates pass on the same
  frozen three-toolchain input, generated AOT objects compile, CLI behavior is stable, and remaining
  failures are byte/line-equivalent clean-HEAD baselines outside this milestone.
- Remaining work is Syntax 05 M5 consumer/reflection/migration convergence; it may not add name/text
  inference for the reference contract established here.
