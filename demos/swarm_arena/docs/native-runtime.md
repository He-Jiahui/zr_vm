# GD-C2 native runtime and module contract

Linux C11 targets, enabled by `ZR_VM_BUILD_DEMOS=ON`:

| Target | Output / purpose |
| --- | --- |
| `zrgame_runtime` | `native/libzrgame_runtime.so`: clock, zones, sinks, batch and null backend |
| `zrgame_modules` | Static descriptor archive exposing `ZrGame_HostModule()` and `ZrGame_GfxModule()` for `zr_swarm` |
| `zrgame_plugins` | Builds `native/zrvm_native_zrgame_host.so` and `native/zrvm_native_zrgame_gfx.so` |
| `zrgame_ffi` | Existing `native/libzrgame_ffi.so` |
| `zrgame_native_tests` | Standalone C runner; eight tests, registered as `demo_native_runtime` |

`native/` above is `${CMAKE_BINARY_DIR}/demos/swarm_arena/native/`. Set
`ZR_VM_NATIVE_PATH` to that directory when using the stock CLI. The plugins
both depend on the same `libzrgame_runtime.so`, so runtime state is shared.
Each exports `ZrVm_GetNativeModule_v1()` and returns the descriptor also
available for static registration. GD-C4 adds the `zr_swarm` host using those
same descriptors; see [host decisions](gd-c4-decisions.md).

Descriptors use plugin ABI 6, minimum runtime ABI 4, runtime provider phase,
module version `1.0.0`, and contract names `zrgame.host:v1` / `zrgame.gfx:v1`.
Every successful function or constructor writes its result, including null
for void functions, and declares `RESULT_ALWAYS_WRITTEN`.

## `import("zrgame.host")`

| Function | Behavior |
| --- | --- |
| `nowMicros(): int` | Absolute `CLOCK_MONOTONIC` microseconds |
| `zoneBegin(id: int): void` | Push zone ID 0–63; maximum nesting depth 256 |
| `zoneEnd(id: int): void` | Pop the matching top zone and aggregate inclusive elapsed microseconds |
| `zoneReport(): string` | JSON `{"zones":[{"id":2,"count":2,"total_us":10,"max_us":7}]}`, ordered by ID, omitting zones without completed samples |
| `openSink(path: string): int` | Open/truncate a binary metrics file; positive handle on success, 0 on open failure or capacity exhaustion |
| `writeSink(h: int, line: string): void` | Write all string bytes, append one newline, flush |
| `closeSink(h: int): void` | Close a handle; repeated/stale closes are harmless |
| `rssKb(): int` | Current resident pages from `/proc/self/statm`, in KiB; 0 if unavailable |
| `isHostDriven(): bool` | False in stock CLI mode; true while `zr_swarm` owns the VM |

Zone samples are process-wide and cumulative. Recursive uses of the same ID
are counted independently. Mismatched ends leave the stack intact. The C API
provides explicit reset/stats and timestamped begin/end helpers for deterministic
unit tests. The monotonic clock wrappers use those same aggregation functions.

There are 16 sink slots. Handles are never reused, so a closed handle cannot
write to a newly opened file. Callers own successful handles and close them
before shutdown. Sink paths reject embedded NUL bytes. Invalid zone inputs,
sink writes and I/O failures raise a catchable `RuntimeError`.

## `import("zrgame.gfx")`

| Function | Behavior |
| --- | --- |
| `beginFrame(frame: int): void` | Reset sprite/text counts and start a frame |
| `camera(x: float, y: float): void` | Set camera coordinates; persist across frames |
| `sprite(x: float, y: float, size: float, rot: float, rgba: int): void` | Mode A: append one instance |
| `spriteInst(inst: SpriteInstance): void` | Mode B: consume a checked `InlineArgumentView`, then narrow numeric fields into the C instance |
| `submitSoA(xs: Array<float>, ys: Array<float>, sizes: Array<float>, colors: Array<int>, first: int, count: int): void` | Mode C: append a slice of `zr.container.Array` columns with rotation 0 |
| `text(x: float, y: float, msg: string): void` | Copy text into fixed frame storage |
| `endFrame(): int` | Consume the frame with the null backend and return sprite count |
| `snapshot(path: string): int` | Return the last completed frame's stable hash; path is ignored and no file is created |

`SpriteInstance` is a native STRUCT with constructor
`init gfx.SpriteInstance(x: float, y: float, size: float, rot: float, rgba: int)`.
Its VM fields are four float64 values and one int64, matching the verified P20
layout. `spriteInst` checks the 40-byte inline layout and field offsets, reads
the borrowed span synchronously without a safepoint, and copies into the
20-byte batch representation: float32 x/y/size/rot plus uint32 rgba.

Capacity is 65,536 sprites and 128 text records. Each text record holds at most
255 bytes plus a terminator; the original byte length participates in hashing.
Coordinates, size and rotation must be finite and representable as float32;
size is nonnegative and color is an integer in 0–4,294,967,295. Submission and
camera calls require an active frame. Invalid input, overflow and unmatched
`endFrame` raise catchable `RuntimeError`s.

`submitSoA` validates nonnegative ranges against all four column lengths and
remaining batch capacity. It roots the wrappers' `__zr_items` backing arrays
before resolving more fields and normalizes the engine's compact integer
backing with the public `SuperArrayMaterializeGeneric` helper. It then uses
`ZrLib_Array_Length/Get` for scalar reads with no safepoint in the loops. The entire slice is validated before any
append. Invalid slices leave the batch intact; an empty slice at the end is
valid. No array, argument, inline-span or call-context pointer is retained.

The null backend hashes camera coordinates, sprite count and ordered instance
fields, then text count and ordered text coordinates/length/bytes. It uses
FNV-1a with 32-bit unsigned arithmetic, little endian numeric words, canonical
positive zero, and returns the low 31 bits. Frame number and snapshot path do
not affect the content hash. The smoke's one-sprite/HUD golden is `299488900`,
identical across modes A, B and C and across interpreter/binary execution.

The runtime is a singleton for one owner thread. Per-VM worlds, concurrency,
software/GL backends, kernels and images remain later milestones. GD-C3 game
content and the GD-C4 host use this same runtime contract.

## Validation

`demo_native_runtime` checks batch capacity/overflow, reset and invalid values,
the null hash and copied text, text capacity, exact nested/recursive zone
aggregation, zone errors/stack capacity, sink bytes/capacity/stale handles,
the monotonic clock, RSS and drive state. Assertions remain active in Release.

`demo_native_smoke_{interp,binary}` imports both plugins and exercises every
public function, modes A/B/C individually and together, float32 rounding and
rotation, explicit full GC, exact native error messages and error recovery,
atomic rejection of invalid SoA slices, sink bytes and zone counts. The binary
case compiles fresh artifacts and removes copied sources before execution.
The runner creates all projects, outputs, result files and raw logs under
`/tmp/zr-swarm-gd-c2-*` and rejects detector diagnostics.
