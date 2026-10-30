#!/usr/bin/env python3
"""Isolate session rollback with scalar, script-class and Array<int> state.

The class case reproduces the GD-M5 runtime blocker. A failed replay exits 1;
it is never treated as successful rollback or registered as a passing CTest.
Only the standard-library ctypes module and the built binding are required.
"""
import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
import textwrap

SOURCES = {
    "scalar": """
        var n = 0;
        pub fn tick(): int { n += 1; return n; }
        pub fn fullHash(): int { return n; }
        return 0;
    """,
    "class": """
        pub class Counter {
            pub var n: int = 0;
            pub @constructor() { }
            pub fn step(): int { this.n += 1; return this.n; }
            pub fn read(): int { return this.n; }
        }
        var counter = new Counter();
        pub fn tick(): int { return counter.step(); }
        pub fn fullHash(): int { return counter.read(); }
        return 0;
    """,
    "array": """
        let container = import("zr.container");
        var values = new container.Array<int>();
        values.add(0);
        pub fn tick(): int { values[0] += 1; return values[0]; }
        pub fn fullHash(): int { return values[0]; }
        return 0;
    """,
}


class ErrorInfo(c.Structure):
    _fields_ = [("status", c.c_int), ("message", c.c_char * 512)]


class Binding:
    def __init__(self, library):
        self.library = c.CDLL(str(library))
        pointer = c.c_void_p
        output = c.POINTER(pointer)
        self.new = self.function("Runtime_NewStandard", [pointer, output])
        self.open = self.function("Project_Open", [c.c_char_p, output])
        self.start = self.function("ProjectSession_Start", [pointer, pointer, pointer, output])
        self.call = self.function("ProjectSession_CallModuleExport",
                                  [pointer, c.c_char_p, c.c_char_p, pointer, c.c_size_t, output])
        self.read = self.function("Value_ReadInt", [pointer, c.POINTER(c.c_int64)])
        self.checkpoint = self.function("ProjectSession_Checkpoint", [pointer, output])
        self.rollback = self.function("ProjectSession_Rollback", [pointer, pointer])
        self.error = self.function("GetLastErrorInfo", [c.POINTER(ErrorInfo)], result=None)
        self.free_value = self.function("Value_Free", [pointer])
        self.free_checkpoint = self.function("ProjectSessionCheckpoint_Free", [pointer])
        self.free_session = self.function("ProjectSession_Free", [pointer])
        self.free_workspace = self.function("ProjectWorkspace_Free", [pointer])
        self.free_runtime = self.function("Runtime_Free", [pointer])

    def function(self, name, arguments, result=c.c_int):
        function = getattr(self.library, "ZrRustBinding_" + name)
        function.argtypes = arguments
        function.restype = result
        return function


def probe(binding, out, name, source):
    directory = out / name
    (directory / "src").mkdir(parents=True)
    (directory / "src/main.zr").write_text(textwrap.dedent(source).lstrip())
    manifest = directory / "probe.zrp"
    manifest.write_text(json.dumps(dict(manifestVersion=1, name="session_probe_" + name,
        version="0.1.0", kind="application", source="src", binary="bin", entry="main"), indent=2) + "\n")
    runtime, workspace, session, checkpoint = (c.c_void_p() for _ in range(4))
    result = dict(case=name, status="FAIL", steps=[], equal=False)

    def check(operation, status):
        record = dict(operation=operation, status=status)
        if status:
            error = ErrorInfo()
            binding.error(c.byref(error))
            record["error"] = error.message.decode(errors="replace")
        result["steps"].append(record)
        if status:
            raise ValueError(operation)

    def export(name):
        value, number = c.c_void_p(), c.c_int64()
        try:
            status = binding.call(session, b"main", name.encode(), None, 0, c.byref(value))
            if status:
                check(name, status)
            status = binding.read(value, c.byref(number))
            if status:
                check(name + ".readInt", status)
            return number.value
        finally:
            binding.free_value(value)

    try:
        check("Runtime_NewStandard", binding.new(None, c.byref(runtime)))
        check("Project_Open", binding.open(str(manifest).encode(), c.byref(workspace)))
        check("Start", binding.start(runtime, workspace, None, c.byref(session)))
        for frame in range(600):
            if export("tick") != frame + 1:
                raise ValueError("first-pass state mismatch")
        result["hash600"] = export("fullHash")
        check("Checkpoint", binding.checkpoint(session, c.byref(checkpoint)))
        for frame in range(600, 720):
            if export("tick") != frame + 1:
                raise ValueError("first-pass state mismatch")
        result["hash720"] = export("fullHash")
        check("Rollback", binding.rollback(session, checkpoint))
        result["restored600"] = export("fullHash")
        for frame in range(600, 720):
            if export("tick") != frame + 1:
                raise ValueError("replay state mismatch")
        result["replay720"] = export("fullHash")
        result["equal"] = (result["hash600"] == result["restored600"] == 600 and
                           result["hash720"] == result["replay720"] == 720)
        if not result["equal"]:
            raise ValueError("rollback value mismatch")
        result["status"] = "OK"
    except ValueError as exc:
        result["failure"] = str(exc)
    finally:
        binding.free_checkpoint(checkpoint)
        binding.free_session(session)
        binding.free_workspace(workspace)
        binding.free_runtime(runtime)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binding", required=True)
    parser.add_argument("--out", required=True, help="fresh evidence directory")
    args = parser.parse_args()
    library = Path(args.binding).resolve(strict=True)
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=False)
    binding = Binding(library)
    cases = [probe(binding, out, name, source) for name, source in SOURCES.items()]
    result = dict(schema="zr-swarm-session-rollback-probe/1", status="OK" if all(
        case["status"] == "OK" for case in cases) else "FAIL",
        binding_sha256=hashlib.sha256(library.read_bytes()).hexdigest(), cases=cases)
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if result["status"] == "OK" else 1


if __name__ == "__main__":
    raise SystemExit(main())
