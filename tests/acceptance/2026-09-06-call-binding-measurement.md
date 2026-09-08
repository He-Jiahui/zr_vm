---
related_code:
  - tests/benchmarks/call_binding_measurement.c
  - zr_vm_core/src/zr_vm_core/call_binding_member.c
implementation_files:
  - tests/benchmarks/call_binding_measurement.c
plan_sources:
  - user: 2026-09-06 W2 / Call Binding M1
tests:
  - tests/cmake/zr_vm_call_binding_tests.cmake
doc_type: acceptance-record
---

# Call Binding Measurement

## Conditions

WSL GCC 11.4 Debug, Valgrind/Callgrind 3.18.1, counting mode (no cache or
branch simulation). The same executable compiles each source once, then
executes it five times. Instrumentation covers execution only. This includes
the first execution's cache warmup and GC costs, but excludes compilation and
shutdown. The timing of concurrent Callgrind processes is not used as evidence.

`bound` preserves CallBinding. `cache-only` clears bindings in the test process
for sites whose original cache lookup remains executable. Import bindings
remain in both modes because their lowering has removed the final name load.
No production fallback or runtime option is introduced. The dynamic
`call_chain_polymorphic` fixture has no removable binding (one retained import),
so it is a control workload rather than a static-binding speedup example.

## Command

Build target: `zr_vm_call_binding_measurement` in
`/home/hejiahui/zr-call-binding-gcc`.

```sh
valgrind --tool=callgrind --instr-atstart=no --cache-sim=no --branch-sim=no \
  --callgrind-out-file=/home/hejiahui/zr-call-binding-gcc/call-binding-CASE-MODE.callgrind \
  /home/hejiahui/zr-call-binding-gcc/bin/zr_vm_call_binding_measurement CASE MODE
```

Run each CASE below with MODE `bound` and `cache-only`. The `.callgrind.1`
dump contains the explicitly instrumented interval used below. The log's
collected Ir includes 10 additional harness instructions after that dump. The
root `.callgrind` file is the final interval and is not used for comparison.

## Results

| Case | Bound Ir | Cache-Only Ir | Ir Reduction | Result |
| --- | ---: | ---: | ---: | ---: |
| call_chain_polymorphic | 621,467,422 | 621,467,422 | 0.00% | 47250207 |
| native_member | 169,260,085 | 182,780,457 | 7.40% | 28000 |
| accessor | 567,950,344 | 629,594,219 | 9.79% | 7998000 |

Every pair returns the same result. Native member has one removable binding
and one retained import; accessor has two removable bindings. These results
measure the binding path relative to existing cache behavior within the final
tree. They do not compare historical builds, establish release timings, or
close the release performance gate. No general performance gain is claimed.
