# Branch range/null producer acceptance

Status: implementation supplied for root validation; GREEN execution unverified.

Root observed actual RED on the deliberately empty producer: 36 cases,
33 failures, process exit 1, including positive `lt` and `proved-immediate`
assertions. Receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/llvm-branch-red-v4/Root-receipt.json`.
Receipt SHA256: `f21bc4118ef0c98ec38bc3fb121dab6c80cb7c169c0c0fe26790e9cf09174424`.
Root reported LLVM clang 19 plus LLD, current source/dependency checks,
natural process exit and empty/reaped Job. This receipt is RED evidence and
does not establish feature acceptance.

Current standalone fixture has 53 cases. Expected successful output:
`branch range/null: 53 cases, 0 failures`, process exit 0. Root owns compilation,
runtime execution and evidence capture. The agent did not run compiler or VM.

The unit target requires the new producer and fixture plus the existing Core
ExecIR verifier closure already used by SSA tests. No parser wrapper, shared
GVN/ranges/pass-manager/canonical edits are dependencies of this producer.

Coverage includes all six predicates, swapped operands, four INT64 boundaries,
opaque sign/null type tokens and constants, mixed domains, unsupported selector,
explicit/nullability metadata, copied witnesses, revision/generation/IR/pool
invalidations, malformed module/value/block/range inputs, local-seed rejection,
failed/successful rebuild, definition liveness, excluded endpoints, overflow
witnesses, conservative arithmetic, PHI hull/UNKNOWN/null joins, unreachable
PHI paths, parallel edges, reachable/unreachable cycles, nested intervals,
contradictory edges and a 128-block DAG.

Remaining gates: actual GREEN, allocation fault injection and sanitizer evidence,
production CMake registration, real upstream representation-witness producer,
consumer proof integration and backend differential tests. Whole 02.02 remains
open; loops still require widening and narrowing.
