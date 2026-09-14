# Hotpatch capability validation contract

Hotpatch admission is a closed-world intersection:

    required = manifest.requiredCapabilities | every requirement.requiredBits
    admissible = (required & ~hostAllowedCapabilities) == 0

The capability closure is computed before generation preparation. Every
requirement must have a non-zero token and bit set, a zero reserved field, and
must fit the bounded requirement count. Unknown manifest flags, missing rows,
and capability escalation are reported with a structured diagnostic; callers
must not turn them into a generic false/success result.

ZrCore_HotPatch_ValidateCapabilityClosure delegates identity and signature
checks to the manifest validator, then uses the transactional closure seam in
hotpatch_capability.c. The output mask is written only after all rows pass.
Repeated requirements are harmless (bitwise union), while a single
out-of-policy bit rejects the complete patch.

The host must separately validate artifact schema, ABI/profile identity,
immutable content, and machine-code policy. Capability admission does not
authorize relocation or native imports by itself. iOS/WASM restricted profiles
therefore remain interpreter-only and reject relocation sections before
execution.

Focused CTest coverage is provided by ssa_capability_validation and
ssa_rollback_restricted. The tests exercise a valid closure, escalation,
unknown flags, signature/content failures, idempotent application, fresh
generation rollback, and restricted-section rejection. Availability is
explicit: an unsupported capability or backend is never reported as accepted.
