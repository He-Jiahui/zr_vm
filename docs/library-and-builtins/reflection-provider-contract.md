---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_abi_conf.h
  - zr_vm_common/include/zr_vm_common/zr_contract_conf.h
  - zr_vm_core/include/zr_vm_core/global.h
  - zr_vm_core/include/zr_vm_core/closure.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/module/module.c
  - zr_vm_core/src/zr_vm_core/module/module_reflection_import.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_core/src/zr_vm_core/reflection_descriptor_native.c
  - zr_vm_core/src/zr_vm_core/reflection_module.c
  - zr_vm_core/src/zr_vm_core/reflection_module_cache.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/src/zr_vm_library/builtin_module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_contract_validation.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_official_inventory.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_reflection_contract.c
  - zr_vm_library/src/zr_vm_library/project/project_import_resolver.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_interface.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_reflection_surface.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_stack_copy.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_contract_conf.h
  - zr_vm_core/include/zr_vm_core/closure.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_core/include/zr_vm_core/global.h
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/module/module.c
  - zr_vm_core/src/zr_vm_core/module/module_reflection_import.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_core/src/zr_vm_core/reflection_descriptor_native.c
  - zr_vm_core/src/zr_vm_core/reflection_module.c
  - zr_vm_core/src/zr_vm_core/reflection_module_cache.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/src/zr_vm_library/builtin_module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_contract_validation.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_official_inventory.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_reflection_contract.c
  - zr_vm_library/src/zr_vm_library/project/project_import_resolver.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_interface.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_reflection_surface.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_stack_copy.c
plan_sources:
  - user: 2026-08-03 严格执行一次性破坏性语法和 provider identity 切换
  - user: 2026-08-04 完成 Syntax 08 上层 construction、AOT、LSP 与压力 gate
  - docs/plans/syntax/2026-07-19-08-reflection-library-type-system-design.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/library/test_official_provider_convergence.c
  - tests/library/test_project_import_resolver.c
  - tests/module/test_module_system.c
  - tests/module/test_reflection_dynamic_generic_instance.c
  - tests/parser/test_reflection_type_surface.c
  - tests/parser/test_reflection_type_stress.c
  - tests/parser/test_aot_c_reflection_construction_shared_library.c
  - tests/parser/test_legacy_migration.c
  - tests/language_server/test_lsp_expression_fact_hover.c
  - tests/parser/test_type_inference.c
  - tests/acceptance/2026-08-03-syntax-08-m1-reflection-provider-contract.md
  - tests/acceptance/2026-08-04-syntax-08-m4-runtime-construction.md
  - tests/acceptance/2026-08-04-syntax-08-m5-lsp-migration-stress.md
doc_type: module-detail
---

# Reflection Provider Contract

## Purpose

Syntax 08 M1 requires reflection identity to come from an authenticated official
provider contract. Compiler and runtime code must not infer reflection authority
from a concrete type name or from the text `zr.reflection`.

The native registry now owns that authority. `zr.builtin` publishes the compile
metadata type roles and `zr.reflection` publishes the runtime reflection type
hierarchy. Core and parser consumers query those roles; they do not carry a
second private table.

## Contract Model

`EZrProviderContractRole` identifies the official provider capability. The first
two owners are `BUILTIN_TYPE_SURFACE`, owned by `zr.builtin`, and `REFLECTION`,
owned by `zr.reflection`.

`EZrCanonicalTypeRole` identifies each canonical type independently of its
spelling. A `ZrLibCanonicalTypeRoleDescriptor` binds the role to its canonical
name, parent role, exposed surface flags, and reflection projection category.
The descriptor is immutable registry input.

The native plugin descriptor ABI is version 5 because `ZrLibModuleDescriptor`
now carries provider role and canonical TypeRole storage. A descriptor is
rejected when pointer/count storage is inconsistent, a role belongs to another
provider, a surface flag or projection is invalid, a role/name is duplicated,
the reflection projection set is incomplete or duplicated, a parent role is
absent, or the parent graph contains a cycle.

`zr.reflection` is a contract-only descriptor. It owns identity and compiler
surface data but cannot be materialized by the generic native loader, the
internal module-link resolver, or the low-level materializer. This prevents an
empty descriptor module from competing with the caller-runtime-bound reflection
service module.

## Registration And Lookup

