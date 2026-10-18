#!/usr/bin/env python3
"""Isolate a C1 probe, retain raw evidence, and enforce the required-feature gate.

Use python3 -I. --expect-failure is solely for CTest WILL_FAIL: return 1 only
when the exact recorded runtime failure is reproduced. Readiness remains FAIL;
--gate is never inverted. Unexpected failures, timeouts and sanitizer reports
cannot pass a WILL_FAIL test through this runner.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


DEMO = Path(__file__).resolve().parents[1]
SANITIZER_ERRORS = (
    "ERROR: AddressSanitizer", "AddressSanitizer:DEADLYSIGNAL",
    "LeakSanitizer:", "runtime error:",
)


def gate():
    data = json.loads((DEMO / "probes/status.json").read_text())
    blocked = [p["id"] for p in data["probes"] if p["required"]
               and (p["interp"] != "OK" or p["binary"] != "OK")]
    if blocked:
        print("GD-C1 BLOCKED: required probes are not OK: " + ", ".join(blocked))
        print(data["reason"])
        print("See demos/swarm_arena/docs/feature-readiness.md")
        return 1
    print("GD-C1 READY: all required probes OK")
    return 0


def run(args):
    cli = Path(args.cli).resolve(strict=True)
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    project = Path(tempfile.mkdtemp(prefix="project-", dir=out))
    source = DEMO / "probes/p01_struct_array"
    shutil.copytree(source / "src", project / "src")
    manifest = json.loads((source / "probe.zrp").read_text())
    manifest["entry"] = args.variant
    manifest_path = project / "probe.zrp"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    steps = []

    def execute(name, command):
        start = time.perf_counter_ns()
        try:
            proc = subprocess.run(command, capture_output=True, text=True,
                                  cwd=DEMO.parents[1], timeout=30)
            code, output = proc.returncode, proc.stdout + proc.stderr
        except subprocess.TimeoutExpired as exc:
            code, output = 124, str(exc)
        (out / (name + ".log")).write_text(output)
        steps.append({"step": name, "command": command, "exit": code,
                      "elapsed_us": (time.perf_counter_ns() - start) / 1000,
                      "output": output})
        print(output, end="" if output.endswith("\n") else "\n")
        return code, output

    compiled = True
    source_removed = False
    if args.mode == "binary":
        code, _ = execute("compile", [str(cli), "--compile", str(manifest_path)])
        compiled = code == 0 and (project / "bin" / (args.variant + ".zro")).is_file()
        if compiled:
            # Make source fallback impossible, including freshness recompilation.
            (project / "src").rename(project / "source-disabled")
            source_removed = True
    code, output = (125, "Binary compilation did not produce the entry .zro")
    if compiled:
        code, output = execute("run", [str(cli), str(manifest_path),
                              "--execution-mode", args.mode, "--emit-executed-via"])
    sanitizer_error = any(s in step["output"] for s in SANITIZER_ERRORS for step in steps)
    success = (code == 0 and "PROBE_OK P01" in output
               and "executed_via=" + args.mode in output and not sanitizer_error)
    reproduced = (compiled and code > 0 and code not in (124, 125)
                  and args.expect_failure is not None
                  and "Error: " + args.expect_failure in output
                  and not sanitizer_error)
    result = {"schema": "zr-swarm-probe/1", "probe": "P01", "variant": args.variant,
              "mode": args.mode, "cli": str(cli),
              "cli_sha256": hashlib.sha256(cli.read_bytes()).hexdigest(),
              "status": "OK" if success else "FAIL", "source_removed": source_removed,
              "expected_failure_reproduced": reproduced, "sanitizer_error": sanitizer_error,
              "steps": steps}
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("P01 " + args.variant + " " + args.mode + ": " + result["status"])
    if args.expect_failure:
        print("Recorded failure reproduced: " + str(reproduced))
        return 1 if reproduced else 0
    return 0 if success else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gate", action="store_true")
    parser.add_argument("--cli")
    parser.add_argument("--out")
    parser.add_argument("--mode", choices=("interp", "binary"))
    parser.add_argument("--variant", choices=("main", "nested", "control"), default="main")
    parser.add_argument("--expect-failure")
    args = parser.parse_args()
    if args.gate:
        return gate()
    if not all((args.cli, args.out, args.mode)):
        parser.error("--cli, --out and --mode are required unless --gate is used")
    try:
        return run(args)
    except (OSError, ValueError) as exc:
        print("Probe harness error: " + str(exc))
        return 0 if args.expect_failure else 1


if __name__ == "__main__":
    raise SystemExit(main())
