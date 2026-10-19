#!/usr/bin/env python3
"""Isolate a C1 probe, retain raw evidence, and enforce the required-feature gate.

Use python3 -I. --expect-failure is solely for CTest WILL_FAIL: return 1 only
when the exact recorded compilation/runtime failure is reproduced. Readiness remains FAIL;
--gate is never inverted. Unexpected failures, timeouts and sanitizer reports
cannot pass a WILL_FAIL test through this runner.
"""

import argparse
import hashlib
import json
import os
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


def gate(out=None):
    data = json.loads((DEMO / "probes/status.json").read_text())
    blocked = [p["id"] for p in data["probes"] if p["required"]
               and (p["interp"] != "OK" or p["binary"] != "OK"
                    or any(value != "OK" for config in ("asan", "gcstress")
                           for value in p.get(config, {}).values()))]
    if out:
        for case in json.loads((DEMO / "probes/cases.json").read_text()):
            if not case.get("required"):
                continue
            for mode in ("interp", "binary"):
                result_path = Path(out) / (case["name"] + "-" + mode) / "result.json"
                if not result_path.is_file() or json.loads(result_path.read_text())["status"] != "OK":
                    blocked.append(case["name"] + "-" + mode)
        p22 = Path(out) / "p22-result.json"
        if not p22.is_file() or json.loads(p22.read_text())["status"] != "OK":
            blocked.append("P22-current-results")
    if blocked:
        print("GD-C1 BLOCKED: required probes are not OK: " + ", ".join(blocked))
        print(data["reason"])
        print("See demos/swarm_arena/docs/feature-readiness.md")
        return 1
    print("GD-C1 READY: all required probes OK")
    return 0


