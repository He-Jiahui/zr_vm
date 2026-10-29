#!/usr/bin/env python3
"""GD-M4 profile staging, trace bytecode goldens and direct/Job spawn parity.

Use python3 -I. The stock loader reads the boolean features map, but does not
select build.profiles. Staging explicitly projects the selected profile into
that map, without changing source or removing trace calls in this tool.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time

DEMO = Path(__file__).resolve().parents[1]
DETECTOR_ERRORS = ("ERROR: AddressSanitizer", "AddressSanitizer:DEADLYSIGNAL",
                   "LeakSanitizer:", "runtime error:")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def stage(destination, profile, tool=None):
    shutil.copytree(DEMO / "game", destination,
                    ignore=shutil.ignore_patterns("bin", "test-bin"))
    manifest_path = destination / "swarm_arena.zrp"
    manifest = json.loads(manifest_path.read_text())
    declared = manifest["build"]["features"]
    enabled = manifest["build"]["profiles"][profile]["enableFeatures"]
    require(len(set(declared)) == len(declared), "duplicate feature declaration")
    require(len(set(enabled)) == len(enabled) and set(enabled) <= set(declared),
            "profile enables duplicate or undeclared features")
    manifest["features"] = {name: name in enabled for name in declared}
    if tool:
        manifest["entry"] = "_m4_" + tool
        shutil.copy2(destination / "tools" / (tool + ".zr"),
                     destination / "src" / (manifest["entry"] + ".zr"))
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest_path


def fields(output, prefix):
    lines = [line for line in output.splitlines() if line.startswith(prefix + " ")]
    require(len(lines) == 1, "missing or repeated " + prefix)
    return dict(re.findall(r"(\w+)=([^ ]+)", lines[0]))


def simulation(output):
    return {key: value for key, value in fields(output, "SWARM_OK").items()
            if not key.startswith("gc_") and key not in ("tick_p95_us", "render_p95_us")}


def check_golden(state, scenario, frames, seed):
    if seed != 12345:
        return
    key = scenario + "_dod_cli" + ("_4" if frames == 4 else "")
    goldens = json.loads((DEMO / "goldens/hashes.json").read_text())
    golden = goldens[key]
    require(golden["frames"] == frames, scenario + " golden frame count mismatch")
    expected = dict(scenario=scenario, lane="dod", mode="cli", frames=str(frames), seed=str(seed),
                    hash=str(golden["hash"]), **{key: str(value) for key, value in golden["state"].items()})
    require(state == expected, scenario + " state differs from retained golden")


class Runner:
    def __init__(self, args, out):
        self.cli = Path(args.cli).resolve(strict=True)
        self.native = Path(args.native_dir).resolve(strict=True)
        self.mode = args.mode
        self.out = out
        self.steps = []
        self.projects = []
        self.env = os.environ.copy()
        self.env["ZR_VM_NATIVE_PATH"] = str(self.native)

    def execute(self, name, command, expected=0):
        command = list(map(str, command))
        started = time.monotonic()
        try:
            proc = subprocess.run(command, env=self.env, capture_output=True, text=True, timeout=600)
            output = proc.stdout + proc.stderr
        except subprocess.TimeoutExpired as exc:
            (self.out / (name + ".log")).write_bytes((exc.stdout or b"") + (exc.stderr or b""))
            raise ValueError(name + " timed out") from exc
        (self.out / (name + ".log")).write_text(output)
        self.steps.append(dict(name=name, command=command, exit=proc.returncode,
                               seconds=round(time.monotonic() - started, 3)))
        require(not any(marker in output for marker in DETECTOR_ERRORS), name + " detector finding")
        require(proc.returncode == expected, name + " exit " + str(proc.returncode))
        return output

    def project(self, name, profile="release", tool=None):
        manifest = stage(self.out / name, profile, tool)
        source = (manifest.parent / "src" / ("_m4_" + tool + ".zr" if tool else "main.zr")).read_text()
        record = dict(name=name, profile=profile, features=json.loads(manifest.read_text())["features"],
                      source_removed=False)
        self.projects.append(record)
        if self.mode == "binary":
            self.execute(name + "-compile", [self.cli, "--compile", manifest])
            require((manifest.parent / "bin" / ("_m4_" + tool + ".zro" if tool else "main.zro")).is_file(),
                    name + " compilation produced no entry artifact")
            for path in manifest.parent.rglob("*.zr"):
                path.unlink()
            require(not list(manifest.parent.rglob("*.zr")), name + " source removal failed")
            record["source_removed"] = True
            record["artifacts"] = {str(path.relative_to(manifest.parent)): hashlib.sha256(path.read_bytes()).hexdigest()
                                   for path in sorted(manifest.parent.rglob("*.zro"))}
        return manifest, source

    def run(self, name, manifest, arguments=(), dump=None, expected=0):
        command = [self.cli, manifest, "--execution-mode", self.mode, "--emit-executed-via"]
        if dump:
            command += ["--dump-bytecode", dump]
        output = self.execute(name, command + ["--", *arguments], expected)
        if expected == 0:
            require("executed_via=" + self.mode in output, name + " execution-mode marker missing")
        return output


def trace_opcodes(dump, source, call):
    matching = [index for index, line in enumerate(source.splitlines(), 1) if line.strip() == call]
    require(len(matching) == 1, "trace call line is ambiguous")
    contents = dump.read_text()
    require(contents.startswith("ZR_DISASSEMBLY "), "invalid or empty bytecode dump")
    rows = re.findall(r"^\s*\d+\s+(\w+)\s+.*; line (\d+)\s*$", contents, re.MULTILINE)
    require(rows, "no disassembly instructions parsed")
    return [opcode for opcode, line in rows if int(line) == matching[0]]


def trace_check(runner):
    observed = {}
    for profile in ("debug", "release"):
        main, source = runner.project(profile + "-game", profile)
        main_dump = runner.out / (profile + "-game.bytecode")
        output = runner.run(profile + "-game-run", main,
                            ["--scenario", "smoke", "--frames", "120", "--seed", "12345"], main_dump)
        state = simulation(output)
        check_golden(state, "smoke", 120, 12345)
        main_ops = trace_opcodes(main_dump, source, "traceStart(cfg.scenario, cfg.frames);")
        logs = [line for line in output.splitlines() if line.startswith("SWARM_TRACE ")]
        require(logs == (["SWARM_TRACE scenario=smoke frames=120"] if profile == "debug" else []),
                profile + " trace logging mismatch")
        probe, source = runner.project(profile + "-arguments", profile, "trace_arguments")
        probe_dump = runner.out / (profile + "-arguments.bytecode")
        output = runner.run(profile + "-arguments-run", probe, dump=probe_dump)
        probe_ops = trace_opcodes(probe_dump, source, "trace(message());")
        expected = "1" if profile == "debug" else "0"
        require(fields(output, "TRACE_OK") == {"arguments": expected, "calls": expected},
                profile + " conditional call or argument evaluation mismatch")
        require(("TRACE_PROBE argument-evaluated" in output) == (profile == "debug"),
                profile + " argument logging mismatch")
        if profile == "release":
            require(not main_ops and not probe_ops, "release retained trace call instructions")
        else:
            require(any("CALL" in op.upper() for op in main_ops) and
                    sum("CALL" in op.upper() for op in probe_ops) >= 2,
                    "debug positive control lacks trace/argument calls")
        observed[profile] = dict(main_trace_opcodes=main_ops, argument_trace_opcodes=probe_ops,
                                 arguments_evaluated=int(expected), trace_calls=int(expected))
    (runner.out / "trace-observed.json").write_text(json.dumps(observed, indent=2) + "\n")
    require(observed == json.loads((DEMO / "goldens/trace.json").read_text()), "trace opcode golden mismatch")
    return dict(dumps=observed, profile_selection="demo staging: build.profiles -> features")


def jobs_check(runner):
    probe, _ = runner.project("semantics", tool="jobs")
    output = runner.run("semantics-run", probe)
    require(fields(output, "JOBS_OK") == dict(scheduled="120", drained="120", boss_waves="1", burst_spawns="10"),
            "ordered requests or queue reuse check failed")
    manifest, _ = runner.project("game")
    pairs = [("smoke", 120, 12345), ("smoke", 61, 1), ("smoke", 1, 2147483646)]
    pairs += [(scenario, 4, 12345) for scenario in ("arena_2k", "swarm_10k", "bullet_storm", "particle_burst")]
    compared = []
    for scenario, frames, seed in pairs:
        states = []
        for mode in ("direct", "jobs"):
            name = scenario + "-" + str(frames) + "-" + str(seed) + "-" + mode
            output = runner.run(name, manifest, ["--scenario", scenario, "--frames", str(frames),
                                                "--seed", str(seed), "--spawn-mode", mode])
            state = simulation(output)
            check_golden(state, scenario, frames, seed)
            states.append(state)
            if mode == "jobs":
                jobs = fields(output, "SWARM_JOBS")
                require(jobs["scheduled"] == str(frames) and jobs["drained"] == str(frames)
                        and int(jobs["spawned"]) > 0, name + " did not drain each director Job")
            else:
                require("SWARM_JOBS " not in output, "direct control used Job path")
        require(states[0] == states[1], name + " direct/Job final hash or state mismatch")
        compared.append(dict(scenario=scenario, frames=frames, seed=seed, state=states[0]))
    output = runner.run("bad-spawn-mode", manifest, ["--spawn-mode", "invalid"], expected=2)
    require("SWARM_ERROR spawn-mode must be direct or jobs" in output and "SWARM_OK " not in output
            and output.count("SWARM_FINALLY cleanup=ok") == 1, "spawn-mode validation failed")
    return dict(pairs=compared, semantics="ordered requests and RNG, single drain, queue reuse and boss burst",
                scheduler="schedule eagerly pumps the local queue; result collected during spawn")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", choices=("trace", "jobs"))
    parser.add_argument("--stage-profile", choices=("debug", "release"))
    parser.add_argument("--cli")
    parser.add_argument("--native-dir")
    parser.add_argument("--mode", choices=("interp", "binary"), default="interp")
    parser.add_argument("--out")
    args = parser.parse_args()
    if args.stage_profile:
        if not args.out or args.check:
            parser.error("--stage-profile requires --out and cannot be combined with --check")
        print(stage(Path(args.out).resolve(), args.stage_profile))
        return 0
    if not args.check or not args.cli or not args.native_dir:
        parser.error("--check, --cli and --native-dir are required")
    out = Path(args.out).resolve() if args.out else Path(tempfile.mkdtemp(prefix="zr-swarm-gd-m4-" + args.check + "-"))
    out.mkdir(parents=True, exist_ok=True)
    result = dict(schema="zr-swarm-gd-m4/1", status="FAIL", check=args.check, mode=args.mode)
    runner = None
    try:
        runner = Runner(args, out)
        result.update(cli_sha256=hashlib.sha256(runner.cli.read_bytes()).hexdigest(),
                      runtime_sha256=hashlib.sha256((runner.native / "libzrgame_runtime.so").read_bytes()).hexdigest())
        result.update(trace_check(runner) if args.check == "trace" else jobs_check(runner))
        result["status"] = "OK"
    except (OSError, ValueError, KeyError, TypeError) as exc:
        result["error"] = str(exc)
    if runner:
        result.update(steps=runner.steps, projects=runner.projects)
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("GD-M4 " + args.check + " " + args.mode + " " + result["status"] + " evidence=" + str(out))
    if "error" in result:
        print(result["error"])
    return 0 if result["status"] == "OK" else 1


if __name__ == "__main__":
    raise SystemExit(main())
