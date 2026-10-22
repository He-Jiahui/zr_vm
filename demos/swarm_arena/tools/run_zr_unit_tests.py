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
    out = Path(tempfile.mkdtemp(prefix="zr-swarm-gd-c3-units-", dir=tempfile.gettempdir()))
    project = out / "project"
    shutil.copytree(DEMO / "game", project, ignore=shutil.ignore_patterns("bin", "test-bin"))
    # testSource is not consumed by the CLI. Keep canonical tests outside src,
    # but stage them under src/tests only in this isolated, disposable project.
    shutil.copytree(project / "tests", project / "src/tests")
    # Discovery visits every source, including modules without tests. Limit the
    # staged graph to the modules these tests use; do not compile the CLI,
    # renderer and metrics again in each fresh test worker.
    needed = {"events.zr", "player.zr", "director.zr", "config.zr", "lanes/dod/world.zr",
              "lanes/dod/grid.zr", "lanes/dod/systems.zr",
              "lanes/dod/bullet_store.zr", "lanes/dod/pickup_store.zr"}
    for path in (project / "src").rglob("*.zr"):
        relative = path.relative_to(project / "src")
        if relative.parts[0] != "tests" and relative.as_posix() not in needed:
            path.unlink()
    manifest_path = project / "swarm_arena.zrp"
    manifest = json.loads(manifest_path.read_text())
    manifest["entry"] = "tests/rng_tests"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    env = os.environ.copy()
    env["ZR_VM_NATIVE_PATH"] = str(Path(args.native_dir).resolve(strict=True))
    command = [str(cli), "test", str(manifest_path),
               "--jobs", "2", "--timeout", "600s"]
    started = time.monotonic()
    result = {"schema": "zr-swarm-gd-c3-units/1", "status": "FAIL", "command": command,
              "tests_staged": True, "cli_sha256": hashlib.sha256(cli.read_bytes()).hexdigest()}
    try:
        proc = subprocess.run(command, env=env, capture_output=True, text=True, timeout=1800)
        output = proc.stdout + proc.stderr
        (out / "unit.log").write_text(output)
        summaries = re.findall(r"test-result: passed=(\d+) failed=(\d+) skipped=(\d+) timedout=(\d+) crashed=(\d+)", output)
        if (proc.returncode or not summaries or summaries[-1] != ("20", "0", "0", "0", "0")
                or any(marker in output for marker in DETECTOR_ERRORS)):
            raise ValueError("unit result mismatch; see unit.log")
        result.update(status="OK", passed=20, failed=0, timedout=0, crashed=0)
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
