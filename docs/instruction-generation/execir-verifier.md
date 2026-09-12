# ExecIR effect and token verifier

`ZrCore_ExecIr_VerifyEffects` performs the conservative effect portion of
ExecIR verification. Opcode schema flags are checked against instruction
flags; memory-reading and memory-writing instructions must carry non-zero
memory token ranges; token identities are non-zero and monotonic; and
observable instructions carry an increasing effect-token pair. Phi nodes are
required to have one incoming for each predecessor, with no duplicate or
foreign predecessor edge.

The parser-facing `ZrParser_ExecIr_Verify` entry point delegates to the core
module verifier. Diagnostics preserve function, instruction, and source
identity so malformed IR can be rejected before execution or projection.

This milestone intentionally does not infer effects from parser facts or
construct exception edges. Those facts are supplied by the SemIR lowering
milestone; missing flags and tokens are rejected conservatively here.
