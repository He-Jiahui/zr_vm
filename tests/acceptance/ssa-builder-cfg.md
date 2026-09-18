# SSA 01.02: builder CFG adjacency slice

## Scope and observed RED

After `4fe07dbf`, an independent `ssa_builder_cfg` fixture linked the real
SemanticIR builder, dominator, SSA and core ExecIR implementations. The first
MSVC CTest exited 1 with `FAIL: builder lost the first outgoing edge` because
each successor append replaced its block's range. After correcting successor
ranges, the test failed with `FAIL: builder lost a merge predecessor or its
dominator`: each append likewise replaced the destination's predecessor range.
After gathering contiguous predecessor rows, the invalid-target fixture
failed with `FAIL: builder silently dropped an invalid target block`. These
were separate actual failing runs, followed by an input-edge preflight.

## Verified fixtures

- Four-block diamond: both outgoing edges, both incoming merge predecessors,
  and the entry as the merge's immediate dominator.
- Inline two-successor fallback: both outgoing edges and their separate
  predecessor rows; no outgoing-edge array needed.
- Invalid target: `INVALID_BLOCK`, source block 1, expected block count 1,
  actual one-based destination 2; original caller output remains unchanged.
- Malformed outgoing-edge backing array and inline count over its fixed
  capacity: `INVALID_RANGE` with the owning block, without out-of-bounds read.

## Execution evidence (2026-09-17)

MSVC through the VSDevCmd wrapper configured and built the standalone target
in `D:/zr-ssa-verify-871bc234` (E: lacked space for another full build).
`ctest --test-dir D:/zr-ssa-verify-871bc234 -R
'^ssa_(builder_cfg|dominator_cfg|core_model|effects_verifier|pass_manager_scalar)$'
--output-on-failure --no-tests=error` reported **5/5 passed** after the final
inline-capacity fixture and builder cleanup (the same five tests had also
passed before those additions). MSVC printed the existing `/W3`-overridden-by-
`/W4` D9025 build warning; the target compiled and linked successfully.

WSL `uname -r` briefly succeeded, then the focused GCC sanitizer compile
failed before the compiler started with host resource error `HCS 0x800705aa`.
Retrying the read-only `wsl --exec uname -r` reproduced that host error.
No GCC sanitizer or Clang acceptance claim is made on that failed attempt;
the earlier dominator-only GCC/Clang results do not cover this builder change.

## Remaining gates

This is a focused implementation slice, **not** the 01.02 or M1 exit gate.
Semantic terminator and source fact coverage, exceptional and cleanup edges,
phi edge identity and renaming, four-backend differential behavior, and the
full GCC/Clang CTest matrix remain unverified. The pre-existing uncommitted
`ssa_construction` fixture also builds adjacency by overwriting a range for
each additional edge; that test remains outside this isolated commit and needs
its own fixture repair before it can serve as an integration gate.
