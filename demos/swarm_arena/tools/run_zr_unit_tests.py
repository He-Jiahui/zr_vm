#!/usr/bin/env python3
"""Stage testSource into a private source tree for the stock zr.testing runner."""
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", required=True)
    parser.add_argument("--native-dir", required=True)
    args = parser.parse_args()
    cli = Path(args.cli).resolve(strict=True)
    out = Path(tempfile.mkdtemp(prefix="zr-swarm-gd-c3-units-", dir="/tmp"))
    project = out / "project"
    shutil.copytree(DEMO / "game", project, ignore=shutil.ignore_patterns("bin", "test-bin"))
    # testSource is not consumed by the CLI. Keep canonical tests outside src,
    # but stage them under src/tests only in this isolated, disposable project.
    shutil.copytree(project / "tests", project / "src/tests")
    env = os.environ.copy()
    env["ZR_VM_NATIVE_PATH"] = str(Path(args.native_dir).resolve(strict=True))
    command = [str(cli), "test", str(project / "swarm_arena.zrp"),
               "--jobs", "2", "--timeout", "30s"]
    started = time.monotonic()
    result = {"schema": "zr-swarm-gd-c3-units/1", "status": "FAIL", "command": command,
              "tests_staged": True, "cli_sha256": hashlib.sha256(cli.read_bytes()).hexdigest()}
    try:
        proc = subprocess.run(command, env=env, capture_output=True, text=True, timeout=120)
        output = proc.stdout + proc.stderr
        (out / "unit.log").write_text(output)
        summaries = re.findall(r"test-result: passed=(\d+) failed=(\d+) skipped=(\d+) timedout=(\d+) crashed=(\d+)", output)
        if (proc.returncode or not summaries or summaries[-1] != ("9", "0", "0", "0", "0")
                or any(marker in output for marker in DETECTOR_ERRORS)):
            raise ValueError("unit result mismatch; see unit.log")
        result.update(status="OK", passed=9, failed=0, timedout=0, crashed=0)
    except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
        if isinstance(exc, subprocess.TimeoutExpired):
            (out / "unit.log").write_bytes((exc.stdout or b"") + (exc.stderr or b""))
        result["error"] = str(exc) if not isinstance(exc, subprocess.TimeoutExpired) else "unit timeout"
    result["seconds"] = round(time.monotonic() - started, 3)
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("GD-C3 units " + result["status"] + " evidence=" + str(out))
    return 0 if result["status"] == "OK" else 1


if __name__ == "__main__":
    raise SystemExit(main())
