#!/usr/bin/env python3
"""W10 boundary calls; checksums gate every row, raw loop costs are descriptive."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import tempfile
import time

DEMO = Path(__file__).resolve().parents[1]
PATHS = ("zr", "native_module", "native_extern", "dynamic_ffi")
DETECTORS = ("ERROR: AddressSanitizer", "AddressSanitizer:DEADLYSIGNAL",
             "LeakSanitizer:", "runtime error:")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", required=True)
    parser.add_argument("--native-dir", required=True)
    parser.add_argument("--mode", choices=("interp", "binary"), default="interp")
    parser.add_argument("--calls", type=int, default=20000)
    parser.add_argument("--warmup", type=int, default=1000)
    parser.add_argument("--repetitions", type=int, default=3)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    if not 1 <= args.calls <= 1000000 or not 1 <= args.warmup <= 1000000 or not 1 <= args.repetitions <= 10:
        parser.error("calls/warmup must be in 1..1000000; repetitions in 1..10")
    cli = Path(args.cli).resolve(strict=True)
    native = Path(args.native_dir).resolve(strict=True)
    out = args.out or Path(tempfile.mkdtemp(prefix="swarm-w10-"))
    out = out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    project = Path(tempfile.mkdtemp(prefix="project-", dir=out))
    shutil.copytree(DEMO / "probes/w10_boundary", project, dirs_exist_ok=True)
    source = project / "src/main.zr"
    source.write_text(source.read_text().replace("@FFI_LIBRARY@", str(native / "libzrgame_ffi.so"))
                      .replace("@CALLS@", str(args.calls)).replace("@WARMUP@", str(args.warmup)))
    manifest = str(project / "probe.zrp")
    env = os.environ.copy()
    env["ZR_VM_NATIVE_PATH"] = str(native)
    # Pin to the design's CPU 5 when allowed, otherwise the first allowed CPU.
    allowed = sorted(os.sched_getaffinity(0))
    cpu = 5 if 5 in allowed else allowed[0]
    command_prefix = ["taskset", "-c", str(cpu)]
    steps, rows = [], []
    result = {"schema": "zr-swarm-w10/1", "status": "FAIL", "mode": args.mode,
              "calls": args.calls, "warmup_calls": args.warmup, "repetitions": args.repetitions,
              "cpu": cpu, "steps": steps, "rows": rows, "source_removed": False,
              "cli_sha256": hashlib.sha256(cli.read_bytes()).hexdigest(),
              "ffi_sha256": hashlib.sha256((native / "libzrgame_ffi.so").read_bytes()).hexdigest(),
              "host_plugin_sha256": hashlib.sha256((native / "zrvm_native_zrgame_host.so").read_bytes()).hexdigest(),
              "runtime_sha256": hashlib.sha256((native / "libzrgame_runtime.so").read_bytes()).hexdigest(),
              "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
              "measurement": "raw loop ns/call; includes loop, checksum and argument construction; no subtraction",
              "dynamic_setup": "loadLibrary/getSymbol cached outside timing; call uses a fresh guest array"}

    def execute(name, command, expected_exit=0):
        start = time.monotonic()
        proc = subprocess.run(command_prefix + command, env=env, capture_output=True,
                              text=True, timeout=600)
        output = proc.stdout + proc.stderr
        (out / (name + ".log")).write_text(output)
        steps.append({"name": name, "command": command_prefix + command, "exit": proc.returncode,
                      "seconds": round(time.monotonic() - start, 3)})
        if proc.returncode != expected_exit or any(marker in output for marker in DETECTORS):
            raise ValueError(name + " failed; see " + name + ".log")
        return output

    try:
        if args.mode == "binary":
            execute("compile", [str(cli), "--compile", manifest])
            if not (project / "bin/main.zro").is_file():
                raise ValueError("missing binary entry")
            for path in project.rglob("*.zr"):
                path.unlink()
            result["source_removed"] = not list(project.rglob("*.zr"))
        loads = []
        for repeat in range(args.repetitions):
            loads.append(os.getloadavg()[0])
            output = execute("run-" + str(repeat + 1),
                             [str(cli), manifest, "--execution-mode", args.mode, "--emit-executed-via"])
            if "executed_via=" + args.mode not in output:
                raise ValueError("execution mode marker missing")
            found = re.findall(r"^W10_OK op=(\w+) path=([0-3]) calls=(\d+) elapsed_us=(\d+) checksum=([^\s]+)$",
                               output, re.MULTILINE)
            if len(found) != 12 or len({(op, path) for op, path, *_ in found}) != 12:
                raise ValueError("expected twelve distinct verified W10 rows")
            for op, path, calls, elapsed, checksum in found:
                if int(calls) != args.calls or int(elapsed) < 1:
                    raise ValueError("invalid measurement row")
                expected = {"add_i64": args.calls * (args.calls - 1) // 2 + args.calls * 7,
                            "noise2": args.calls * (args.calls - 1) / 8 + args.calls * 2.25,
                            "nop": args.calls}[op]
                if float(checksum) != expected:
                    raise ValueError("checksum mismatch")
                rows.append({"op": op, "path": PATHS[int(path)], "repetition": repeat + 1,
                             "elapsed_us": int(elapsed), "ns_per_call": int(elapsed) * 1000 / args.calls,
                             "checksum": checksum})
        closed_output = execute("closed-library",
                                [str(cli), manifest, "--execution-mode", args.mode, "--", "--closed-library"],
                                expected_exit=1)
        if ("[LoadError] owning library handle is closed" not in closed_output
                or "W10 closed library accepted symbol call" in closed_output):
            raise ValueError("closed-library rejection mismatch")
        result["closed_library_rejected"] = True
        medians = [{"op": op, "path": path,
                    "ns_per_call": statistics.median(row["ns_per_call"] for row in rows
                                                      if row["op"] == op and row["path"] == path)}
                   for op in ("add_i64", "noise2", "nop") for path in PATHS]
        result.update(status="OK", load_avg=loads, regression_eligible=max(loads) <= 2.0, medians=medians)
        lines = ["# W10 boundary calls", "", f"Mode: {args.mode}; CPU: {cpu}; calls: {args.calls}; "
                 f"warmup: {args.warmup}; repetitions: {args.repetitions}.", "",
                 result["measurement"] + ". " + result["dynamic_setup"] + ".", "",
                 "Load averages: " + ", ".join(map(str, loads)) + ". Regression eligible: "
                 + str(result["regression_eligible"]) + ".", "",
                 "| Function | Path | Median ns/call |", "|---|---|---:|"]
        lines += [f"| {row['op']} | {row['path']} | {row['ns_per_call']:.1f} |" for row in medians]
        (out / "w10.md").write_text("\n".join(lines) + "\n")
    except (ValueError, OSError, KeyError, subprocess.TimeoutExpired) as exc:
        result["error"] = str(exc)
        if isinstance(exc, subprocess.TimeoutExpired):
            (out / "timeout.log").write_bytes((exc.stdout or b"") + (exc.stderr or b""))
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("W10 " + result["status"] + " evidence=" + str(out))
    if result["status"] != "OK":
        print(result["error"])
    return 0 if result["status"] == "OK" else 1


if __name__ == "__main__":
    raise SystemExit(main())
