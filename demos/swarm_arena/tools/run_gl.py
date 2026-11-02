#!/usr/bin/env python3
"""GD-M1: private Xvfb/llvmpipe, native geometry and host snapshot acceptance."""
import argparse
import ctypes
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import select
import shutil
import struct
import subprocess
import time

DEMO = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("swarm_image_diff", DEMO / "tools/image_diff.py")
images = importlib.util.module_from_spec(spec)
spec.loader.exec_module(images)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fixture_coverage(path):
    """Independent pixel-center coverage for the fixture's rotated world quads.

    This analytic reference catches geometry/camera mistakes independently of
    the separate H5 software-backend comparison.
    HUD rows are excluded; blending changes color but not these opaque masks.
    """
    width, height, pixels = images.read_ppm(path)
    quads = [(100, 50, 48, 0), (300, 150, 64, 0.45),
             (640, 290, 60, 0.3), (100, 300, 20, 0)]
    expected_count = actual_count = intersection = union = 0
    for y in range(40, height):
        for x in range(width):
            expected = False
            for cx, cy, size, angle in quads:
                dx, dy = x + 0.5 - cx, y + 0.5 - cy
                c, s = math.cos(angle), math.sin(angle)
                u, v = dx * c + dy * s, -dx * s + dy * c
                expected |= abs(u) < size / 2 and abs(v) < size / 2
            i = (y * width + x) * 3
            actual = pixels[i:i + 3] != bytes((16, 20, 28))
            expected_count += expected
            actual_count += actual
            intersection += expected and actual
            union += expected or actual
    result = dict(reference="analytic rotated quads",
                  expected_pixels=expected_count, actual_pixels=actual_count,
                  intersection=intersection, union=union, iou=intersection / union)
    require(result["iou"] >= 0.995, "fixture coverage differs from analytic reference")
    return result


