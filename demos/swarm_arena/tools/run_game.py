#!/usr/bin/env python3
"""GD-C3 smoke/parity and clean CLI failure gates; keep raw evidence in /tmp."""
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
BAD_CASES = [
    (["--scenario", "unknown"], "scenario must be smoke"),
    (["--scenario"], "missing value for --scenario"),
    (["--unknown", "1"], "unknown argument --unknown"),
    (["--frames", "bad"], "--frames must be a decimal integer"),
    (["--seed", "2147483647"], "--seed must be a decimal integer"),
    (["--frames", "1", "--expect-hash", "1"], "hash mismatch: expected 1 got "),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", required=True)
    parser.add_argument("--native-dir", required=True)
    parser.add_argument("--mode", choices=("interp", "binary"), default="interp")
    parser.add_argument("--bad", action="store_true")
    args = parser.parse_args()
    cli = Path(args.cli).resolve(strict=True)
    native = Path(args.native_dir).resolve(strict=True)
    kind = "bad" if args.bad else args.mode
    out = Path(tempfile.mkdtemp(prefix="zr-swarm-gd-c3-game-" + kind + "-", dir=tempfile.gettempdir()))
    env = os.environ.copy()
    env["ZR_VM_NATIVE_PATH"] = str(native)
    steps = []
    result = {"schema": "zr-swarm-gd-c3-game/1", "status": "FAIL", "steps": steps,
              "cli_sha256": hashlib.sha256(cli.read_bytes()).hexdigest(),
              "runtime_sha256": hashlib.sha256((native / "libzrgame_runtime.so").read_bytes()).hexdigest()}

    def execute(name, command):
        started = time.monotonic()
        try:
            proc = subprocess.run(command, env=env, capture_output=True, text=True, timeout=600)
            output = proc.stdout + proc.stderr
        except subprocess.TimeoutExpired as exc:
            (out / (name + ".log")).write_bytes((exc.stdout or b"") + (exc.stderr or b""))
            raise ValueError(name + " timeout") from exc
        (out / (name + ".log")).write_text(output)
        steps.append({"name": name, "command": command, "exit": proc.returncode,
                      "seconds": round(time.monotonic() - started, 3)})
        if any(marker in output for marker in DETECTOR_ERRORS):
            raise ValueError(name + " detector finding")
        return proc.returncode, output

    def project_for(mode):
        project = out / mode
        shutil.copytree(DEMO / "game", project, ignore=shutil.ignore_patterns("bin", "test-bin"))
        manifest = str(project / "swarm_arena.zrp")
        if mode == "binary":
            code, _ = execute("compile", [str(cli), "--compile", manifest])
            if code or not (project / "bin/main.zro").is_file():
                raise ValueError("binary compilation failed")
            # Remove every script, including packages and tests, before loading
            # artifacts in a new CLI process. No source fallback is possible.
            for path in project.rglob("*.zr"):
                path.unlink()
            if list(project.rglob("*.zr")):
                raise ValueError("binary source removal failed")
            result["source_removed"] = True
            result["artifacts"] = sorted(str(p.relative_to(project)) for p in project.rglob("*.zro"))
        return manifest

    try:
        if args.bad:
            for mode in ("interp", "binary"):
                manifest = project_for(mode)
                for index, (arguments, message) in enumerate(BAD_CASES):
                    code, output = execute(mode + "-bad-" + str(index),
                        [str(cli), manifest, "--execution-mode", mode, "--", *arguments])
                    if (code != 2 or "SWARM_ERROR " + message not in output
                            or output.count("SWARM_FINALLY cleanup=ok") != 1 or "SWARM_OK " in output):
                        raise ValueError(mode + " bad case " + str(index) + " did not fail cleanly")
            result.update(status="OK", cases=len(BAD_CASES) * 2)
        else:
            golden = json.loads((DEMO / "goldens/hashes.json").read_text())["smoke_dod_cli"]
            manifest = project_for(args.mode)
            runs = []
            for index in range(3):
                code, output = execute(args.mode + "-" + str(index + 1),
                    [str(cli), manifest, "--execution-mode", args.mode, "--emit-executed-via", "--",
                     "--scenario", "smoke", "--lane", "dod", "--frames", "120", "--seed", "12345",
                     "--render", "null", "--submit", "bulk", "--expect-hash", str(golden["hash"])])
                lines = [line for line in output.splitlines() if line.startswith("SWARM_OK ")]
                if code or len(lines) != 1 or "executed_via=" + args.mode not in output:
                    raise ValueError(args.mode + " smoke execution failed")
                fields = dict(re.findall(r"(\w+)=([^ ]+)", lines[0]))
                expected = dict(scenario="smoke", lane="dod", mode="cli", frames="120", seed="12345")
                expected.update({key: str(value) for key, value in golden["state"].items()})
                expected["hash"] = str(golden["hash"])
                if any(fields.get(key) != value for key, value in expected.items()):
                    raise ValueError(args.mode + " golden state mismatch")
                for key in ("tick_p95_us", "render_p95_us", "gc_minor", "gc_major", "gc_full", "gc_total_us", "gc_max_us"):
                    if int(fields.get(key, "-1")) < 0:
                        raise ValueError("missing/nonmonotonic metric " + key)
                runs.append({key: fields[key] for key in expected})
            result.update(status="OK", mode=args.mode, hash=golden["hash"], runs=runs)
    except (ValueError, OSError, KeyError) as exc:
        result["error"] = str(exc)
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("GD-C3 " + kind + " " + result["status"] + " evidence=" + str(out))
    if result["status"] != "OK":
        print(result["error"])
    return 0 if result["status"] == "OK" else 1


if __name__ == "__main__":
    raise SystemExit(main())
