#!/usr/bin/env python3
"""GD-M6 evidence probe. AOT findings return 1; no interpreter substitution.

Use python3 -I, an explicit empty --out directory, and an owned TMPDIR.
This is deliberately not a passing CTest when AOT is unavailable.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

DEMO = Path(__file__).resolve().parents[1]
REPO = DEMO.parents[1]
WORKLOADS = (
    ("W1", "Integrate", "ns/entity", "systems.integrate"),
    ("W2", "Seek steering (sqrt)", "ns/entity", "systems.steer (seek)"),
    ("W3", "Grid build", "ns/entity", "Grid.build"),
    ("W4", "Separation", "ns/entity; capacity", "systems.steer (neighbours)"),
    ("W5", "Bullet spawn/despawn (bullet_storm)", "allocations/frame; GC us/frame", "Bullets; systems.moveBullets"),
    ("W6", "Bullet/enemy collisions", "ns/bullet", "systems.collide"),
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def owned_path(value):
    path = Path(value).resolve()
    require(path != Path("/tmp") and Path("/tmp") not in path.parents, "use an owned directory outside /tmp")
    return path


class Runner:
    def __init__(self, args):
        self.out = owned_path(args.out)
        self.out.mkdir(parents=True, exist_ok=False)
        self.cli = Path(args.cli).resolve(strict=True)
        self.host = Path(args.aot_host).resolve(strict=True)
        self.lib = Path(args.lib_dir).resolve(strict=True)
        self.cc = str(Path(args.cc).resolve(strict=True))
        self.env = os.environ.copy()
        self.env["TMPDIR"] = str(owned_path(os.environ["TMPDIR"]))
        require(Path(self.env["TMPDIR"]).is_dir(), "TMPDIR must exist")
        self.env["ZR_VM_NATIVE_PATH"] = str(Path(args.native_dir).resolve(strict=True))
        self.result = dict(schema="zr-swarm-gd-m6-aot/1", status="FINDINGS", steps=[], projects=[], findings=[],
                           cli_sha256=digest(self.cli), aot_host_sha256=digest(self.host),
                           tool_sha256=digest(Path(__file__)), tmpdir=self.env["TMPDIR"],
                           coverage="unavailable", game_hash_parity=None,
                           git=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip(),
                           branch=subprocess.check_output(["git", "branch", "--show-current"], cwd=REPO, text=True).strip(),
                           load_average=list(os.getloadavg()),
                           runtime_sha256={p.name: digest(p) for p in sorted(self.lib.glob("libzr_vm_*.so"))})

    def execute(self, name, command, timeout=180):
        command = list(map(str, command))
        started = time.monotonic()
        try:
            proc = subprocess.run(command, env=self.env, capture_output=True, timeout=timeout)
            output = proc.stdout + proc.stderr
            code = proc.returncode
        except subprocess.TimeoutExpired as exc:
            output = (exc.stdout or b"") + (exc.stderr or b"")
            code = None
        log = self.out / (name + ".log")
        log.write_bytes(output)
        step = dict(name=name, command=command, exit=code, seconds=round(time.monotonic() - started, 3),
                    log=log.name, timeout=code is None)
        self.result["steps"].append(step)
        self.save()
        return code, output.decode(errors="replace")

    def save(self):
        (self.out / "result.json").write_text(json.dumps(self.result, indent=2) + "\n")

    def finding(self, project, stage, detail):
        self.result["findings"].append(dict(project=project, stage=stage, detail=detail))

    def stage(self, name, entry="main", tool=None, control=False):
        project = self.out / name
        if control:
            (project / "src").mkdir(parents=True)
            (project / "src/main.zr").write_text("return 424242;\n")
            manifest = dict(manifestVersion=1, name="gd_m6_scalar_control", version="0.1.0",
                            kind="application", source="src", binary="bin", entry="main")
        else:
            shutil.copytree(DEMO / "game", project, ignore=shutil.ignore_patterns("bin", "test-bin"))
            manifest = json.loads((project / "swarm_arena.zrp").read_text())
            manifest["entry"] = entry
            if tool:
                shutil.copy2(project / "tools" / (tool + ".zr"), project / "src" / (entry + ".zr"))
        path = project / "swarm_arena.zrp"
        path.write_text(json.dumps(manifest, indent=2) + "\n")
        record = dict(name=name, entry=entry, manifest=str(path), libraries=[],
                      sources={str(p.relative_to(project)): digest(p) for p in sorted(project.rglob("*.zr"))})
        self.result["projects"].append(record)
        return project, path, record

    def build_load(self, name, project, record):
        for index, source in enumerate(sorted(project.rglob("*.c"))):
            text = source.read_text()
            match = re.search(r'/\* descriptor.moduleName = (.*?) \*/', text)
            require(match, "generated C has no module identity: " + str(source))
            module = match[1]
            # Runtime loader's documented aot_c/lib/zrvm_aot_<sanitized>.so convention.
            source_root = next(p for p in source.parents if p.name == "aot_c")
            library = source_root / "lib" / ("zrvm_aot_" + re.sub(r"[^A-Za-z0-9_]", "_", module) + ".so")
            library.parent.mkdir(parents=True, exist_ok=True)
            command = [self.cc, "-std=c11", "-O3", "-DNDEBUG", "-DZR_RELEASE", "-DZR_PLATFORM_UNIX",
                       "-fPIC", "-shared", "-fno-fast-math", "-ffp-contract=off"]
            command += ["-I" + str(REPO / part / "include") for part in ("zr_vm_common", "zr_vm_core", "zr_vm_library")]
            command += [source, "-L" + str(self.lib), "-Wl,-rpath," + str(self.lib), "-Wl,--no-undefined",
                        "-lzr_vm_library", "-lzr_vm_core", "-lm", "-ldl", "-pthread", "-o", library]
            code, output = self.execute(name + "-build-" + str(index), command)
            artifact = dict(module=module, source=str(source), source_sha256=digest(source), path=str(library), build_exit=code)
            record["libraries"].append(artifact)
            if code != 0:
                self.finding(name, "build", output[-2000:])
                continue
            artifact["sha256"] = digest(library)
            code, output = self.execute(name + "-load-" + str(index), [self.host, "--inspect", library, module])
            artifact["load_exit"] = code
            if code != 0:
                self.finding(name, "descriptor-load", output[-2000:])

    def probe(self, name, entry="main", tool=None, control=False, arguments=None, functional=False):
        project, path, record = self.stage(name, entry, tool, control)
        checksum = None
        if functional or arguments is not None or control:
            command = [self.cli, path, "--execution-mode", "interp", "--emit-executed-via"]
            if arguments is not None:
                command += ["--", *arguments]
            code, output = self.execute(name + "-interp", command, 600)
            record["interp_exit"] = code
            if code == 0 and "executed_via=interp" in output:
                values = re.findall(r"^([0-9]+)\s*$", output, re.MULTILINE)
                checksum = int(values[-1]) if values else None
                record["interp_checksum"] = checksum
                require(checksum is not None, name + " has no interpreter integer result")
                if name in ("full-game", "game-smoke"):
                    lines = [line for line in output.splitlines() if line.startswith("SWARM_OK ")]
                    require(len(lines) == 1, name + " missing SWARM_OK")
                    fields = dict(re.findall(r"(\w+)=([^ ]+)", lines[0]))
                    golden = json.loads((DEMO / "goldens/hashes.json").read_text())["smoke_dod_cli"]
                    require(fields["hash"] == str(golden["hash"]) and
                            all(fields.get(k) == str(v) for k, v in golden["state"].items()), name + " golden mismatch")
                    record["interp_state"] = fields
            else:
                self.finding(name, "interp-reference", output[-2000:])
        code, output = self.execute(name + "-emit", [self.cli, "--compile", path, "--emit-aot-c"])
        record["emit_exit"] = code
        if code != 0:
            self.finding(name, "emit", output[-2000:])
        self.build_load(name, project, record)
        entry_lib = next((lib for lib in record["libraries"] if lib["module"] == entry and
                          lib["build_exit"] == 0 and lib.get("load_exit") == 0), None)
        record["aot_checksum_parity"] = None
        if entry_lib and checksum is not None and name != "full-game":
            command = [self.host, "--project", path, "--library", entry_lib["path"], "--module", entry,
                       "--checksum", checksum]
            code, output = self.execute(name + "-execute", command, 600)
            record["execute_exit"] = code
            if code == 0 and "status=RAN" in output and "actual=aot_c" in output:
                record["aot_checksum_parity"] = True
            elif "reason=CHECKSUM_MISMATCH" in output:
                record["aot_checksum_parity"] = False
            if record["aot_checksum_parity"] is not True:
                self.finding(name, "execute", output[-2000:])
            elif control:
                require(checksum == 424242, "scalar control interpreter value changed")
                bad = command[:-1] + [checksum + 1]
                bad_code, bad_output = self.execute(name + "-bad-checksum", bad)
                require(bad_code == 1 and "reason=CHECKSUM_MISMATCH" in bad_output, "checksum mismatch accepted")
                missing = command.copy()
                missing[4] = project / "missing.so"
                bad_code, bad_output = self.execute(name + "-missing-library", missing)
                require(bad_code == 1 and "status=UNAVAILABLE" in bad_output and "entry_invoked=0" in bad_output,
                        "missing compiled entry was substituted")
        if name == "game-smoke":
            self.result["game_hash_parity"] = record["aot_checksum_parity"]
        self.save()
        print(name + ": emit=" + str(record["emit_exit"]) + " libraries=" + str(len(record["libraries"])) +
              " parity=" + str(record["aot_checksum_parity"]), flush=True)

    def report_rows(self):
        rows = []
        for ident, workload, unit, source in WORKLOADS:
            evidence = ["lane-systems-emit.log"]
            if ident == "W3":
                evidence = ["game-grid-execute.log"]
            elif ident == "W5":
                evidence += ["game-bullets-execute.log"]
            rows.append(dict(id=ident, workload=workload, backend="aot-c", lane="dod", unit=unit,
                             value=None, status="unavailable", source=source,
                             evidence=evidence,
                             finding="Full DOD AOT emit/run is incomplete; functional subsets are not workload timings."))
        self.result["workloads"] = rows
        # Even a future successful compiler run cannot make unmeasured W rows green.
        if not self.result["findings"]:
            self.finding("W1-W6", "measurement", "This functional probe supplies no validated workload timings.")
        lines = ["# GD-M6 AOT descriptive baseline — 2026-10-29", "",
                 "No AOT workload timing is claimed. See result.json for actual hash parity; null values are unavailable, not zero.", "",
                 "| ID | Workload | Tier | Metric | Value | Outcome |", "|---|---|---|---|---|---|"]
        lines += [f'| {r["id"]} | {r["workload"]} | AOT C / DOD | {r["unit"]} | — | unavailable |' for r in rows]
        lines += ["", "See result.json for per-project emit/build/load/execute results and findings."]
        (self.out / "perf-aot.md").write_text("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("cli", "aot-host", "native-dir", "lib-dir", "out"):
        parser.add_argument("--" + option, required=True)
    parser.add_argument("--cc", default="/usr/bin/gcc")
    args = parser.parse_args()
    runner = Runner(args)
    try:
        runner.probe("scalar-control", control=True)
        runner.probe("full-game", arguments=["--scenario", "smoke", "--frames", "120", "--seed", "12345",
                                                "--expect-hash", "552988638"])
        for source in sorted((DEMO / "game/src/lanes/dod").glob("*.zr")):
            runner.probe("lane-" + source.stem, entry="lanes/dod/" + source.stem)
        for tool in ("smoke", "grid", "bullets"):
            runner.probe("game-" + tool, entry="_m6_" + tool, tool="aot_" + tool, functional=True)
    except (ValueError, OSError, KeyError, StopIteration) as exc:
        runner.finding("harness", "validation", str(exc))
    runner.report_rows()
    runner.result["status"] = "FINDINGS" if runner.result["findings"] else "OK"
    runner.save()
    print("GD-M6 " + runner.result["status"] + " evidence=" + str(runner.out))
    return 1 if runner.result["findings"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
