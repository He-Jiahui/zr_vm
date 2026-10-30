#!/usr/bin/env python3
"""GD-M5 session smoke parity, instruction termination and 600/720 rewind.

Use an owned TMPDIR and/or --out. Rewind failures remain failures, with raw
binding diagnostics and the partial frame table preserved for investigation.
"""
import argparse
import csv
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
METRICS = ("executedInstructions", "elapsedMicros", "gcMicros", "peakHeapBytes", "nativeCalls")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fields(output):
    lines = [line for line in output.splitlines() if line.startswith("SWARM_OK ")]
    require(len(lines) == 1, "expected exactly one SWARM_OK")
    return dict(re.findall(r"(\w+)=([^ ]+)", lines[0]))


def metrics(directory, frames, replayed=0, success=True):
    summary = json.loads((directory / "session.json").read_text())
    require(summary["schema"] == "zr-swarm-session/1", "session schema mismatch")
    require(summary["status"] == ("OK" if success else "FAIL"), "session status mismatch")
    with (directory / "session-frames.csv").open(newline="") as source:
        rows = [{key: value if key == "pass" else int(value) for key, value in row.items()}
                for row in csv.DictReader(source)]
    require(len(rows) == frames + replayed, "incorrect number of budgeted frame rows")
    require(summary["budgeted_calls"] == len(rows), "call total mismatch")
    require([row["frame"] for row in rows if row["pass"] == "first"] == list(range(1, frames + 1)),
            "first-pass frame sequence mismatch")
    for row in rows:
        require(all(row[key] >= 0 for key in METRICS), "negative call metric")
        require(row["executedInstructions"] <= summary["instruction_budget"], "instruction cap exceeded")
        require(0 <= row["slackMicros"] <= 16667, "slack outside frame allowance")
        require(row["gcStepRequested"] == int(row["slackMicros"] > 0), "GC slack request mismatch")
        if row["gcStepRequested"]:
            require(row["gcStepCrossBoundaryReferences"] == 0, "frame leaked session Value roots")
        if success:
            require(row["success"] == 1 and row["termination"] == 0, "frame did not complete")
            require(row["executedInstructions"] > 0 and row["peakHeapBytes"] > 0, "empty VM usage")
    for key in METRICS:
        expected = max(row[key] for row in rows) if key == "peakHeapBytes" else sum(row[key] for row in rows)
        require(summary[key] == expected, "summary metric mismatch: " + key)
    require(summary["gc_steps"] == sum(row["gcStepRequested"] for row in rows), "GC step total mismatch")
    require(summary["gc_step_pause_micros"] == sum(row["gcStepPauseMicros"] for row in rows),
            "GC pause total mismatch")
    if success:
        require(summary["completed_frames"] == frames and summary["replayed_frames"] == replayed,
                "completed frame totals mismatch")
        require(summary["nativeCalls"] > 0, "native calls were not counted")
    return summary, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--cli", required=True)
    parser.add_argument("--native-dir", required=True)
    parser.add_argument("--check", choices=("smoke", "budget", "rewind"), default="smoke")
    parser.add_argument("--out")
    args = parser.parse_args()
    host = Path(args.host).resolve(strict=True)
    cli = Path(args.cli).resolve(strict=True)
    native = Path(args.native_dir).resolve(strict=True)
    out = Path(args.out).resolve() if args.out else Path(tempfile.mkdtemp(prefix="zr-swarm-gd-m5-"))
    out.mkdir(parents=True, exist_ok=True)
    require(not (out / "result.json").exists(), "use a fresh evidence directory")
    env = os.environ.copy()
    env["ZR_VM_NATIVE_PATH"] = str(native)
    result = dict(schema="zr-swarm-gd-m5/1", status="FAIL", check=args.check, steps=[],
                  host_sha256=hashlib.sha256(host.read_bytes()).hexdigest(),
                  binding_sha256=hashlib.sha256((host.parent.parent / "lib/libzr_vm_rust_binding.so").read_bytes()).hexdigest())

    def execute(name, command, expected=0):
        command = list(map(str, command))
        started = time.monotonic()
        try:
            proc = subprocess.run(command, env=env, capture_output=True, text=True, timeout=1800)
        except subprocess.TimeoutExpired as exc:
            (out / (name + ".log")).write_bytes((exc.stdout or b"") + (exc.stderr or b""))
            raise ValueError(name + " timed out") from exc
        output = proc.stdout + proc.stderr
        (out / (name + ".log")).write_text(output)
        result["steps"].append(dict(name=name, command=command, exit=proc.returncode,
                                    seconds=round(time.monotonic() - started, 3)))
        require(not any(marker in output for marker in ("ERROR: AddressSanitizer", "AddressSanitizer:DEADLYSIGNAL",
                                                        "LeakSanitizer:", "runtime error:")), name + " detector error")
        require(proc.returncode == expected, name + " exit " + str(proc.returncode))
        return output

    try:
        project = out / "project"
        shutil.copytree(DEMO / "game", project, ignore=shutil.ignore_patterns("bin", "test-bin"))
        manifest = project / "swarm_arena.zrp"
        base = [host, "--project", manifest, "--seed", "12345", "--render", "null", "--submit", "bulk"]
        if args.check == "smoke":
            output = execute("session", [*base, "--host-api", "session", "--out", out / "session",
                                          "--expect-hash", "552988638"])
            summary, rows = metrics(out / "session", 120)
            require(summary["gc_steps"] > 0, "no session GcStep was exercised in smoke")
            state = fields(output)
            golden = json.loads((DEMO / "goldens/hashes.json").read_text())["smoke_dod_cli"]
            expected = dict(scenario="smoke", lane="dod", mode="host", frames="120", seed="12345",
                            hash=str(golden["hash"]), **{key: str(value) for key, value in golden["state"].items()})
            require(state == expected and summary["hash"] == golden["hash"], "session smoke golden mismatch")
            legacy = execute("library", [*base, "--out", out / "library", "--expect-hash", str(golden["hash"])])
            require(fields(legacy) == state, "library/session final state differs")
            cli_output = execute("cli", [cli, manifest, "--execution-mode", "interp", "--", "--expect-hash", str(golden["hash"])])
            cli_state = {key: value for key, value in fields(cli_output).items()
                         if not key.startswith("gc_") and key not in ("tick_p95_us", "render_p95_us")}
            require(cli_state == dict(state, mode="cli"), "CLI/session final state differs")
            result.update(hash=summary["hash"], gc_steps=summary["gc_steps"],
                          executedInstructions=summary["executedInstructions"],
                          instruction_min=min(row["executedInstructions"] for row in rows),
                          instruction_max=max(row["executedInstructions"] for row in rows))
        elif args.check == "budget":
            output = execute("budget", [*base, "--host-api", "session", "--frames", "1",
                                         "--instruction-budget", "0", "--out", out / "budget"], expected=2)
            summary, rows = metrics(out / "budget", 1, success=False)
            require(summary["binding_status"] == 10 and summary["failure_operation"] == "frameStep",
                    "budget failure did not report execution termination")
            require(summary["completed_frames"] == 0 and summary["termination"] == 1,
                    "zero instruction budget did not terminate before the first instruction")
            require(rows[0]["executedInstructions"] == 0 and rows[0]["success"] == 0,
                    "terminated usage was not recorded")
            require("SWARM_OK " not in output, "terminated session printed success")
            # Reusing --out must replace success artifacts with the new failure.
            execute("before-failure", [*base, "--host-api", "session", "--frames", "1", "--out", out / "reuse"])
            execute("after-failure", [*base, "--host-api", "session", "--frames", "1",
                                      "--instruction-budget", "1", "--out", out / "reuse"], expected=2)
            reused, terminated = metrics(out / "reuse", 1, success=False)
            require(reused["status"] == "FAIL" and reused["binding_status"] == 10 and
                    terminated[0]["executedInstructions"] == 1 and terminated[0]["termination"] == 1,
                    "nonzero budget did not terminate correctly or left a stale success artifact")
            execute("wrong-api", [*base, "--instruction-budget", "1"], expected=2)
            execute("wrong-lane", [*base, "--host-api", "session", "--lane", "c"], expected=2)
            execute("short-rewind", [*base, "--host-api", "session", "--rewind"], expected=2)
            execute("bad-api", [*base, "--host-api", "unknown"], expected=2)
            result.update(termination=summary["termination"], executedInstructions=summary["executedInstructions"])
        else:
            output = execute("rewind", [*base, "--host-api", "session", "--scenario", "arena_2k",
                                         "--frames", "720", "--rewind", "--out", out / "session"])
            summary, rows = metrics(out / "session", 720, 120)
            rewind = summary["rewind"]
            require(rewind["checkpoint_created"] and rewind["rollback_succeeded"] and rewind["equal"], "rewind was incomplete")
            require(rewind["checkpoint_hash"] == rewind["restored_hash"], "restored checkpoint hash differs")
            require(rewind["first_hash"] == rewind["replay_hash"] == summary["hash"], "rewind full hash differs")
            first = {row["frame"]: row for row in rows if row["pass"] == "first"}
            replay = [row for row in rows if row["pass"] == "replay"]
            require([row["frame"] for row in replay] == list(range(601, 721)), "replay frame sequence mismatch")
            require(all(row["hash"] == first[row["frame"]]["hash"] for row in replay), "replayed frame hash differs")
            require("SWARM_REWIND checkpoint=600 target=720" in output, "missing rewind marker")
            # Independent original host run checks the new wrapper and rewind final state.
            legacy = execute("library", [*base, "--scenario", "arena_2k", "--frames", "720", "--out", out / "library"])
            require(fields(legacy) == fields(output), "library/rewind final state differs")
            result.update(hash=summary["hash"], rewind=rewind, gc_steps=summary["gc_steps"])
        result["status"] = "OK"
    except (OSError, ValueError, KeyError) as exc:
        result["error"] = str(exc)
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("GD_M5_" + result["status"] + " check=" + args.check + " evidence=" + str(out))
    return 0 if result["status"] == "OK" else 1


if __name__ == "__main__":
    raise SystemExit(main())
