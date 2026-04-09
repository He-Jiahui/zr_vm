# Language Benchmark Positioning

## Snapshot
- Source report:
  - `build/codex-wsl-current-gcc-debug-make/tests_generated/performance/comparison_report.md`
- Snapshot time:
  - `2026-04-09T12:01:42Z`
- Comparison subject:
  - `ZR interp`
- Primary meaning of the ratios below:
  - `> 1.0x` means `ZR interp` is slower than the compared language
  - `< 1.0x` means `ZR interp` is faster than the compared language

## Coverage
- `Python`: 14 / 14 cases
- `Node.js`: 14 / 14 cases
- `QuickJS`: 8 / 14 cases
- `Lua`: 8 / 14 cases
- `C#/.NET`: 8 / 14 cases
- `Java`: 14 / 14 cases

## Aggregated Position

| language | coverage | min ratio | max ratio | arithmetic mean | geometric mean | current position |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Lua | 8 | 7.368x | 18.109x | 10.350x | 9.921x | `ZR interp` still far slower |
| QuickJS | 8 | 6.677x | 11.964x | 8.903x | 8.688x | `ZR interp` still far slower |
| Node.js | 14 | 1.104x | 4.186x | 1.741x | 1.576x | `ZR interp` slower, but several cases are close |
| Java | 14 | 0.834x | 3.129x | 1.261x | 1.153x | near parity; mixed wins and losses |
| Python | 14 | 1.734x | 3.651x | 2.215x | 2.164x | `ZR interp` slower across all covered cases |
| C#/.NET | 8 | 0.165x | 0.731x | 0.327x | 0.278x | `ZR interp` is faster on all covered cases |

## Representative Cases

### Node.js
- Best:
  - `sort_array = 1.104x`
- Also close:
  - `string_build = 1.129x`
  - `call_chain_polymorphic = 1.235x`
- Worst:
  - `numeric_loops = 4.186x`
  - `dispatch_loops = 3.872x`

### Python
- Best:
  - `sort_array = 1.734x`
- Next closest:
  - `array_index_dense = 1.804x`
  - `call_chain_polymorphic = 1.855x`
- Worst:
  - `numeric_loops = 3.651x`
  - `dispatch_loops = 3.140x`

### QuickJS
- Best:
  - `sort_array = 6.677x`
- Worst:
  - `numeric_loops = 11.964x`

### Lua
- Best:
  - `sort_array = 7.368x`
- Worst:
  - `numeric_loops = 18.109x`

### Java
- Best:
  - `call_chain_polymorphic = 0.834x`
  - this means `ZR interp` is about `1.20x` faster on this case
- Also faster:
  - `sort_array = 0.848x`
  - `matrix_add_2d = 0.872x`
  - `string_build = 0.884x`
  - `array_index_dense = 0.945x`
- Worst:
  - `dispatch_loops = 3.129x`
  - `numeric_loops = 2.510x`
  - `container_pipeline = 1.216x`

### C#/.NET
- Best:
  - `sort_array = 0.165x`
  - this means `ZR interp` is about `6.06x` faster on this case
- Worst:
  - `dispatch_loops = 0.731x`
  - this still means `ZR interp` is about `1.37x` faster on this case

## Interpretation
- Current stage ordering from fastest to slowest on the covered aggregate picture is roughly:
  - `Lua / QuickJS`
  - `Node.js`
  - `Java`
  - `Python`
  - `ZR interp`
  - `C#/.NET`
- That ordering is only for the currently wired benchmark set and current harness implementations.
- The most realistic short-term target is now:
  - fully surpass `Java` on the remaining losing cases
  - especially `dispatch_loops` and `numeric_loops`
  - then continue closing the gap to `Node.js`

## Workload-Level Read
- `Java` is now the nearest managed-language comparison point on the full 14-case set:
  - `call_chain_polymorphic = 0.834x vs Java`
  - `sort_array = 0.848x vs Java`
  - `matrix_add_2d = 0.872x vs Java`
  - `string_build = 0.884x vs Java`
  - `array_index_dense = 0.945x vs Java`
- The remaining Java losses are concentrated in:
  - `dispatch_loops = 3.129x`
  - `numeric_loops = 2.510x`
  - `container_pipeline = 1.216x`
  - `object_field_hot = 1.134x`
  - `map_object_access = 1.108x`
- Against `Node.js`, the closest families are still:
  - `sort_array = 1.104x`
  - `string_build = 1.129x`
  - `call_chain_polymorphic = 1.235x`
- Against `Python`, the whole set is still behind, but the gap is materially smaller than `Lua / QuickJS`.

## Next Reporting Gap
- The remaining Java reporting gap is no longer wiring or coverage.
- The remaining gap is benchmark quality and performance follow-up:
  - continue shrinking `dispatch/call` overhead where Java still wins clearly
  - continue shrinking `numeric_loops` and `container_pipeline`
  - keep rechecking the Java line after each hot-path optimization slice
