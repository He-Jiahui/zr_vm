#!/usr/bin/env python3
"""Run the GD-C2 native contract smoke; raw artifacts/logs live only in /tmp."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

DEMO = Path(__file__).resolve().parents[1]
DETECTOR_ERRORS = ("ERROR: AddressSanitizer", "AddressSanitizer:DEADLYSIGNAL", "LeakSanitizer:", "runtime error:")


def run(args):
    cli = Path(args.cli).resolve(strict=True)
    native = Path(args.native_dir).resolve(strict=True)
    out = Path(tempfile.mkdtemp(prefix="zr-swarm-gd-c2-" + args.mode + "-", dir=tempfile.gettempdir()))
    project = out / "project"
    shutil.copytree(DEMO / "native/tests/smoke", project)
    source = project / "src/main.zr"
    source.write_text(source.read_text().replace("@SINK_PATH@", str(out / "sink.jsonl"))
                      .replace("@SNAPSHOT_PATH@", str(out / "null.ppm")))
    manifest = str(project / "smoke.zrp")
    env = os.environ.copy()
    env["ZR_VM_NATIVE_PATH"] = str(native)
    steps = []

    def execute(step, command):
        proc = subprocess.run(command, cwd=project, env=env, capture_output=True, text=True, timeout=30)
        output = proc.stdout + proc.stderr
        (out / (step + ".log")).write_text(output)
        steps.append({"step": step, "command": command, "exit": proc.returncode})
        if any(marker in output for marker in DETECTOR_ERRORS):
            raise ValueError("detector finding")
        if proc.returncode:
            raise ValueError(step + " failed (see raw log)")
        return output

    result = {"schema": "zr-swarm-gd-c2/1", "mode": args.mode, "status": "FAIL", "steps": steps}
    try:
        if args.mode == "binary":
            execute("compile", [str(cli), "--compile", manifest])
            if not (project / "bin/main.zro").is_file():
                raise ValueError("missing binary artifact")
            shutil.rmtree(project / "src")
            result["source_removed"] = True
        output = execute("run", [str(cli), manifest, "--execution-mode", args.mode, "--emit-executed-via"])
        match = re.search(r"GD_C2_OK nowMicros=(\d+) sprites=3 hash=(\d+) modeB=inline rejected=6", output)
        if not match or "executed_via=" + args.mode not in output or int(match[1]) <= 0:
            raise ValueError("wrong smoke result")
        if int(match[2]) != 299488900:
            raise ValueError("null-backend hash mismatch")
        zone_line = next(line.removeprefix("GD_C2_ZONES ") for line in output.splitlines() if line.startswith("GD_C2_ZONES "))
        zones = json.loads(zone_line)["zones"]
        if [(zone["id"], zone["count"]) for zone in zones] != [(2, 2), (3, 1)]:
            raise ValueError("zone aggregation mismatch")
        if any(zone["max_us"] > zone["total_us"] for zone in zones):
            raise ValueError("zone duration mismatch")
        if (out / "sink.jsonl").read_text() != "GD-C2 sink\n" + zone_line + "\n":
            raise ValueError("wrong sink contents")
        if (out / "null.ppm").exists():
            raise ValueError("null backend unexpectedly wrote an image")
        result.update(status="OK", hash=int(match[2]), now_micros=int(match[1]), detector_findings=0,
                      cli_sha256=hashlib.sha256(cli.read_bytes()).hexdigest(),
                      runtime_sha256=hashlib.sha256((native / "libzrgame_runtime.so").read_bytes()).hexdigest())
    except (ValueError, OSError, StopIteration, subprocess.TimeoutExpired) as exc:
        result["error"] = str(exc) if not isinstance(exc, subprocess.TimeoutExpired) else "smoke timeout"
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("GD-C2 " + args.mode + " " + result["status"] + " evidence=" + str(out))
    if result["status"] != "OK":
        print(result["error"])
        return 1
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", required=True)
    parser.add_argument("--native-dir", required=True)
    parser.add_argument("--mode", choices=("interp", "binary"), required=True)
    return run(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
