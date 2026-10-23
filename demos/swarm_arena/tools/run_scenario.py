#!/usr/bin/env python3
"""Gate one GD-C5 DOD scenario through CLI and host with retained artifacts."""
import argparse
from concurrent.futures import ThreadPoolExecutor
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
SCENARIOS = {
    "arena_2k": (2000, 10, 4096),
    "swarm_10k": (10000, 10, 4096),
    "bullet_storm": (1000, 100, 8192),
    "particle_burst": (2000, 10, 16384),
}
spec = importlib.util.spec_from_file_location("swarm_host_checks", DEMO / "tools/run_host.py")
host_checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(host_checks)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", choices=SCENARIOS, required=True)
    parser.add_argument("--frames", type=int, default=0)
    parser.add_argument("--cli", required=True)
    parser.add_argument("--host", required=True)
    parser.add_argument("--native-dir", required=True)
    args = parser.parse_args()
    cli = Path(args.cli).resolve(strict=True)
    host = Path(args.host).resolve(strict=True)
    native = Path(args.native_dir).resolve(strict=True)
    frames = args.frames or 1800
    out = Path(tempfile.mkdtemp(prefix="zr-swarm-gd-c5-" + args.scenario + "-"))
    project = out / "project"
    shutil.copytree(DEMO / "game", project, ignore=shutil.ignore_patterns("bin", "test-bin"))
    manifest = project / "swarm_arena.zrp"
    env = os.environ.copy()
    env["ZR_VM_NATIVE_PATH"] = str(native)
    steps = []
    result = dict(schema="zr-swarm-gd-c5-scenario/1", status="FAIL", scenario=args.scenario,
                  frames=frames, steps=steps,
                  cli_sha256=hashlib.sha256(cli.read_bytes()).hexdigest(),
                  host_sha256=hashlib.sha256(host.read_bytes()).hexdigest())

    def execute(name, command):
        command = list(map(str, command))
        start = time.monotonic()
        try:
            proc = subprocess.run(command, env=env, capture_output=True, text=True, timeout=7200)
        except subprocess.TimeoutExpired as exc:
            (out / (name + ".log")).write_bytes((exc.stdout or b"") + (exc.stderr or b""))
            raise ValueError(name + " timeout") from exc
        output = proc.stdout + proc.stderr
        (out / (name + ".log")).write_text(output)
        steps.append(dict(name=name, command=command, exit=proc.returncode,
                          seconds=round(time.monotonic()-start, 3)))
        host_checks.require(not any(marker in output for marker in host_checks.DETECTOR_ERRORS),
                            name + " functional failure")
        host_checks.require(proc.returncode == 0, name + " incorrect exit status")
        if name == "cli":
            host_checks.require("executed_via=interp" in output, "CLI execution mode is not established")
        lines = [line for line in output.splitlines() if line.startswith("SWARM_OK ")]
        host_checks.require(len(lines) == 1, name + " missing success marker")
        return dict(re.findall(r"(\w+)=([^ ]+)", lines[0]))

    try:
        goldens = json.loads((DEMO / "goldens/hashes.json").read_text())
        suffix = "" if frames == 1800 else "_" + str(frames)
        cli_golden = goldens[args.scenario + "_dod_cli" + suffix]
        host_golden = goldens[args.scenario + "_dod_host" + suffix]
        host_checks.require(cli_golden == host_golden and cli_golden["frames"] == frames,
                            "golden drive/length mismatch")
        common = ["--scenario", args.scenario, "--lane", "dod", "--seed", 12345,
                  "--render", "null", "--submit", "bulk", "--expect-hash", cli_golden["hash"]]
        if args.frames:
            common += ["--frames", frames]
        cli_command = [cli, manifest, "--execution-mode", "interp", "--emit-executed-via", "--", *common]
        host_command = [host, "--project", manifest, *common, "--out", out / "artifacts"]
        if frames == 1800:
            with ThreadPoolExecutor(max_workers=2) as workers:
                cli_future = workers.submit(execute, "cli", cli_command)
                host_future = workers.submit(execute, "host", host_command)
                cli_values, host_values = cli_future.result(), host_future.result()
        else:
            cli_values = execute("cli", cli_command)
            host_values = execute("host", host_command)
        host_checks.require(cli_values.pop("mode") == "cli" and host_values.pop("mode") == "host",
                            "incorrect drive mode")
        simulation = {key: value for key, value in cli_values.items() if key in host_values}
        host_checks.require(simulation == host_values, "CLI/host final state mismatch")
        expected = dict(scenario=args.scenario, lane="dod", frames=str(frames), seed="12345",
                        hash=str(cli_golden["hash"]))
        expected.update({key: str(value) for key, value in cli_golden["state"].items()})
        host_checks.require(all(host_values.get(key) == value for key, value in expected.items()),
                            "golden final state mismatch")
        cap, aimed, particles = SCENARIOS[args.scenario]
        summary, rows = host_checks.artifacts(out / "artifacts", frames, 12345, "auto",
                            scenario=args.scenario, caps=(cap, (aimed+8)*90+64, particles))
        host_checks.require(summary["hash"] == cli_golden["hash"], "artifact hash mismatch")
        if frames >= 60:
            host_checks.require(rows[59]["hash"] == cli_golden["state"]["hash60"], "checkpoint mismatch")
        if frames == 1800:
            host_checks.require(int(host_values["boss_waves"]) == 2 and int(host_values["burst_spawns"]) > 0,
                                "boss waves did not complete")
            host_checks.require(int(host_values["level"]) > 1 and int(host_values["collected"]) > 0,
                                "pickup progression did not run")
            if args.scenario == "particle_burst":
                host_checks.require(summary["entities"]["particles_max"] >= 64, "particle burst did not run")
        result.update(status="OK", hash=summary["hash"], state=simulation)
    except (ValueError, OSError, KeyError, TypeError) as exc:
        result["error"] = str(exc)
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("GD-C5 " + args.scenario + " " + result["status"] + " evidence=" + str(out))
    if result["status"] != "OK":
        print(result["error"])
    return 0 if result["status"] == "OK" else 1


if __name__ == "__main__":
    raise SystemExit(main())
