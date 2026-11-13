#!/usr/bin/env python3
"""Measure actual-checkout Swarm runs and verify deterministic frame parity."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess

ROOT = Path(__file__).resolve().parents[2]
DEMO = ROOT / "demos/swarm_arena"
SCENARIOS = ("smoke", "arena_2k", "swarm_10k", "bullet_storm", "particle_burst")
DETECTOR_ERRORS = ("ERROR: AddressSanitizer", "AddressSanitizer:DEADLYSIGNAL",
                   "LeakSanitizer:", "runtime error:")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def case(value):
    name, separator, frames = value.partition(":")
    if name not in SCENARIOS or not separator or not frames.isdigit():
        raise argparse.ArgumentTypeError("case must be SCENARIO:FRAMES")
    frames = int(frames)
    if not 1 <= frames <= (120 if name == "smoke" else 1800):
        raise argparse.ArgumentTypeError("frame count is outside the scenario's range")
    return name, frames


def run(command, env, log, cpu):
    command = list(map(str, command))
    if cpu is not None:
        command = ["taskset", "-c", str(cpu), *command]
    proc = subprocess.run(command, env=env, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True, timeout=1800)
    log.write_text(proc.stdout, encoding="utf-8")
    require(proc.returncode == 0, "command failed; see " + str(log))
    require(not any(marker in proc.stdout for marker in DETECTOR_ERRORS),
            "detector finding; see " + str(log))
    lines = [line for line in proc.stdout.splitlines() if line.startswith("SWARM_OK ")]
    require(len(lines) == 1, "missing or repeated success marker; see " + str(log))
    return dict(re.findall(r"(\w+)=([^ ]+)", lines[0]))


def read_run(directory, fields, scenario, frames, lane):
    summary = json.loads((directory / "summary.json").read_text())
    with (directory / "frames.csv").open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    require(summary["status"] == "ok" and summary["scenario"] == scenario and
            summary["frames"] == frames and summary["lane"] == lane and
            summary["seed"] == 12345 and len(rows) == frames, "artifact metadata mismatch")
    require(summary["hash"] == int(fields["hash"]), "summary/final hash mismatch")
    require([int(row["frame"]) for row in rows] == list(range(frames)), "frame sequence mismatch")
    state = {key: value for key, value in fields.items() if key != "lane"}
    deterministic = dict(state=state, entities=summary["entities"],
                         frames=[[row["frame"], row["hash"], row["sprites"]] for row in rows])
    golden_key = scenario + "_dod_host" + ("" if frames in (120, 1800) else "_" + str(frames))
    goldens = json.loads((DEMO / "goldens/hashes.json").read_text())
    golden = goldens.get(golden_key)
    if golden is not None and golden["frames"] == frames:
        require(summary["hash"] == golden["hash"], "committed golden hash mismatch")
        require(all(fields.get(key) == str(value) for key, value in golden["state"].items()),
                "committed golden state mismatch")
    return summary, deterministic


def write_project(out):
    # Only manifests are generated: all production inputs stay in the checkout.
    manifest = json.loads((DEMO / "game/swarm_arena.zrp").read_text())
    manifest["source"] = str(DEMO / "game/src")
    manifest["binary"] = str(out / "binary")
    manifest["dependencies"]["$swarmcore"]["path"] = str(
        DEMO / "game/packages/swarmcore/swarmcore.zrp")
    path = out / "game.zrp"
    path.write_text(json.dumps(manifest, indent=2) + "\n")
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--case", type=case, action="append")
    parser.add_argument("--repeat", type=int, default=3)
    parser.add_argument("--cpu", type=int)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--check-c-lane", action="store_true")
    parser.add_argument("--max-smoke-tick-us", type=float)
    args = parser.parse_args()
    require(args.repeat > 0, "repeat must be positive")
    host = args.host.resolve(strict=True)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    cases = args.case or [("smoke", 120), ("arena_2k", 60), ("swarm_10k", 8),
                          ("bullet_storm", 60), ("particle_burst", 60)]
    require(len(set(cases)) == len(cases), "duplicate case")
    env = os.environ.copy()
    project = write_project(out)
    source_files = sorted((DEMO / "game").rglob("*.zr"))
    identity = {str(path.relative_to(ROOT)): sha256(path) for path in source_files}
    build = host.parent.parent
    libraries = sorted((build / "lib").glob("*.so")) + sorted(
        (build / "demos/swarm_arena/native").glob("*.so"))
    library_identity = {str(path.relative_to(build)): sha256(path) for path in libraries}
    report = dict(schema="zr-swarm-comparison/1", status="FAIL", cpu=args.cpu,
                  host_sha256=sha256(host), source_sha256=identity,
                  library_sha256=library_identity,
                  project_manifest=dict(path=str(project), sha256=sha256(project)), cases={})
    try:
        baseline = json.loads(args.baseline.read_text()) if args.baseline else None
        if baseline:
            require(baseline["status"] == "OK" and baseline["cpu"] == args.cpu and
                    baseline["host_sha256"] == report["host_sha256"] and
                    baseline["library_sha256"] == library_identity,
                    "baseline requires the same executable, native libraries and CPU")
        for scenario, frames in cases:
            key = scenario + ":" + str(frames)
            samples = []
            reference = None
            for index in range(args.repeat):
                directory = out / (scenario + "-" + str(frames) + "-dod-" + str(index))
                fields = run([host, "--project", project, "--scenario", scenario,
                              "--lane", "dod", "--frames", frames, "--seed", 12345,
                              "--render", "null", "--submit", "bulk", "--out", directory],
                             env, out / (directory.name + ".log"), args.cpu)
                summary, deterministic = read_run(directory, fields, scenario, frames, "dod")
                if reference is None:
                    reference = deterministic
                require(reference == deterministic, key + " repetition changed simulation")
                samples.append({metric: summary[metric] for metric in ("tick_us", "frame_us", "render_us")})
            measured = dict(samples=samples, deterministic=reference,
                            median_tick_mean_us=statistics.median(s["tick_us"]["mean"] for s in samples),
                            median_tick_p95_us=statistics.median(s["tick_us"]["p95"] for s in samples))
            report["cases"][key] = measured
            if baseline:
                previous = baseline["cases"][key]
                require(reference == previous["deterministic"], key + " baseline simulation changed")
                measured["tick_mean_change_percent"] = 100 * (
                    measured["median_tick_mean_us"] / previous["median_tick_mean_us"] - 1)
            if args.check_c_lane:
                directory = out / (scenario + "-" + str(frames) + "-c")
                fields = run([host, "--project", project, "--scenario", scenario,
                              "--lane", "c", "--frames", frames, "--seed", 12345,
                              "--render", "null", "--submit", "bulk", "--out", directory],
                             env, out / (directory.name + ".log"), args.cpu)
                _, native = read_run(directory, fields, scenario, frames, "c")
                require(reference == native, key + " independent C simulation mismatch")
                measured["c_lane_equal"] = True
            if args.max_smoke_tick_us is not None and scenario == "smoke":
                require(measured["median_tick_mean_us"] <= args.max_smoke_tick_us,
                        "smoke exceeds the specified tick budget")
            print(key + " tick mean median=" + str(measured["median_tick_mean_us"]), flush=True)
        require(identity == {str(p.relative_to(ROOT)): sha256(p) for p in source_files},
                "source changed during measurement")
        require(library_identity == {str(p.relative_to(build)): sha256(p) for p in libraries},
                "native libraries changed during measurement")
        report["status"] = "OK"
    except (ValueError, OSError, KeyError, subprocess.TimeoutExpired) as exc:
        report["error"] = str(exc)
    finally:
        (out / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print("SWARM_PERF " + report["status"] + " " + str(out / "results.json"))
    if report["status"] != "OK":
        print(report["error"])
    return 0 if report["status"] == "OK" else 1


if __name__ == "__main__":
    raise SystemExit(main())
