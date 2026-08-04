---
scope: Syntax 08 M4 runtime construction cache AOT and native module
status: proven
date: 2026-08-04
toolchains:
  - WSL GCC 11.4
  - WSL Clang 14.0
  - MSVC 19.44 x64 Debug
---

# Syntax 08 M4 runtime construction acceptance

## Reopened boundary

`ConstructibleType.createInstance` already had a constructor binder, but its
descriptor method expected an implicit frame receiver. That made direct and
spread source calls depend on a receiver layout which could not be shared with
captured native closures or AOT C. Open generic declarations were also not
distinguished from constructible closed declarations, and runtime modules had
no fresh generation to invalidate constructor plans after reload.

## Accepted implementation

- Descriptor methods use one closed, GC-traced descriptor capture and an
  explicit captured-receiver binding mode. Direct and spread arguments begin
  immediately after the callable.
- Each module receives a nonzero process-unique metadata generation. TypeId
  authentication and constructor cache keys inherit it, so an equal module/type
  spelling in a later module instance cannot reuse the old plan.
- Generic class, struct, and interface declarations carry an open-generic
  modifier. Closed materializations clear it; abstract, interface, resource,
  ref-like, erased, and open-generic categories reject construction.
- Ordinary `new`, `init`, and function calls leave the reflection construction
  cache at zero hits and zero misses.
- AOT C avoids a stale scalar local after a same-block dynamic stack copy and
  executes direct plus spread construction from the emitted binary artifact.

## Evidence

The focused matrix passed on GCC, Clang, and MSVC:

| Executable | Assertions | Boundary |
|---|---:|---|
| `zr_vm_reflection_type_surface_test` | 21 | construction matrix, generation invalidation, ordinary-path bypass |
| `zr_vm_reflection_type_stress_test` | 4 | throw cleanup, compact GC, 10,000 cache hits |
| `zr_vm_official_provider_convergence_test` | 9 | unique official identity and phase ownership |
| `zr_vm_aot_c_reflection_construction_shared_library_test` | 1 | VM/AOT direct+spread result equals 42 |

The AOT shared-library body ran under GCC and Clang. MSVC passed the same
registered executable with its explicit non-Unix platform guard. All registered
CTest targets reported zero failures on all three toolchains.

## Review result

The captured descriptor remains rooted while the closure and closed capture are
allocated, and the closure write uses the normal raw-object barrier. Module
generation is atomic and skips zero. The AOT scan is limited to the current
basic block with a half-open instruction interval, so it does not suppress
scalar reuse based on writes from an adjacent block.

Syntax 08 M4 is promoted.
