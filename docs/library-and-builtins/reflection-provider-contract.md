---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_abi_conf.h
  - zr_vm_common/include/zr_vm_common/zr_contract_conf.h
  - zr_vm_core/include/zr_vm_core/global.h
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/module/module_reflection_import.c
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
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_reflection_surface.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_contract_conf.h
  - zr_vm_core/include/zr_vm_core/global.h
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/module/module_reflection_import.c
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
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_reflection_surface.c
plan_sources:
  - user: 2026-08-03 严格执行一次性破坏性语法和 provider identity 切换
  - docs/plans/syntax/2026-07-19-08-reflection-library-type-system-design.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
tests:
  - tests/library/test_official_provider_convergence.c
  - tests/library/test_project_import_resolver.c
  - tests/module/test_module_system.c
  - tests/module/test_reflection_dynamic_generic_instance.c
  - tests/parser/test_reflection_type_surface.c
  - tests/parser/test_type_inference.c
  - tests/acceptance/2026-08-03-syntax-08-m1-reflection-provider-contract.md
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

The 2026-08-03 acceptance replay passed the same 395 Unity tests under WSL GCC
11.4, WSL Clang 14, and MSVC 19.44.

## Scope

This closes Syntax 08 M1 provider/canonical-identity routing. It does not promote
M2 member contracts, M3 artifact/corruption coverage, M4 construction/AOT, or
M5 LSP/stress acceptance. Those gates remain open.
