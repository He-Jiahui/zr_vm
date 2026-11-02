# GD-H5 software renderer and snapshots

`zr_swarm --render soft` renders on the CPU without a window or display server.
It is always built when the demo is enabled, including with `ZR_SWARM_GL=OFF`.
The framebuffer is fixed at 640×360, top-to-bottom RGBA8, with the existing
1280×720 world view centered on `gfx.camera`. The background is `(16,20,28,255)`.

The existing 20-byte sprite has no shape field. On soft, `rot == 0` (including
negative zero) selects a filled circle; nonzero rotation selects a square,
rotated clockwise in screen coordinates. `size` is diameter/side in world
units. Bulk SoA sprites have rotation zero and therefore render as circles.
GL continues to draw squares for all instances. Submission APIs, batch
hashes and game rules are unchanged.

Rasterization samples pixel centers without antialiasing or filtering. Circle
boundaries are inclusive; square local bounds are `[-size/2, size/2)`.
Geometry uses double precision after float32 instance conversion; the soft
translation unit disables fast math and floating point contraction. Numeric
`0xRRGGBBAA` colors use integer source-over in submission order, with nearest
rounding on division by 255. Alpha stays opaque over the background. Original
5×7 HUD masks match GL. HUD draws last at screen-pixel coordinates, rounded
to nearest (ties toward positive infinity), with 6-pixel character advance
and 8-pixel newline advance. Lowercase uses uppercase; unknown bytes use `?`.

```sh
cmake --build /workspace/cargo-targets/zr-gd-h5/release \
  --target zr_swarm zrgame_soft_tests -j1
/workspace/cargo-targets/zr-gd-h5/release/bin/zr_swarm \
  --project demos/swarm_arena/game/swarm_arena.zrp \
  --lane dod --scenario smoke --frames 120 --render soft \
  --snapshot 60,120 --out /workspace/zr_vm-review/evidence/gd-h5-20261029/example
ctest --test-dir /workspace/cargo-targets/zr-gd-h5/release \
  -R '^demo_soft_snapshot$' -j1 --output-on-failure
```

Use the existing demo CMake configuration with `ZR_VM_BUILD_DEMOS=ON` and
`BUILD_TESTS=ON`. Set `ZR_SWARM_TEST_ARTIFACT_DIR` to the desired evidence root.
Frame lists use one-based completed frames and write `frame-000060.ppm`, etc.,
under `--out`. A `.ppm` or `.png` filename instead captures the final frame.
PNG uses engine-owned `zr_third_party_zr_miniz_static` (`libzr_miniz.a`),
without any new dependency or vendor changes. PPM is P6 RGB8; PNG retains
RGBA8. Both are top-to-bottom. Soft accepts `--headless`; `--interactive`
requires windowed GL.

On soft and GL, `gfx.snapshot("")` returns FNV-1a over the completed RGBA8 image,
masked to 31 bits. A nonempty `.ppm`/`.png` path writes it and returns the same
hash. Readback, unsupported extension and write failures raise `RuntimeError`;
embedded NUL paths are rejected. Host snapshot failures exit 2 without a
successful summary. Null keeps its batch hash and ignores native snapshot
paths. The stock CLI defaults to null; the embedding host selects a backend.

`demo_soft_snapshot` requires byte-exact SHA-256 matches to frozen files and
[images.json](../goldens/images.json); CTest never generates goldens. Checks
include three native fixture repetitions, three smoke runs per C/DOD lane
(frames 60/120), and frame 60 of all four other scenarios in both lanes.
Every frame's simulation hash and sprite count must agree before accepting
image equality. Native checks cover circles, rotation, camera, alpha, clipping,
extreme finite inputs, HUD, frame clearing and reopen. A Python stdlib PNG
decoder checks CRCs, alpha and decoded equality to PPM. Actual VM
`gfx.snapshot` writes and CLI/native write failures are exercised. Candidate
capture uses `tools/run_soft.py --capture` and only writes to the supplied
artifact directory. Updating goldens requires a written reason and review.

The checks establish repeatability and C/DOD agreement with the tested
toolchain; game/libm agreement on every CPU/compiler is not established.
OOP/Hybrid are outside current lane support. W9 null submission modes remain
unchanged. Soft raster cost is recorded separately as `backend_us`.

M1's GL test also renders the soft fixture and smoke and compares
non-background masks. It gates shared rotated-square coverage at IoU ≥99.5%.
Whole-fixture/smoke coverage is descriptive because soft circles and GL
squares differ. GL retains its own tolerance goldens. On 2026-11-01 John's
authorized Codex computer-use substitute passed the windowed check; see
[GD-M1 acceptance](gd-m1.md#acceptance-record). Results, hashes, measured cost
and coverage numbers are in
`/workspace/zr_vm-review/codex-zr-gd-h5-20261029.md`.