`ZrLibrary_NativeRegistry_Attach` registers both official contracts and installs
the native module loader plus a provider-role to module-name resolver. Registry
attachment composes any host loader/resolver/owner observer already installed;
misses delegate to the host loader, reserved contract-only identities do not,
and teardown restores the original callbacks.

Consumers can find the descriptor owning a provider role, find a canonical type
by role, registered name, or unique non-erased projection kind. The official
inventory is the admission boundary: non-official descriptors cannot claim a
provider role, and official descriptors must declare exactly the frozen role.

## Compiler And Runtime Flow

Compiler state initialization attaches the registry before type inference.
Type literals, `typeid`, `typeof`, reflection category projections, inherited
surface members, and represented `TypeId` fields resolve their names and
capabilities from registered TypeRoles. Non-erased categories select the unique
registered `projectionKind`. The removed parser-local 20-entry table, private
capability enum, and category-to-role switch no longer exist.

Core reflection import, module creation, and cache validation resolve the
reflection module name from `REFLECTION`; they do not compare a literal module
name. Without an installed provider resolver the special reflection import
fails closed and returns no service module.

## Runtime Construction And Cache Generations

Reflection descriptor methods are native closures with one closed, GC-traced
descriptor capture. `Object_InvokeMember` recognizes this captured-receiver
mode and does not append a second frame receiver. The native entry therefore
sees the callable followed immediately by direct or spread construction
arguments, while the descriptor remains live across allocation and compacting
GC.

Each runtime module receives a process-unique, nonzero metadata generation.
TypeId projection copies the owner module generation into the authenticated
identity, so reloading the same module and type spelling produces a distinct
descriptor/cache key. Constructor plans can be reused within one generation
and cannot leak into a replacement module generation.

Open generic class, struct, and interface declarations carry a shared modifier
flag. Open generic structs project as erased and open generic classes remain
non-constructible; materializing a closed generic instance clears the flag.
Abstract, interface, resource, ref-like, erased, and open-generic categories
all fail before constructor invocation.

## AOT Construction Boundary

Member spread inference unwraps the spread expression before inferring its
type. Unknown-arity reflection member calls defer fixed-signature validation to
the runtime constructor binder; ordinary fixed signatures remain strict.

The AOT C scalar stack-copy path also rejects a cached scalar local when the
same basic block previously overwrote that slot from a dynamically typed stack
source. It reads the canonical VM value slot instead. This preserves descriptor
and result values across direct plus spread reflection calls without weakening
scalar-local reuse for ordinary statically typed writes.

## Reserved Official Root

Ordinary source cannot declare a module under `zr` or `zr.*`. Project current
ModuleId derivation rejects that root after path/explicit-name normalization,
while import resolution continues to accept official modules. This prevents a
workspace file such as `zr/reflection.zr` from acquiring an official canonical
type name and borrowing registered reflection capabilities.

## Failure Boundaries

- Missing registry attachment yields no TypeRole lookup.
- Missing provider resolver yields no core reflection service import.
- Contract-only descriptors cannot create or cache ordinary native modules.
- Registry attachment preserves and composes a host native loader.
- A third-party descriptor claiming `REFLECTION` is rejected.
- An official reflection descriptor omitting its provider role is rejected.
- A workspace source claiming `zr.reflection` is rejected before compilation.
- An ordinary workspace prototype is not overwritten with an intrinsic role
  solely because its name matches a canonical descriptor.

## Test Coverage

Provider convergence verifies inventory ownership, registered role/projection
lookup, graph validation, non-materialization, callback composition/restoration,
teardown/reattach, and spoof rejection. Project resolution verifies official-root
declaration rejection without blocking imports. Reflection surface and dynamic
runtime tests verify the registered hierarchy, workspace spoof rejection, and
missing-resolver failure. Type inference and module-system suites protect the
adjacent compiler and ABI behavior.

The M4/M5 replay adds 21 reflection surface tests, 4 stress tests, one VM/AOT C
equivalence test, 9 provider convergence tests, the legacy migration suite, and
8 focused LSP hover/completion assertions. GCC 11.4, Clang 14, and MSVC 19.44
all pass the registered matrix; the AOT shared-library execution path is
directly exercised on both GCC and Clang, while MSVC verifies the guarded
platform boundary.

## Scope

This document now covers the complete promoted Syntax 08 provider and runtime
boundary. Compile-time generated metadata remains owned by Syntax 11, and
global provider inventory convergence remains owned by Syntax 10C.
