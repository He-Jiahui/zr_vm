# GD-H2 OOP lane blocker (2026-10-24)

**GD-H2 is blocked and is not accepted.** Work stopped under John's explicit
"Hard unclear blocker → Claude note + stop" instruction. The incomplete OOP
integration was preserved outside the checkout and removed from the product
tree. DOD/C behavior, rules, goldens, CTest registrations and engine sources
are unchanged. This milestone does not add `--lane oop` support.

The design's `Actor` shape fails even in one source module, without imports,
interfaces, inheritance, a renderer or native providers. Both interpreter
execution and bytecode compilation exit 1 with
`Failed to build class member frame layout metadata`, pointing at the
constructor that initializes the two `Vec2` fields:

```zr
struct Vec2 {
    pub var x: float;
    pub var y: float;
    pub @constructor(x: float, y: float) { this.x = x; this.y = y; }
    pub static @add(a: Vec2, b: Vec2): Vec2 { return init Vec2(a.x + b.x, a.y + b.y); }
    pub static @mul(a: Vec2, b: float): Vec2 { return init Vec2(a.x * b, a.y * b); }
}

class Actor {
    pub var pos: Vec2;
    pub var vel: Vec2;
    pub @constructor(x: float, y: float) {
        this.pos = init Vec2(x, y);
        this.vel = init Vec2(0.0, 0.0);
    }
    pub virtual fn update(dt: float): void { this.pos = this.pos + this.vel * dt; }
}
var a = new Actor(2.0, 3.0);
a.vel = init Vec2(60.0, 120.0);
a.update(1.0 / 60.0);
var pos = a.pos;
if (pos.x != 3.0 || pos.y != 5.0) { throw "Actor position"; }
return 0;
```

The fresh Release build uses GCC 14.2.0 and the demo checkout at
`1b8ae408e88ca82423d65e6ce5d4e6c82c8ffa69`. The retained P02 operator and
P03 interface/inheritance/property probes each pass interpreter execution,
compilation and source-removed binary execution with these same binaries.
Those probes do not cover this composition of class fields and script structs.
Their passing status has not been changed or extended to imply OOP readiness.

Separate reduced projects also reproduce two import-boundary diagnostics:
`init module.Vec2(...)` rejects an exported script struct as not value
constructible, and an override of an imported `Actor` reports
`Failed to publish canonical override relation`. A same-module factory makes
standalone imported struct operators work, but the attempted class-field
composition does not validate. These observations are not established causes
of the single-module blocker and are not accepted workarounds.

All new evidence is under `/workspace/zr_vm-review/gd-h2-20261024/`, including:

- `blocker-minimal/`: manifest, exact source, interpreter/compile logs and result JSON.
- `controls.json` and `controls/`: passing P02/P03 control results and logs.
- `binary-identities.json`: CLI, parser, host and native-runtime SHA-256 identities.
- `configure-release.log`, `build-release.log`, `inventory-before.json`: fresh build and original inventory.
- `wip/`: unvalidated object lane sources, selector and integration patch, for review only.
- `initial-status.txt` and `after-wip-isolation-status.txt`: identical pre-existing checkout state.

The Claude handoff is
`/workspace/zr_vm-review/claude-zr-gd-h2-blocker-20261024.md`;
the requested report and Chinese summary are
`/workspace/zr_vm-review/codex-zr-gd-h2-20261024.md` and
`/workspace/zr_vm-review/codex-zr-gd-h2-20261024-zh.md`.

No OOP hash parity, host smoke, ASan or forced-GC acceptance is claimed.
The existing demo/core suites were not rerun for this blocked attempt. No
golden was regenerated, no failing feature was marked as an expected passing
test, and `/workspace/zr_vm-perf` was not modified. Resumption requires Claude
review of the compiler blocker and a separately reviewed resolution under
the design's engine-change rule.
