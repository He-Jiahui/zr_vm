---
scope: Syntax 08 M2 reflection callable and invocation boundary
status: proven
date: 2026-08-04
toolchains:
  - WSL GCC 11.4
  - WSL Clang 14.0
  - MSVC 19.44 x64 Debug
---

# Syntax 08 M2 reflection invocation boundary acceptance

## Reopened defect

Public reflection objects already exposed source/native parameter passing names,
and ExecIR already preserved all seven source forms. The generated AOT
`SZrAotSignatureType`, however, carried only type/ownership shape. Both token
invocation paths could therefore dispatch a by-ref callable through the generic
object argument vector as though it were value-only.

The RED build failed because the focused tests required a missing
`EZrAotParameterPassingMode` and `SZrAotSignatureType.passingMode`. This
confirmed that no public AOT carrier existed before the implementation.

## Implemented contract

- Advanced `ZR_VM_AOT_ABI_VERSION` from 14 to 15.
- Added explicit ABI values for unknown, value, in, ref, ref-readonly,
  scoped-ref, scoped-ref-readonly, and out.
- Appended the passing mode plus reserved expansion bytes to
  `SZrAotSignatureType` without changing existing field offsets.
- Projected every valid ExecIR form with an explicit switch. Missing or invalid
  projection remains unknown.
- Emitted `.passingMode` in every generated C signature row. Return rows and
  legacy/ambiguous parameter rows use unknown rather than value.
- Required an explicit value mode before either reflection invocation API may
  dispatch. Unknown, all six non-value modes, and a corrupt value 255 fail with
  an untouched invoker call count.
- Preserved the counted dispatcher's signature-shape, arity, base-type,
  required-return reset, return-type, and void-return canonicalization guards.

## Focused matrix

| Suite | GCC | Clang | MSVC |
|---|---:|---:|---:|
| reflection method invoke | 6 | 6 | 6 |
| reflection token resolve | 30 | 30 | 30 |
| AOT MethodInfo signature | 12 | 12 | 9 |
| public AOT header contract | 1 | 1 | 1 |
| **Total** | **49** | **49** | **46** |

All executed assertions passed. The three omitted MSVC cases are established
Unix-only ExecIR projection tests guarded by `ZR_PLATFORM_UNIX`; the new
all-mode mapping and generated signature-row checks execute on MSVC.

## Review result

The implementation has one source of parameter truth: typed-local roles project
to ExecIR, generated signatures project from ExecIR, and reflection invocation
consumes the versioned signature. It does not infer by-ref behavior from type
names and does not add a second public reflection descriptor model.

Syntax 08 M2 is promoted. Syntax 08 M3-M5 remain open for their independent
artifact/preservation/corruption, full VM/AOT construction, LSP navigation,
migration, and final stress gates.
