# GD-C5 full v1 DOD gameplay acceptance

Requested scope is GD-C5 only: extend the retained DOD lane and shared CLI/host
Session for full v1 gameplay and the four named scenarios, using null rendering
and bulk submission. Engine source remains at `e1d3ab19`. Other lanes,
renderers, interactive input, performance harnesses, H/M milestones, runtime
reflection and the v2 package alias remain deferred.

## GD-C4 prerequisite baseline

Before applying gameplay changes, demo `e988a3ee` passes **73/73** in Release
(60.38 s), **73/73** in memory-error-detector (2202.79 s), and **73/73**
in forced-GC (1804.60 s). Each retains native **8/8**, required probes
**38/38**, P22 **18/18**, ten ZR units, smoke CLI/host **224663030**, and
checkpoint 60 **227689432**. Every complete suite passes without a retry.
The unchanged engine Release core passes **515/515** in **68.13 s**. All demo
core inventories remain 515. The retained policy is leak detection disabled.

Commands, timings and private evidence are under
`/tmp/zr-swarm-gd-c5-20261021/`; `results.jsonl` records individual runs.
The [verification record](../../demos/swarm_arena/docs/verification.md) contains
the gate table. Gameplay implementation and final acceptance gates are pending.
