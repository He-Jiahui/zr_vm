# SSA 01.05: scalar/control ExecBC projection runner

The projection now has a pointer-free instruction consumer independent of the
ExecIR oracle. It executes constants, copies, moves, conversions, checked
numeric arithmetic, comparisons, branches, switches, phis, and returns using
physical value slots. Unsupported effect and runtime-ABI instructions retain
their metadata but set `runnable` false and are rejected before execution.
The runner does not replace the VM's default interpreter or emit bytecode.

`ssa_oracle_projections` compares scalar addition, both numeric conversion
directions (including an inferred result type), branch/phi selection and MOVE
ownership behavior against the
direct oracle. An unsupported iterator/invoke/payload projection fails with
`UNSUPPORTED`; signed overflow fails without replacing an earlier successful
result. `ssa_oracle_parallel_edges` executes a branch whose terminator
uses a separate successor pool range, traverses its synthetic edge, and
returns the value from the selected parallel phi incoming. A verified loop
with entry and backedge phis stops at its explicit instruction limit without
publishing partially initialized result slots.

## Validation (2026-09-26)

Both projection targets rebuilt and their two CTests passed in WSL GCC 11.4
Debug, WSL Clang 14 Debug, Windows MSVC 19.44 Debug, and WSL GCC ASan/UBSan.
The GCC Debug SSA-label sweep passed 80/80; the MSVC Debug
`zr_vm_parser_static` build compiled the runner and linked the library.
This stage does not close effect callbacks,
per-function production cutover, or AOTIR/C/LLVM execution gates.
