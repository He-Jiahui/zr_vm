# 2026-07-19 AOT 08-S6T / 10-S4Z41 / 11-S6J Bound Provider Generic TypeSpec Identity

## Scope

This cross-stage slice resolves a requester module's generic TypeSpec through an existing
`SZrMetadataTokenBinding` into a different provider module and then uses the provider runtime as the authoritative
owner of the AOT generic type instance. It affects reflection generic-instance routing and runtime metadata-binding
compatibility. It does not add a metadata table, change the zrp format, synthesize a layout, or introduce a global
TypeSpec registry.

## Baseline

- The local generic resolver accepted only a TypeSpec token owned by the supplied runtime.
- The existing TypeSpec binder could record canonical requester/provider TypeSpec identities with different RIDs, but
  `ZrCore_MetadataRuntime_CheckTokenBindingCompatibility()` rejected that binding as a metadata-token mismatch.
- The first integration RED stopped before the production resolver because the hand-built reflection fixture did not
  expose the entity TypeSpec signature range expected from generated metadata. The fixture was corrected; production
  matching rules were not relaxed.

## Implementation

- `ZrCore_Reflection_ResolveBoundGenericTypeInstanceFromProvider()` requires distinct requester/provider modules,
  resolves the requester's existing module metadata binding, and validates ref token/signature/hash identity.
- The resolver validates the provider module signature hash, provider TypeSpec and paired Signature records, and then
  rereads both TypeSpec signature views. Canonical signature bytes must have equal nonzero lengths and compare exactly,
  so stale records or a hash collision fail closed.
- The provider runtime's existing `ZrCore_Reflection_ResolveDynamicGenericTypeInstance()` remains the only route/layout
  resolver. A successful result therefore carries the provider TypeSpec RID, provider signature RID, and provider
  registered layout.
- Runtime binding compatibility now explicitly permits TypeSpec-to-TypeSpec RID remapping only when both paired tokens
  are Signature tokens. Module version, module hash, signature hash, and layout checks remain active.
- Unbound requesters, same-module requests, wrong providers, malformed binding `expected*` identity, invalid token
  tables, and post-binding signature-byte drift clear the output and return false.

## RED / GREEN

- RED 1: the focused reflection executable reported 24 tests / 1 failure because no binding was created from the
  incomplete entity-signature fixture. Adding the real zrp signature-pool view and entity range moved the failure into
  the resolver.
- RED 2: the resolver returned false for a valid RID 1 -> RID 9 binding. The compatibility unit suite then reported
  17 tests / 1 failure with status `ZR_METADATA_RUNTIME_BINDING_STATUS_METADATA_TOKEN_MISMATCH`.
- GREEN 1: the narrow TypeSpec mapping rule produced metadata compatibility 17/0 and reflection generic identity 24/0.
- RED 3: after mutating the provider's first generic argument from int64 to uint64 without updating cached hashes, the
  resolver returned true; the reflection suite reported 24/1.
- GREEN 2: exact requester/provider signature-view comparison rejected the drift and restored the suite to 24/0.

## Test Inventory

- `tests/module/test_reflection_dynamic_generic_instance.c`
- `tests/module/test_reflection_dynamic_generic_cross_module.h`
- `tests/module/test_metadata_runtime_binding_compatibility.c`
- Focused CTest: `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`,
  `reflection_token_resolve`, and `metadata_runtime_binding_compatibility`.
- Shared regressions: GC 66 cases, instruction execution 31 cases, and instruction table 95 cases.
- Negative boundaries: unbound requester, wrong provider module hash, malformed expected TypeSpec RID, output clearing,
  and same-hash signature-byte drift.

## Tooling Evidence

- WSL GCC 11.4 and Clang 14.0 isolated builds:
  `cmake --build /tmp/zr_vm-aot-08-s6b-isolated-{gcc,clang} --target` followed by the seven focused/shared targets.
- WSL focused execution:
  `ctest --test-dir <build> -R '^(reflection_token_resolve|metadata_runtime_binding_compatibility|metadata_runtime_typespec_layout|reflection_dynamic_generic_instance)$' --output-on-failure`.
- Windows MSVC 19.44 isolated build used
  `%TEMP%/zr_vm-aot-08-s6b-msvc-red` through `VsDevCmd.bat`, followed by the same CTest regex with `-C Debug`.
- GCC, Clang, and MSVC build logs were filtered for warning/error diagnostics attributed to all changed implementation
  and focused test files; the filtered result was empty.
- Cross-toolchain direct execution was serialized. A parallel GCC/Clang attempt made both instruction suites contend
  for the same relative `instruction_import_runtime_fixture.zro` path and each reported the binary-import case as the
  only failure (31/1). The test source confirms that shared fixed path; serial final runs after relink passed 31/0 on
  every toolchain.

## Results

- WSL GCC: focused CTest 4/4; GC 66/0; instruction execution 31/0; instruction table 95/0.
- WSL Clang: focused CTest 4/4; GC 66/0; instruction execution 31/0; instruction table 95/0.
- Windows MSVC: focused CTest 4/4; GC 66/0; instruction execution 31/0; instruction table 95/0.
- Changed-file diagnostic scan: no GCC, Clang, or MSVC source warning/error.
- Final accepted results are the serialized post-relink runs; the explained parallel fixture collision is not counted
  as a product failure.

## Acceptance Decision

Accepted as 08-S6T / 10-S4Z41 / 11-S6J. Cross-module generic TypeSpec identity now consumes the existing canonical
binding and preserves provider runtime/token/layout authority with exact signature revalidation. Full 08-S6 and 10-S4
remain open for method-token-to-function resolution and script-level generic reflection. This slice does not claim a
new metadata format or complete full-AOT reflection closure.
