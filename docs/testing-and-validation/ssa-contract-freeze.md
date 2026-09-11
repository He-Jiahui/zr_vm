# SSA execution contract freeze

The shared execution contract is defined in
[`zr_vm_core/include/zr_vm_core/execution_contract.h`](../../zr_vm_core/include/zr_vm_core/execution_contract.h).
It contains only stable scalar identities: schema/ABI/logical versions,
metadata target token, generation, signature/layout/module hashes, capability
requirements, and declared effects.  Runtime pointers, AST nodes, and host
addresses are deliberately absent.

The current legacy readers remain on call-binding schema 1, artifact schema 5,
and AOT ABI 16.  The candidate shared execution contract is v6/v17; a version
mismatch returns `RECOMPILE_REQUIRED` instead of accepting an old artifact under
a new interpretation.  Contract checks distinguish target, signature, layout,
module, capability, effect, and stale-generation failures and populate a
scalar diagnostic with expected/actual values.

The focused `ssa_contract_freeze` test covers identical contracts, structured
identity failures, version/generation rejection, and the separation between
effect declarations and capability requirements.  It is intentionally
standalone so a contract regression is visible even when the larger parser
suite is unavailable.
