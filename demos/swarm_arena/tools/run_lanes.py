#!/usr/bin/env python3
"""GD-H1 C host smoke and independent DOD==C gameplay/artifact gate."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time

DEMO = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("swarm_host_checks", DEMO / "tools/run_host.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)


def equivalent(dod, native):
    """Compare gameplay and deterministic metadata; both artifacts are schema checked."""
    dod_fields, dod_summary, dod_rows = dod
    c_fields, c_summary, c_rows = native
    checks.require(dod_fields["lane"] == "dod" and c_fields["lane"] == "c", "incorrect lane marker")
    checks.require(set(dod_fields) == set(c_fields), "success field set mismatch")
    for key in dod_fields.keys() - {"lane"}:
        checks.require(dod_fields[key] == c_fields[key], "DOD/C final state mismatch: " + key)
    for key in ("schema", "git", "build", "scenario", "drive", "render", "submit", "gc_policy",
                "gc_budget_us", "seed", "frames", "warmup", "hash", "status", "entities", "measurement"):
        checks.require(dod_summary[key] == c_summary[key], "DOD/C summary mismatch: " + key)
    checks.require(len(dod_rows) == len(c_rows), "DOD/C frame count mismatch")
    for left, right in zip(dod_rows, c_rows):
        for key in ("frame", "hash", "sprites"):
            checks.require(left[key] == right[key], "DOD/C frame " + str(left["frame"]) + " mismatch: " + key)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--native-dir", required=True)
    parser.add_argument("--check", choices=("smoke", "equivalence"), required=True)
    args = parser.parse_args()
    host = Path(args.host).resolve(strict=True)
    native = Path(args.native_dir).resolve(strict=True)
    out = Path(tempfile.mkdtemp(prefix="zr-swarm-gd-h1-" + args.check + "-"))
    project = out / "project"
    shutil.copytree(DEMO / "game", project, ignore=shutil.ignore_patterns("bin", "test-bin"))
    manifest = project / "swarm_arena.zrp"
    golden = json.loads((DEMO / "goldens/hashes.json").read_text())["smoke_dod_host"]
    env = os.environ.copy()
    env["ZR_VM_NATIVE_PATH"] = str(native)
    steps = []
    result = dict(schema="zr-swarm-gd-h1/1", status="FAIL", check=args.check, steps=steps,
                  host_sha256=hashlib.sha256(host.read_bytes()).hexdigest(),
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
        checks.require(not any(marker in output for marker in checks.DETECTOR_ERRORS), name + " detector finding")
        checks.require(proc.returncode == expected, name + " incorrect exit status")
        if expected:
            checks.require("SWARM_ERROR " in output and "SWARM_OK " not in output, name + " unclear failure")
        return output

    def run(name, lane="c", frames=120, seed=12345, policy="auto", interval=30, expected_hash=None):
        directory = out / name / "artifacts"
        command = [host, "--project", manifest, "--scenario", "smoke", "--lane", lane,
                   "--frames", frames, "--seed", seed, "--render", "null", "--submit", "bulk",
                   "--gc-policy", policy, "--gc-minor-interval", interval, "--out", directory]
        if expected_hash is not None:
            command += ["--expect-hash", expected_hash]
        output = execute(name, command)
        lines = [line for line in output.splitlines() if line.startswith("SWARM_OK ")]
        checks.require(len(lines) == 1, name + " missing or repeated success marker")
        fields = dict(re.findall(r"(\w+)=([^ ]+)", lines[0]))
        summary, rows = checks.artifacts(directory, frames, seed, policy, interval, lane=lane)
        expected = dict(scenario="smoke", lane=lane, mode="host", frames=str(frames), seed=str(seed))
        checks.require(all(fields.get(key) == value for key, value in expected.items()), name + " incorrect run metadata")
        checks.require(summary["status"] == "ok" and summary["hash"] == int(fields["hash"]), name + " artifact hash mismatch")
        if frames == 120 and seed == 12345:
            checks.require(summary["hash"] == golden["hash"], name + " golden hash mismatch")
            for key, value in golden["state"].items():
                checks.require(fields.get(key) == str(value), name + " golden state mismatch: " + key)
            checks.require(rows[59]["hash"] == golden["state"]["hash60"], name + " checkpoint mismatch")
        return fields, summary, rows

    try:
        if args.check == "smoke":
            full = run("golden-c", expected_hash=golden["hash"])
            repeated = run("repeat-c", expected_hash=golden["hash"])
            checks.require(full[0] == repeated[0] and [r["hash"] for r in full[2]] ==
                           [r["hash"] for r in repeated[2]], "C repetition changed gameplay")
            reference = run("light-auto", frames=4)
            for policy in ("slack", "manual"):
                current = run("light-" + policy, frames=4, policy=policy, interval=2,
                              expected_hash=int(reference[0]["hash"]))
                checks.require([r["hash"] for r in current[2]] == [r["hash"] for r in reference[2]],
                               "C GC policy changed frame hashes")
            for lane in ("oop", "hybrid", "pool", "unknown"):
                output = execute("bad-lane-" + lane, [host, "--project", manifest, "--lane", lane], expected=2)
                checks.require("--lane" in output, "unsupported lane diagnostic omits field")
            output = execute("bad-project-c", [host, "--project", out / "missing.zrp", "--lane", "c"], expected=2)
            checks.require("--project" in output, "missing project diagnostic omits field")
            # Reuse a successful output directory: mismatch must replace its status and fail.
            directory = out / "light-auto/artifacts"
            wrong_hash = (int(reference[0]["hash"]) + 1) % 1000000007
            output = execute("hash-mismatch-c", [host, "--project", manifest, "--lane", "c", "--frames", 4,
                             "--expect-hash", wrong_hash, "--out", directory], expected=2)
            checks.require("--expect-hash" in output, "hash mismatch diagnostic omits field")
            mismatch, _ = checks.artifacts(directory, 4, 12345, "auto", lane="c")
            checks.require(mismatch["status"] == "hash_mismatch", "stale success after mismatch")
            blocked = out / "blocked"
            blocked.write_text("file, not directory\n")
            execute("blocked-out-c", [host, "--project", manifest, "--lane", "c", "--frames", 1,
                                      "--out", blocked], expected=2)
            result.update(status="OK", hash=full[1]["hash"], hash60=full[2][59]["hash"])
        else:
            pairs = []
            for name, frames, seed in (("golden", 120, 12345), ("partial", 61, 1), ("boundary-seed", 1, 2147483646)):
                dod = run(name + "-dod", lane="dod", frames=frames, seed=seed)
                native_run = run(name + "-c", frames=frames, seed=seed, expected_hash=dod[1]["hash"])
                equivalent(dod, native_run)
                pairs.append(dict(frames=frames, seed=seed, hash=dod[1]["hash"]))
                if name == "golden":
                    # Prove the comparison rejects corrupt hash/counter evidence independently
                    # of the host's --expect-hash check and golden assertions above.
                    for mutate in (lambda v: v[2][0].update(hash=v[2][0]["hash"] + 1),
                                   lambda v: v[1].update(hash=v[1]["hash"] + 1),
                                   lambda v: v[0].update(kills="-1"),
                                   lambda v: v[1]["entities"].update(enemies_max=0)):
                        invalid = copy.deepcopy(native_run)
                        mutate(invalid)
                        try:
                            equivalent(dod, invalid)
                        except ValueError:
                            pass
                        else:
                            raise ValueError("equivalence gate accepted a negative control")
            result.update(status="OK", hash=golden["hash"], pairs=pairs)
    except (ValueError, OSError, KeyError, TypeError) as exc:
        result["error"] = str(exc)
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("GD-H1 " + args.check + " " + result["status"] + " evidence=" + str(out))
    if result["status"] != "OK":
        print(result["error"])
    return 0 if result["status"] == "OK" else 1


if __name__ == "__main__":
    raise SystemExit(main())
