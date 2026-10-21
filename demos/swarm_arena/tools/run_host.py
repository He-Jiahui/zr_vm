#!/usr/bin/env python3
"""GD-C4 host artifacts, policies, failure handling and CLI parity gates."""
import argparse
import copy
import csv
import hashlib
import json
import math
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


def validate_schema(value, schema, root=None, path="summary"):
    """Validate the JSON Schema vocabulary used by perf-summary.schema.json.

    The checked-in schema also works with a full Draft 2020-12 validator.
    This small reader keeps the CTests independent of third-party Python packages.
    """
    root = root or schema
    supported = {"$schema", "title", "$defs", "$ref", "type", "const", "enum",
                 "minimum", "maximum", "required", "properties", "additionalProperties",
                 "minItems", "maxItems", "items"}
    if set(schema) - supported:
        raise ValueError(path + ": unsupported schema keyword")
    if "$ref" in schema:
        target = root
        for key in schema["$ref"].removeprefix("#/").split("/"):
            target = target[key]
        validate_schema(value, target, root, path)
    types = {"object": lambda v: isinstance(v, dict), "array": lambda v: isinstance(v, list),
             "integer": lambda v: type(v) is int, "number": lambda v: type(v) in (int, float) and math.isfinite(v),
             "string": lambda v: isinstance(v, str), "boolean": lambda v: type(v) is bool,
             "null": lambda v: v is None}
    if "type" in schema:
        expected = schema["type"] if isinstance(schema["type"], list) else [schema["type"]]
        if not any(types[kind](value) for kind in expected):
            raise ValueError(path + ": incorrect type")
    if "const" in schema and value != schema["const"]:
        raise ValueError(path + ": incorrect constant")
    if "enum" in schema and value not in schema["enum"]:
        raise ValueError(path + ": outside enum")
    if type(value) in (int, float):
        if not math.isfinite(value) or value < schema.get("minimum", -math.inf) or value > schema.get("maximum", math.inf):
            raise ValueError(path + ": outside numeric bounds")
    if isinstance(value, dict):
        if set(schema.get("required", [])) - set(value):
            raise ValueError(path + ": missing required property")
        properties = schema.get("properties", {})
        additional = schema.get("additionalProperties", True)
        for key, item in value.items():
            if key in properties:
                validate_schema(item, properties[key], root, path + "." + key)
            elif additional is False:
                raise ValueError(path + ": unexpected property " + key)
            elif isinstance(additional, dict):
                validate_schema(item, additional, root, path + "." + key)
    if isinstance(value, list):
        if len(value) < schema.get("minItems", 0) or len(value) > schema.get("maxItems", math.inf):
            raise ValueError(path + ": incorrect array length")
        if "items" in schema:
            for item in value:
                validate_schema(item, schema["items"], root, path + "[]")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def percentile(values, percent):
    return sorted(values)[math.ceil(len(values) * percent / 100) - 1]