def parity(out):
    """P22 requires working, source-removed binary execution of P01-P20."""
    failed = []
    checked = []
    for number in range(1, 21):
        probe = "P" + str(number).zfill(2)
        case_name = probe.lower() + "-main"
        results = []
        for mode in ("interp", "binary"):
            path = Path(out) / (case_name + "-" + mode) / "result.json"
            results.append(json.loads(path.read_text()) if path.is_file() else {})
        ok = (all(r.get("status") == "OK" for r in results)
              and results[1].get("source_removed")
              and results[0].get("cli_sha256") == results[1].get("cli_sha256"))
        (checked if ok else failed).append(probe)
    result = {"schema": "zr-swarm-probe/1", "probe": "P22",
              "status": "FAIL" if failed else "OK", "passed": checked, "failed": failed}
    Path(out).mkdir(parents=True, exist_ok=True)
    (Path(out) / "p22-result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("P22 " + result["status"] + ": working interp/binary=" + ",".join(checked))
    if failed:
        print("P22 lacks successful binary parity: " + ",".join(failed))
    return 1 if failed else 0


def run(args):
    cli = Path(args.cli).resolve(strict=True)
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    project = Path(tempfile.mkdtemp(prefix="project-", dir=out))
    matches = list((DEMO / "probes").glob(args.probe.lower() + "_*"))
    if len(matches) != 1:
        raise ValueError("Expected one project for " + args.probe)
    source = matches[0]
    shutil.copytree(source, project, dirs_exist_ok=True)
    manifest = json.loads((source / "probe.zrp").read_text())
    manifest["entry"] = args.variant
    manifest_path = project / "probe.zrp"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    for path in project.rglob("*.zr"):
        content = path.read_text()
        content = content.replace("@FFI_LIBRARY@", str(Path(args.native_dir).resolve() / "libzrgame_ffi.so"))
        path.write_text(content)
    env = os.environ.copy()
    if args.native_dir:
        env["ZR_VM_NATIVE_PATH"] = str(Path(args.native_dir).resolve())
        if args.plugin_search == "project":
            (project / "native").mkdir(exist_ok=True)
            shutil.copy2(Path(args.native_dir) / "zrvm_native_zrgame_probe.so", project / "native")
            env.pop("ZR_VM_NATIVE_PATH", None)
    steps = []

    def execute(name, command):
        start = time.perf_counter_ns()
        try:
            proc = subprocess.run(command, capture_output=True, text=True,
                                  cwd=DEMO.parents[1], timeout=30, env=env)
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
            for package_src in (project / "packages").glob("**/src") if (project / "packages").exists() else ():
                package_src.rename(package_src.with_name("source-disabled"))
            source_removed = True
    code, output = (125, "Binary compilation did not produce the entry .zro")
    if compiled and args.host:
        command = [str(Path(args.host).resolve()), str(manifest_path), args.mode]
        if args.probe == "P08":
            command.append("p08")
        elif args.probe == "P01":
            command.append("p01")
        code, output = execute("run", command)
    elif compiled:
        code, output = execute("run", [str(cli), str(manifest_path),
                              "--execution-mode", args.mode, "--emit-executed-via",
                              "--", "swarm-probe-argument"])
    sanitizer_error = any(s in step["output"] for s in SANITIZER_ERRORS for step in steps)
    success = (code == 0 and "PROBE_OK " + args.probe in output
               and "executed_via=" + args.mode in output and not sanitizer_error)
    if args.require_output:
        success = success and all(output.count(marker) == 1 for marker in args.require_output)
    if args.observe_error:
        success = (compiled and code > 0 and code not in (124, 125)
                   and args.observe_error in output and not sanitizer_error)
    reproduced = (steps[-1]["exit"] > 0 and steps[-1]["exit"] not in (124, 125)
                  and args.expect_failure is not None
                  and args.expect_failure in steps[-1]["output"]
                  and not sanitizer_error)
    result = {"schema": "zr-swarm-probe/1", "probe": args.probe, "variant": args.variant,
              "case": args.case, "plugin_search": args.plugin_search,
              "mode": args.mode, "cli": str(cli),
              "cli_sha256": hashlib.sha256(cli.read_bytes()).hexdigest(),
              "status": "OK" if success else "FAIL", "source_removed": source_removed,
              "expected_failure_reproduced": reproduced, "sanitizer_error": sanitizer_error,
              "steps": steps}
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(args.probe + " " + args.variant + " " + args.mode + ": " + result["status"])
    if args.expect_failure:
        print("Recorded failure reproduced: " + str(reproduced))
        return 1 if reproduced else 0
    return 0 if success else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gate", action="store_true")
    parser.add_argument("--parity", action="store_true")
    parser.add_argument("--case")
    parser.add_argument("--cli")
    parser.add_argument("--out")
    parser.add_argument("--mode", choices=("interp", "binary"))
    parser.add_argument("--probe", default="P01")
    parser.add_argument("--variant", default="main")
    parser.add_argument("--native-dir", default="")
    parser.add_argument("--plugin-search", choices=("env", "project"), default="env")
    parser.add_argument("--expect-failure")
    parser.add_argument("--observe-error", help="Expected checked-arithmetic error, a successful behavior observation")
    parser.add_argument("--require-output", action="append")
    parser.add_argument("--host", help="Embedded host spike executable (P21)")
    parser.add_argument("--host-exe", help="Host executable for cases declaring host=true")
    args = parser.parse_args()
    if args.gate:
        return gate(args.out)
    if args.parity:
        if not args.out:
            parser.error("--parity requires --out")
        return parity(args.out)
    if args.case:
        case = next(c for c in json.loads((DEMO / "probes/cases.json").read_text()) if c["name"] == args.case)
        args.probe = case["probe"]
        args.variant = case.get("variant", "main")
        args.expect_failure = case.get("expect", {}).get(args.mode)
        args.observe_error = case.get("observe_error")
        args.plugin_search = case.get("plugin_search", "env")
        if case.get("host"):
            args.host = args.host_exe
    if not all((args.cli, args.out, args.mode)):
        parser.error("--cli, --out and --mode are required unless --gate is used")
    try:
        return run(args)
    except (OSError, ValueError) as exc:
        print("Probe harness error: " + str(exc))
        return 0 if args.expect_failure else 1


if __name__ == "__main__":
    raise SystemExit(main())
