# AOT Ownership Call Frame And Resume Acceptance

## Status

- Date: 2026-08-29 (UTC+08:00)
- Focused status: `completed`
- Umbrella status: `validated_pending_full_acceptance`
- Plan: `docs/superpowers/plans/2026-08-10-ownership-object-member-separation-implementation.md`

## Accepted contract

| Requirement | Implementation evidence | Regression evidence |
| --- | --- | --- |
| Static direct calls preserve generated frame state | `PrepareStaticDirectCall` plus `CompletePreparedDirectCallWithResume`; every frame carries `recordHandle` and `codeRegistration` | AOT C call/source/guardrail contracts and known-call pipeline |
| Meta and direct calls resume only in their current caller | `CallPreparedOrGenericWithResume` and resume-instruction dispatch in C/LLVM; an exception unwound beyond the caller remains pending | nested, caught, and tail Weak callable generated-product cases |
| Captured generated callables keep closure values | static-direct materialization allocates the metadata capture count and copies staged captures before installing the thunk | captured Weak callable direct-call and post-drop expiry cases |
| Scalar stack copies use current CFG provenance | complete kind-state transfer, predecessor intersection, and fixed-point backedge convergence | same-kind join, mixed join, parameter, loop, and nonprimitive overwrite cases |
| Distinct dense and physical ownership cells release once | the dense alias is cleared before physical release and reloaded after a potentially relocating destructor | type-layout inline-copy cleanup and re-entrant stack-growth cases |
| Object-member names stay ordinary dispatch | generated C/LLVM execute methods named `share`, `degrade`, `wake`, `intoGc`, and `drop` through receiver access | live/expired optional member and direct Weak callable suites |

## Toolchain evidence

The isolated snapshots contained the exact ownership/AOT source overlay. Each
command below was executed directly; no CTest result was inferred from build
success.

| Target | GCC 11.4 | Clang 14 | MSVC 19.44 |
| --- | ---: | ---: | ---: |
| `zr_vm_type_layout_inline_copy_test` | 40/40 | 40/40 | 40/40 |
| `zr_vm_execbc_aot_pipeline_test` | 98/98 | 98/98 | 98/98 |
| `zr_vm_known_call_pipeline_test` | 5/5 | 5/5 | 5/5 |
| `zr_vm_aot_c_source_contracts_test` | 26/26 | 26/26 | 26/26 |
| `zr_vm_aot_c_guardrail_contracts_test` | 6/6 | 6/6 | 6/6 |
| `zr_vm_aot_c_call_contracts_test` | 9/9 | 9/9 | 9/9 |
| `zr_vm_aot_receiver_guard_shared_library_smoke_test` | 8/8 | 8/8 | 8 ignored |
| `zr_vm_aot_c_call_shared_library_smoke_test` | 5/5 | 5/5 | 5 ignored |

All processes exited zero. GCC and Clang executed both generated C and LLVM
receiver products. MSVC built and linked the complete production/test targets;
the ignored cases are protected by their existing explicit Unix capability
guards.

After review, the generated-source assertion that depended on `zr_aot_fn_6` and
`zr_aot_fn_7` was replaced with a function-content anchor. GCC and Clang reran
the receiver target at 8/8; MSVC rebuilt it and reported the expected 8 Unix
ignores. The closure cleanup test also forces stack growth from a resource Drop
callback and verifies that re-entrant code observes the dense registration as
null before the physical release.

## Boundary and follow-up

This record accepts the focused AOT ownership leaf. It does not claim final
repository acceptance: an independently owned L8 parser/LSP overlay is still
moving, the deterministic migration-inventory golden must be regenerated from
the final tracked baseline, and the complete GCC/Clang/MSVC graph must be
replayed after that integration. `backend_aot_c_scalar_locals.c` also remains a
large module; its coherent follow-up extraction boundary is the complete scalar
kind transfer/fixed-point/query service.

## Cleanup

After the final receiver rerun, the task removed and verified absent the WSL
source snapshot `ownership-aot-review-final-a20b327`, its GCC and Clang build
roots, the matching Windows MSVC source/build roots, and the eight explicit
`ownership-aot-review-final-a20b327-*.tar` transfer archives. It also removed
the generated `System.Collections.Hashtable.Archive` file from the Unity
submodule and restored that submodule to a clean status. Shared `.codex/logs`
and unrelated session artifacts were not touched.