def artifacts(directory, frames, seed, policy, interval=30):
    schema = json.loads((DEMO / "docs/perf-summary.schema.json").read_text())
    summary = json.loads((directory / "summary.json").read_text())
    validate_schema(summary, schema)
    for key, value in dict(scenario="smoke", lane="dod", drive="host", exec="interp",
                           render="null", submit="bulk", frames=frames, seed=seed,
                           warmup=0, gc_policy=policy).items():
        require(summary[key] == value, "summary option mismatch: " + key)
    require(summary["alloc"]["exact"] is False and summary["gc"]["p99_pause_us"] is None,
            "best-effort measurements claim exact precision")
    require(summary["zones_us"] == {}, "unmeasured zones must be empty")
    with (directory / "frames.csv").open(newline="") as source:
        reader = csv.DictReader(source)
        rows = [{key: int(value) for key, value in row.items()} for row in reader]
    require(len(rows) == frames and [row["frame"] for row in rows] == list(range(frames)),
            "frames.csv must have exactly one ordered row per frame")
    for row in rows:
        require(all(value >= 0 for key, value in row.items() if key != "gc_debt_bytes"), "negative frame metric")
        require(row["frame_us"] == sum(row[key] for key in ("tick_us", "render_us", "backend_us", "gc_step_us")),
                "frame phases do not add up")
    for key in ("frame_us", "tick_us", "render_us", "backend_us"):
        values = [row[key] for row in rows]
        metric = summary[key]
        for percent in (50, 95, 99):
            require(metric["p" + str(percent)] == percentile(values, percent), key + " percentile mismatch")
        require(metric["max"] == max(values) and abs(metric["mean"] - sum(values) / frames) <= 0.000001,
                key + " aggregate mismatch")
    for name, column in (("minor", "gc_minor"), ("major", "gc_major"), ("full", "gc_full"),
                         ("total_us", "gc_total_us"), ("slack_steps", "gc_steps"), ("manual_requests", "manual_minor_requests")):
        require(summary["gc"][name] == sum(row[column] for row in rows), "GC aggregate mismatch: " + name)
    require(summary["gc"]["max_pause_us"] == max(row["gc_max_pause_us"] for row in rows), "GC maximum mismatch")
    require(summary["gc"]["manual_requests"] == (frames // interval if policy == "manual" else 0),
            "manual minor cadence mismatch")
    require(summary["gc"]["manual_interval_frames"] == interval, "manual interval metadata mismatch")
    require([row["manual_minor_requests"] for row in rows] ==
            [int(policy == "manual" and (frame + 1) % interval == 0) for frame in range(frames)],
            "manual minor requests occur on the wrong frames")
    require(summary["rss_kb_max"] == max(row["rss_kb"] for row in rows), "RSS aggregate mismatch")
    estimates = [row["alloc_bytes_estimate"] for row in rows]
    require(summary["alloc"]["bytes_per_frame_p50"] == percentile(estimates, 50) and
            summary["alloc"]["bytes_per_frame_max"] == max(estimates), "allocation estimate mismatch")
    for threshold in (16667, 8333):
        require(summary["budget_misses"][str(threshold)] == sum(row["frame_us"] > threshold for row in rows),
                "budget miss mismatch")
    for key, cap in (("enemies_max", 200), ("bullets_max", 512), ("particles_max", 256)):
        require(0 <= summary["entities"][key] <= cap, "entity maximum out of bounds")
    # Negative controls prevent a malformed metric from silently passing the schema gate.
    for mutate in (lambda v: v.pop("gc"), lambda v: v["alloc"].update(exact="false"),
                   lambda v: v["tick_us"].update(p50=-1)):
        invalid = copy.deepcopy(summary)
        mutate(invalid)
        try:
            validate_schema(invalid, schema)
        except ValueError:
            pass
        else:
            raise ValueError("schema gate accepted a negative control")
    return summary, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--cli", required=True)
    parser.add_argument("--native-dir", required=True)
    parser.add_argument("--check", choices=("smoke", "parity"), required=True)
    args = parser.parse_args()
    host = Path(args.host).resolve(strict=True)
    cli = Path(args.cli).resolve(strict=True)
    native = Path(args.native_dir).resolve(strict=True)
    out = Path(tempfile.mkdtemp(prefix="zr-swarm-gd-c4-host-" + args.check + "-", dir=tempfile.gettempdir()))
    project = out / "project"
    shutil.copytree(DEMO / "game", project, ignore=shutil.ignore_patterns("bin", "test-bin"))
    manifest = project / "swarm_arena.zrp"
    golden = json.loads((DEMO / "goldens/hashes.json").read_text())["smoke_dod_cli"]
    env = os.environ.copy()
    env["ZR_VM_NATIVE_PATH"] = str(native)
    steps = []
    result = dict(schema="zr-swarm-gd-c4-host/1", status="FAIL", check=args.check, steps=steps,
                  host_sha256=hashlib.sha256(host.read_bytes()).hexdigest(),
                  cli_sha256=hashlib.sha256(cli.read_bytes()).hexdigest(),
                  runtime_sha256=hashlib.sha256((native / "libzrgame_runtime.so").read_bytes()).hexdigest())

    def execute(name, command, expected=0):
        started = time.monotonic()
        try:
            proc = subprocess.run(list(map(str, command)), env=env, capture_output=True, text=True, timeout=600)
            output = proc.stdout + proc.stderr
        except subprocess.TimeoutExpired as exc:
            (out / (name + ".log")).write_bytes((exc.stdout or b"") + (exc.stderr or b""))
            raise ValueError(name + " timed out") from exc
        (out / (name + ".log")).write_text(output)
        steps.append(dict(name=name, command=list(map(str, command)), exit=proc.returncode,
                          seconds=round(time.monotonic() - started, 3)))
        require(not any(marker in output for marker in DETECTOR_ERRORS), name + " detector finding")
        require(proc.returncode == expected, name + " unexpected exit status")
        if expected:
            require("SWARM_ERROR " in output and "SWARM_OK " not in output, name + " did not fail clearly")
        return output

    def fields(output, mode):
        lines = [line for line in output.splitlines() if line.startswith("SWARM_OK ")]
        require(len(lines) == 1, mode + " missing or repeated success marker")
        values = dict(re.findall(r"(\w+)=([^ ]+)", lines[0]))
        require(values.get("mode") == mode, "incorrect drive mode")
        return values

    def host_run(name, frames=120, seed=12345, policy="auto", interval=30, expected_hash=None, budget=500):
        directory = out / name / "artifacts"
        command = [host, "--project", manifest, "--scenario", "smoke", "--lane", "dod", "--render", "null",
                   "--submit", "bulk", "--frames", frames, "--seed", seed, "--gc-policy", policy,
                   "--gc-budget-us", budget, "--gc-minor-interval", interval, "--warmup", 0, "--out", directory]
        if expected_hash is not None:
            command += ["--expect-hash", expected_hash]
        values = fields(execute(name, command), "host")
        summary, rows = artifacts(directory, frames, seed, policy, interval)
        require(summary["status"] == "ok" and summary["hash"] == int(values["hash"]), "artifact/output hash mismatch")
        require(summary["gc_budget_us"] == budget, "GC budget was ignored")
        if frames == 120 and seed == 12345:
            require(summary["hash"] == golden["hash"] and rows[59]["hash"] == golden["state"]["hash60"], "host golden mismatch")
            require(rows[-1]["sprites"] == golden["state"]["sprites"], "host sprite count mismatch")
            for key, value in golden["state"].items():
                require(values.get(key) == str(value), "host state mismatch: " + key)
            for key in ("enemies", "bullets", "particles"):
                require(summary["entities"][key + "_max"] >= int(values[key]), "entity maximum below final count")
        return values, summary, rows

    try:
        if args.check == "smoke":
            _, summary, _ = host_run("golden", expected_hash=golden["hash"])
            reference, _, reference_rows = host_run("light-auto", frames=4)
            for policy in ("slack", "manual"):
                _, _, rows = host_run("light-" + policy, frames=4, policy=policy, interval=2,
                                      expected_hash=int(reference["hash"]))
                require([row["hash"] for row in rows] == [row["hash"] for row in reference_rows], "policy changed frame hashes")
            _, zero, _ = host_run("light-slack-zero", frames=4, policy="slack", budget=0, expected_hash=int(reference["hash"]))
            require(zero["gc"]["slack_steps"] == 0, "zero GC budget performed slack work")
            bad = [["--scenario", "arena_2k"], ["--lane", "oop"], ["--render", "soft"], ["--submit", "percall"],
                   ["--gc-policy", "bad"], ["--frames", "0"], ["--frames", "121"], ["--frames", "bad"],
                   ["--seed", "0"], ["--seed", "2147483647"], ["--gc-budget-us", "-1"],
                   ["--gc-budget-us", "1000001"], ["--warmup", "1"], ["--seed", "1", "--seed", "2"],
                   ["--gc-minor-interval", "0"], ["--gc-minor-interval", "121"],
                   ["--expect-hash", "1000000007"], ["--seed", "999999999999999999999999"],
                   ["--unknown", "1"], ["--frames"]]
            for index, arguments in enumerate(bad):
                output = execute("bad-" + str(index), [host, "--project", manifest, *arguments], expected=2)
                require(arguments[0] in output, "bad option diagnostic omits its field")
            require("--project" in execute("missing-project", [host], expected=2), "missing project diagnostic omits its field")
            require("usage: zr_swarm" in execute("help", [host, "--help"]), "missing help")
            execute("bad-project", [host, "--project", out / "missing.zrp"], expected=2)
            mismatch_dir = out / "mismatch"
            output = execute("hash-mismatch", [host, "--project", manifest, "--frames", 1,
                                              "--expect-hash", 1, "--out", mismatch_dir], expected=2)
            require("--expect-hash" in output, "hash mismatch diagnostic omits its field")
            mismatch, _ = artifacts(mismatch_dir, 1, 12345, "auto")
            require(mismatch["status"] == "hash_mismatch", "hash mismatch artifacts need an explicit status")
            blocked = out / "blocked"
            blocked.write_text("a file cannot be an output directory\n")
            execute("blocked-out", [host, "--project", manifest, "--frames", 1, "--out", blocked], expected=2)
            # Exercise a script exception after boot, including stale artifact removal on --out reuse.
            game_file = project / "src/game.zr"
            game_file.write_text(game_file.read_text().replace("pub fn tick(frame: int): int {",
                'pub fn tick(frame: int): int { throw new exception.RuntimeError("injected host tick failure");', 1))
            failed_dir = out / "light-auto/artifacts"
            output = execute("script-failure", [host, "--project", manifest, "--frames", 1, "--out", failed_dir], expected=2)
            require("game.tick failed" in output and not (failed_dir / "summary.json").exists(),
                    "script failure retained a successful summary")
            result.update(status="OK", hash=summary["hash"], policies=["auto", "slack", "manual"], bad_cases=len(bad) + 5)
        else:
            pairs = []
            for name, frames, seed in (("golden", 120, 12345), ("boundary-seed", 1, 2147483646)):
                arguments = ["--scenario", "smoke", "--lane", "dod", "--frames", frames,
                             "--seed", seed, "--render", "null", "--submit", "bulk"]
                output = execute(name + "-cli", [cli, manifest, "--execution-mode", "interp", "--emit-executed-via", "--", *arguments])
                require("executed_via=interp" in output, "CLI mode is not established")
                cli_values = fields(output, "cli")
                host_values, summary, _ = host_run(name + "-host", frames=frames, seed=seed, expected_hash=int(cli_values["hash"]))
                for key in ("scenario", "lane", "frames", "seed", "hash", *golden["state"].keys()):
                    require(cli_values[key] == host_values[key], "CLI/host state mismatch: " + key)
                pairs.append(dict(frames=frames, seed=seed, hash=summary["hash"]))
            result.update(status="OK", hash=golden["hash"], pairs=pairs)
    except (ValueError, OSError, KeyError, TypeError) as exc:
        result["error"] = str(exc)
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("GD-C4 " + args.check + " " + result["status"] + " evidence=" + str(out))
    if result["status"] != "OK":
        print(result["error"])
    return 0 if result["status"] == "OK" else 1


if __name__ == "__main__":
    raise SystemExit(main())