def keyboard_check(fixture, env, out):
    """Drive the real X11 window through XTEST and observe GLFW axis changes."""
    x11 = ctypes.CDLL("libX11.so.6")
    xtest = ctypes.CDLL("libXtst.so.6")
    pointer, window_type = ctypes.c_void_p, ctypes.c_ulong
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = pointer
    x11.XDefaultRootWindow.argtypes = [pointer]
    x11.XDefaultRootWindow.restype = window_type
    x11.XQueryTree.argtypes = [pointer, window_type, ctypes.POINTER(window_type), ctypes.POINTER(window_type),
                               ctypes.POINTER(ctypes.POINTER(window_type)), ctypes.POINTER(ctypes.c_uint)]
    x11.XFetchName.argtypes = [pointer, window_type, ctypes.POINTER(ctypes.c_char_p)]
    x11.XFree.argtypes = [pointer]
    x11.XSetInputFocus.argtypes = [pointer, window_type, ctypes.c_int, ctypes.c_ulong]
    x11.XMapRaised.argtypes = [pointer, window_type]
    x11.XSync.argtypes = [pointer, ctypes.c_int]
    x11.XSetErrorHandler.argtypes = [pointer]
    x11.XSetErrorHandler.restype = pointer
    x11.XKeysymToKeycode.argtypes = [pointer, ctypes.c_ulong]
    x11.XKeysymToKeycode.restype = ctypes.c_uint
    x11.XFlush.argtypes = [pointer]
    x11.XCloseDisplay.argtypes = [pointer]
    xtest.XTestFakeKeyEvent.argtypes = [pointer, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
    # Xlib reads authentication from this process's environment as well.
    previous_auth = os.environ.get("XAUTHORITY")
    os.environ["XAUTHORITY"] = env["XAUTHORITY"]
    display = x11.XOpenDisplay(env["DISPLAY"].encode())
    if previous_auth is None:
        os.environ.pop("XAUTHORITY", None)
    else:
        os.environ["XAUTHORITY"] = previous_auth
    require(display, "cannot open X11 display for keyboard test")
    x_errors = []
    @ctypes.CFUNCTYPE(ctypes.c_int, pointer, pointer)
    def error_handler(_display, _error):
        x_errors.append("X11 request failed")
        return 0
    previous_handler = x11.XSetErrorHandler(ctypes.cast(error_handler, pointer))
    trace = []
    process = None
    try:
        with (out / "keyboard-stderr.log").open("w") as log:
            process = subprocess.Popen([fixture, "--keys"], env=env, stdout=subprocess.PIPE,
                                       stderr=log, text=True, bufsize=1)
        root = x11.XDefaultRootWindow(display)
        target = None
        deadline = time.monotonic() + 8
        while not target and time.monotonic() < deadline:
            root_return, parent = window_type(), window_type()
            children = ctypes.POINTER(window_type)()
            count = ctypes.c_uint()
            if x11.XQueryTree(display, root, ctypes.byref(root_return), ctypes.byref(parent),
                              ctypes.byref(children), ctypes.byref(count)):
                for i in range(count.value):
                    name = ctypes.c_char_p()
                    if x11.XFetchName(display, children[i], ctypes.byref(name)) and name.value:
                        if name.value.startswith(b"Swarm Arena - WASD"):
                            target = children[i]
                        x11.XFree(name)
                if children:
                    x11.XFree(children)
            if not target:
                time.sleep(0.02)
        require(target, "keyboard window not found")
        x11.XMapRaised(display, target)
        x11.XSync(display, 0)
        x11.XSetInputFocus(display, target, 2, 0)
        x11.XSync(display, 0)
        require(not x_errors, "cannot focus keyboard window")

        def expect(line):
            ready, _, _ = select.select([process.stdout], [], [], 3)
            actual = process.stdout.readline().strip() if ready else "TIMEOUT"
            trace.append(actual)
            require(actual == line, "keyboard expected " + line + ", got " + actual)

        expect("KEYS 0 0")
        sequence = [(ord("d"), 1, "1 0"), (ord("w"), 1, "1 -1"),
                    (ord("a"), 1, "0 -1"), (ord("d"), 0, "-1 -1"),
                    (ord("w"), 0, "-1 0"), (ord("a"), 0, "0 0"),
                    (ord("s"), 1, "0 1"), (ord("s"), 0, "0 0")]
        for symbol, pressed, axes in sequence:
            require(xtest.XTestFakeKeyEvent(display, x11.XKeysymToKeycode(display, symbol), pressed, 0),
                    "XTEST key injection unavailable")
            x11.XFlush(display)
            expect("KEYS " + axes)
        xtest.XTestFakeKeyEvent(display, x11.XKeysymToKeycode(display, 0xff1b), 1, 0)  # Escape
        x11.XFlush(display)
        expect("KEYS_CLOSED")
        require(process.wait(timeout=3) == 0, "keyboard window shutdown failed")
        return dict(status="OK", axes=trace, injection="XTEST into a visible Xvfb window")
    finally:
        if process and process.poll() is None:
            process.kill()
            process.wait()
        x11.XCloseDisplay(display)
        x11.XSetErrorHandler(previous_handler)
        (out / "keyboard.log").write_text("\n".join(trace) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host")
    parser.add_argument("--fixture")
    parser.add_argument("--soft-fixture")
    parser.add_argument("--xvfb")
    parser.add_argument("--out", required=True)
    parser.add_argument("--unavailable")
    parser.add_argument("--capture", action="store_true", help="capture for review; do not compare/update goldens")
    args = parser.parse_args()
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    result = dict(schema="zr-swarm-gd-m1/1", status="FAIL", steps=[])
    server = None
    read_fd = write_fd = None
    try:
        if args.unavailable:
            result.update(status="SKIP", reason=args.unavailable)
            return 77
        require(args.host and args.fixture and args.xvfb, "missing GL test executables")
        env = os.environ.copy()
        runtime = out / "runtime"
        runtime.mkdir(exist_ok=True)
        env.update(LIBGL_ALWAYS_SOFTWARE="1", GALLIUM_DRIVER="llvmpipe", LP_NUM_THREADS="1",
                   TMPDIR=str(runtime), XDG_CACHE_HOME=str(runtime / "cache"),
                   XDG_RUNTIME_DIR=str(runtime))
        runtime.chmod(0o700)
        # TCP and automatic display allocation avoid fixed display locks and Unix
        # socket files. The private
        # server and clients share a fresh MIT cookie stored in the evidence dir.
        authority = out / "Xauthority"
        fields = [b"", b"", b"MIT-MAGIC-COOKIE-1", os.urandom(16)]
        authority.write_bytes(struct.pack(">H", 65535) +
                              b"".join(struct.pack(">H", len(f)) + f for f in fields))
        authority.chmod(0o600)
        env["XAUTHORITY"] = str(authority)
        read_fd, write_fd = os.pipe()
        command = [args.xvfb, "-displayfd", str(write_fd), "-screen", "0", "640x360x24",
                   "-nolisten", "unix", "-listen", "tcp", "-noreset", "-auth", str(authority)]
        with (out / "xvfb.log").open("wb") as log:
            server = subprocess.Popen(command, env=env, stdout=log, stderr=log, pass_fds=(write_fd,))
        os.close(write_fd)
        write_fd = None
        ready, _, _ = select.select([read_fd], [], [], 15)
        display = os.read(read_fd, 64).decode().strip() if ready else ""
        require(display.isdecimal(), "Xvfb did not publish a display; see xvfb.log")
        env["DISPLAY"] = "localhost:" + display
        result.update(display=env["DISPLAY"], software="llvmpipe", xvfb_command=command)

        def execute(name, command, expected=0):
            started = time.monotonic()
            process = subprocess.run(list(map(str, command)), env=env, capture_output=True, text=True, timeout=180)
            output = process.stdout + process.stderr
            (out / (name + ".log")).write_text(output)
            result["steps"].append(dict(name=name, command=list(map(str, command)), exit=process.returncode,
                                        seconds=round(time.monotonic() - started, 3)))
            require(not any(s in output for s in ("AddressSanitizer", "LeakSanitizer", "runtime error:")),
                    name + " sanitizer diagnostic")
            if name == "probe" and process.returncode == 77:
                return None
            require(process.returncode == expected, name + ": unexpected exit; see log")
            return output

        if execute("probe", [args.fixture, "--probe"]) is None:
            result.update(status="SKIP", reason="GL3.3 context unavailable; see probe.log")
            return 77
        output = execute("fixture", [args.fixture, out / "fixture.ppm"])
        require("llvmpipe" in output.lower() and "GL_TEST_OK" in output, "fixture must run on llvmpipe")
        result["coverage"] = fixture_coverage(out / "fixture.ppm")
        if args.soft_fixture:
            execute("soft-fixture", [args.soft_fixture, out / "soft-fixture.ppm", out / "soft-fixture.png"])
            result["soft_backend_coverage"] = dict(
                fixture=images.coverage(out / "soft-fixture.ppm", out / "fixture.ppm", y0=40),
                rotated_quads=images.coverage(out / "soft-fixture.ppm", out / "fixture.ppm", x0=200, y0=40),
                reference="GD-H5 soft backend", actual="llvmpipe GL33",
                geometry="soft rot=0 circles, rot!=0 quads; GL all quads; full-image parity is not required")
            require(result["soft_backend_coverage"]["rotated_quads"]["iou"] >= 0.995,
                    "GL/soft shared rotated quad coverage differs")
        # XTEST is an optional test aid, not a requirement for the renderer.
        try:
            result["keyboard"] = keyboard_check(args.fixture, env, out)
        except OSError as exc:
            result["keyboard"] = dict(status="UNAVAILABLE", reason=str(exc))

        project = out / "project"
        if project.exists():
            shutil.rmtree(project)
        shutil.copytree(DEMO / "game", project, ignore=shutil.ignore_patterns("bin", "test-bin"))
        common = [args.host, "--project", project / "swarm_arena.zrp", "--scenario", "smoke",
                  "--frames", "120", "--seed", "12345", "--expect-hash", "552988638"]
        for lane in ("c", "dod"):
            command = [*common, "--lane", lane, "--render", "gl", "--headless",
                       "--snapshot", out / (lane + ".ppm"), "--out", out / lane]
            output = execute("host-" + lane, command)
            require("SWARM_OK " in output and "llvmpipe" in output.lower(), lane + " GL smoke failed")
            summary = json.loads((out / lane / "summary.json").read_text())
            require(summary["render"] == "gl" and summary["frames"] == 120 and summary["hash"] == 552988638,
                    lane + " GL summary mismatch")
        result["lane_images"] = images.compare(out / "c.ppm", out / "dod.ppm")
        require(result["lane_images"]["passed"], "DOD/C snapshots differ")
        if args.soft_fixture:
            execute("soft-smoke", [*common, "--lane", "c", "--render", "soft", "--headless",
                                   "--snapshot", out / "soft-smoke.ppm", "--out", out / "soft-smoke"])
            result["soft_backend_coverage"]["smoke"] = images.coverage(out / "soft-smoke.ppm", out / "c.ppm")
            execute("png-snapshot", [*common, "--lane", "c", "--render", "gl", "--headless",
                                     "--snapshot", out / "c.png", "--out", out / "png-snapshot"])
            png_spec = importlib.util.spec_from_file_location("swarm_soft_checks", DEMO / "tools/run_soft.py")
            png_checks = importlib.util.module_from_spec(png_spec)
            png_spec.loader.exec_module(png_checks)
            png_checks.verify_formats(out / "c.ppm", out / "c.png")
            result["png_snapshot"] = dict(status="OK", decoded_pixels="byte-exact to GL PPM")
        execute("null", [*common, "--lane", "c", "--render", "null", "--out", out / "null"])
        gl_summary = json.loads((out / "c/summary.json").read_text())
        null_summary = json.loads((out / "null/summary.json").read_text())
        require(gl_summary["hash"] == null_summary["hash"], "GL changed simulation state")
        idle_hashes = []
        for lane in ("c", "dod"):
            execute("window-idle-" + lane,
                    [args.host, "--project", project / "swarm_arena.zrp", "--scenario", "smoke",
                     "--frames", "1", "--lane", lane, "--render", "gl", "--interactive",
                     "--out", out / ("window-idle-" + lane)])
            summary = json.loads((out / ("window-idle-" + lane) / "summary.json").read_text())
            idle_hashes.append(summary["hash"])
        require(idle_hashes[0] == idle_hashes[1], "C/DOD interactive input diverges")
        result["interactive_idle_hash"] = idle_hashes[0]
        for name, options in (
                ("headless-input", ["--render", "gl", "--headless", "--interactive"]),
                ("null-input", ["--interactive"]),
                ("null-snapshot", ["--snapshot", out / "bad.ppm"]),
                ("bad-extension", ["--render", "gl", "--snapshot", out / "bad.jpg"]),
                ("bad-frame", ["--render", "gl", "--snapshot", "2"]),
                ("duplicate-frame", ["--render", "gl", "--snapshot", "1,1"]),
                ("write-failure", ["--render", "gl", "--headless", "--snapshot", out / "absent/bad.ppm"])):
            output = execute(name, [args.host, "--project", project / "swarm_arena.zrp", "--lane", "c",
                                   "--frames", "1", "--out", out / name, *options], expected=2)
            require("SWARM_ERROR" in output and "SWARM_OK" not in output, name + " error not reported")
            require(not (out / name / "summary.json").exists(), name + " left success artifacts")
        execute("frame-snapshots", [args.host, "--project", project / "swarm_arena.zrp", "--lane", "c",
                                   "--frames", "2", "--render", "gl", "--headless", "--snapshot", "1,2",
                                   "--out", out / "frame-snapshots"])
        for frame in (1, 2):
            require(images.read_ppm(out / "frame-snapshots" / f"frame-{frame:06d}.ppm")[:2] == (640, 360),
                    "requested frame snapshot missing")

        # Boundary controls ensure the tolerance rule cannot silently weaken.
        reference = bytes([100] * 3000)
        require(images.compare_pixels(reference, bytes([102] * 3000))["passed"], "delta=2 rejected")
        changed = bytearray(reference)
        for i in range(5):
            changed[i * 3] += 3
        require(images.compare_pixels(reference, changed)["passed"], "99.5% match rejected")
        changed[15] += 3
        require(not images.compare_pixels(reference, changed)["passed"], "99.4% match accepted")

        if not args.capture:
            result["goldens"] = {}
            metadata = json.loads((DEMO / "goldens/images-gl33.json").read_text())
            for name, actual in (("fixture", "fixture.ppm"), ("smoke", "c.ppm")):
                filename = "gl33-" + name + ".ppm"
                golden = DEMO / "goldens" / filename
                require(hashlib.sha256(golden.read_bytes()).hexdigest() == metadata["images"][filename]["sha256"],
                        "golden checksum changed without a provenance update")
                comparison = images.compare(golden, out / actual)
                result["goldens"][name] = comparison
                require(comparison["passed"], name + " exceeds golden tolerance")
        result["snapshots_sha256"] = {name: hashlib.sha256((out / name).read_bytes()).hexdigest()
                                      for name in ("fixture.ppm", "c.ppm", "dod.ppm")}
        result.update(status="CAPTURED" if args.capture else "OK",
                      manual_desktop="separate John acceptance required")
        return 0
    except (ValueError, OSError, subprocess.TimeoutExpired, KeyError) as exc:
        result["error"] = str(exc)
        return 1
    finally:
        for fd in (read_fd, write_fd):
            if fd is not None:
                os.close(fd)
        if server is not None:
            server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()
        (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
        print("GD-M1 " + result["status"] + " evidence=" + str(out))
        if "error" in result:
            print(result["error"])


if __name__ == "__main__":
    raise SystemExit(main())
