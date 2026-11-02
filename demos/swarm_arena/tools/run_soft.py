#!/usr/bin/env python3
"""GD-H5: exact frozen snapshots, repeated runs, C/DOD parity and PNG decoding."""
import argparse
import csv
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import time
import zlib

DEMO = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("swarm_image_diff", DEMO / "tools/image_diff.py")
images = importlib.util.module_from_spec(spec)
spec.loader.exec_module(images)
CASES = {"smoke": (120, (60, 120)), "arena_2k": (60, (60,)),
         "swarm_10k": (60, (60,)), "bullet_storm": (60, (60,)), "particle_burst": (60, (60,))}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_png(path):
    """Independent stdlib decoder for the miniz RGBA8 PNG output, including CRCs."""
    data = Path(path).read_bytes()
    require(data[:8] == b"\x89PNG\r\n\x1a\n", "invalid PNG signature")
    position, compressed, header = 8, bytearray(), None
    ended = False
    while position < len(data):
        length = struct.unpack_from(">I", data, position)[0]
        kind = data[position + 4:position + 8]
        payload = data[position + 8:position + 8 + length]
        crc = struct.unpack_from(">I", data, position + 8 + length)[0]
        require(zlib.crc32(kind + payload) == crc, "PNG CRC mismatch")
        position += length + 12
        if kind == b"IHDR":
            require(header is None, "duplicate PNG IHDR")
            header = struct.unpack(">IIBBBBB", payload)
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            require(not payload and position == len(data), "PNG trailing data")
            ended = True
            break
    require(ended and header is not None, "PNG missing header/end")
    width, height, depth, color, compression, filtering, interlace = header
    require((width, height, depth, color, compression, filtering, interlace) ==
            (640, 360, 8, 6, 0, 0, 0), "unexpected PNG format")
    raw = zlib.decompress(compressed)
    stride = width * 4
    require(len(raw) == height * (stride + 1), "incorrect PNG scanline size")
    decoded = bytearray()
    previous = bytearray(stride)
    for y in range(height):
        offset = y * (stride + 1)
        filter_type = raw[offset]
        row = bytearray(raw[offset + 1:offset + 1 + stride])
        require(filter_type <= 4, "unsupported PNG filter")
        for i in range(stride):
            left, above = row[i - 4] if i >= 4 else 0, previous[i]
            upper_left = previous[i - 4] if i >= 4 else 0
            if filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            elif filter_type == 4:
                p = left + above - upper_left
                distances = (abs(p - left), abs(p - above), abs(p - upper_left))
                predictor = (left, above, upper_left)[distances.index(min(distances))]
            else:
                predictor = 0
            row[i] = (row[i] + predictor) & 255
        decoded.extend(row)
        previous = row
    return width, height, bytes(decoded)


def verify_formats(ppm, png):
    width, height, rgb = images.read_ppm(ppm)
    pw, ph, rgba = read_png(png)
    require((width, height) == (pw, ph) == (640, 360), "snapshot size mismatch")
    require(all(a == 255 for a in rgba[3::4]), "snapshot alpha is not opaque")
    require(all(rgb[i * 3:i * 3 + 3] == rgba[i * 4:i * 4 + 3]
                for i in range(width * height)), "PPM/PNG decoded pixels differ")
    return rgba


def image_hash(rgba):
    value = 2166136261
    for byte in rgba:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value & 0x7fffffff


def verify_goldens(candidates, golden_root=DEMO / "goldens"):
    metadata = json.loads((golden_root / "images.json").read_text())
    require(metadata["schema"] == "zr-swarm-soft-goldens/1", "wrong soft golden schema")
    require(set(metadata["images"]) == set(candidates), "soft golden inventory changed")
    for name, actual in candidates.items():
        expected = metadata["images"][name]["sha256"]
        require(digest(golden_root / name) == expected, name + " frozen golden checksum changed")
        require(actual["sha256"] == expected, name + " exact golden mismatch")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--capture", action="store_true", help="capture candidates only; never writes goldens")
    args = parser.parse_args()
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    result = dict(schema="zr-swarm-gd-h5/1", status="FAIL", steps=[], candidates={}, cases={})
    runtime = out / "runtime"
    runtime.mkdir(exist_ok=True)
    env = os.environ.copy()
    env.update(TMPDIR=str(runtime), XDG_CACHE_HOME=str(runtime / "cache"))
    # Soft acceptance must work with no desktop display, including GL-enabled builds.
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)

    def execute(name, command, expected=0):
        started = time.monotonic()
        command = list(map(str, command))
        proc = subprocess.run(command, env=env, capture_output=True, text=True, timeout=180)
        output = proc.stdout + proc.stderr
        (out / (name + ".log")).write_text(output)
        result["steps"].append(dict(name=name, command=command, exit=proc.returncode,
                                    seconds=round(time.monotonic() - started, 3)))
        require(not any(marker in output for marker in ("AddressSanitizer", "LeakSanitizer", "runtime error:")),
                name + " sanitizer diagnostic")
        require(proc.returncode == expected, name + " unexpected exit; see log")
        if expected:
            require("SWARM_ERROR" in output and "SWARM_OK" not in output, name + " unclear error")
        return output

    def candidate(name, actual, **metadata):
        result["candidates"][name] = dict(sha256=digest(actual), path=str(actual), **metadata)

    try:
        result.update(host_sha256=digest(args.host), fixture_sha256=digest(args.fixture))
        for repeat in range(3):
            ppm, png = out / f"fixture-{repeat}.ppm", out / f"fixture-{repeat}.png"
            output = execute(f"fixture-{repeat}", [args.fixture, ppm, png])
            require("SOFT_TEST_OK" in output, "native fixture did not finish")
            verify_formats(ppm, png)
            if repeat:
                for extension in ("ppm", "png"):
                    require(digest(out / f"fixture-{repeat}.{extension}") == digest(out / f"fixture-0.{extension}"),
                            "fixture repeat changed bytes")
        for extension in ("ppm", "png"):
            candidate(f"soft-fixture.{extension}", out / f"fixture-0.{extension}")

        project = out / "project"
        if project.exists():
            shutil.rmtree(project)
        shutil.copytree(DEMO / "game", project, ignore=shutil.ignore_patterns("bin", "test-bin"))
        # Exercise the actual VM-native gfx.snapshot export on the selected
        # backend. Only the copied test project changes; gameplay stays intact.
        source = project / "src/game.zr"
        original = source.read_text()
        anchor = 'hosted.rendered(<int>backend.snapshot(""));'
        require(original.count(anchor) == 1, "game shutdown snapshot hook changed")
        failures = []
        for path in (out / "absent/gfx.ppm", out / "absent/gfx.png", out / "unsupported.jpg"):
            failures.append('try { backend.snapshot(' + json.dumps(str(path)) + '); }\n'
                            '    catch (e) {\n'
                            '        if (e.message != "snapshot readback or image write failed") { throw e; }\n'
                            '        h5Rejected += 1;\n    }\n')
        hook = ('var h5Rejected = 0;\n    ' + '    '.join(failures) +
                '    if (h5Rejected != 3) { throw "GD-H5 missing native snapshot errors"; }\n'
                '    let h5System = import("zr.system");\n'
                '    h5System.console.printLine("GD_H5_NATIVE_ERRORS rejected=3");\n'
                '    var h5Hash = <int>backend.snapshot(' + json.dumps(str(project / "gfx.ppm")) + ');\n'
                '    if (<int>backend.snapshot(' + json.dumps(str(project / "gfx.png")) + ') != h5Hash) '
                '{ throw "GD-H5 PNG hash differs"; }\n    hosted.rendered(h5Hash);')
        source.write_text(original.replace(anchor, hook))
        manifest = project / "swarm_arena.zrp"

        for scenario, (frames, snapshots) in CASES.items():
            runs = []
            repeats = 3 if scenario == "smoke" else 1
            for lane in ("c", "dod"):
                for repeat in range(repeats):
                    name = f"{scenario}-{lane}-{repeat}"
                    directory = out / name
                    output = execute(name, [args.host, "--project", manifest, "--scenario", scenario,
                                     "--frames", frames, "--seed", 12345, "--lane", lane, "--render", "soft",
                                     "--snapshot", ",".join(map(str, snapshots)), "--out", directory])
                    require("SWARM_OK " in output, name + " missing success marker")
                    summary = json.loads((directory / "summary.json").read_text())
                    require(summary["render"] == "soft" and summary["frames"] == frames and
                            summary["status"] == "ok", name + " incorrect summary")
                    with (directory / "frames.csv").open(newline="") as file:
                        rows = list(csv.DictReader(file))
                    require(len(rows) == frames, name + " wrong frame count")
                    hashes = [row["hash"] for row in rows]
                    counts = [row["sprites"] for row in rows]
                    captures = {frame: digest(directory / f"frame-{frame:06d}.ppm") for frame in snapshots}
                    if runs:
                        require(hashes == runs[0]["frame_hashes"] and counts == runs[0]["sprite_counts"] and
                                summary["hash"] == runs[0]["simulation_hash"], name + " simulation diverged")
                        require(captures == runs[0]["images"], name + " image bytes diverged")
                    if scenario == "smoke":
                        require(summary["hash"] == 552988638, "smoke simulation golden changed")
                    if lane == "dod":
                        require("GD_H5_NATIVE_ERRORS rejected=3" in output, "gfx.snapshot native errors not caught")
                        rgba = verify_formats(project / "gfx.ppm", project / "gfx.png")
                        require(digest(project / "gfx.ppm") == captures[frames], "gfx.snapshot/host image differs")
                        require(f"render_hash={image_hash(rgba)}" in output, "gfx.snapshot returned wrong image hash")
                    runs.append(dict(lane=lane, repeat=repeat, simulation_hash=summary["hash"], frame_hashes=hashes,
                                     sprite_counts=counts, images=captures, backend_us=summary["backend_us"]))
            result["cases"][scenario] = runs
            for frame in snapshots:
                candidate(f"soft-{scenario}-{frame:06d}.ppm", out / f"{scenario}-c-0/frame-{frame:06d}.ppm",
                          scenario=scenario, frame=frame, seed=12345, lanes=["c", "dod"],
                          simulation_hash=runs[0]["simulation_hash"] if frame == frames else None)

        # The explicit filename form captures the final frame in either format.
        for extension in ("ppm", "png"):
            path = out / f"smoke-final.{extension}"
            execute("final-" + extension, [args.host, "--project", manifest, "--lane", "c", "--frames", 120,
                                         "--render", "soft", "--snapshot", path, "--out", out / ("final-" + extension)])
            candidate(f"soft-smoke-000120.{extension}", path, scenario="smoke", frame=120, seed=12345,
                      simulation_hash=552988638, lanes=["c", "dod"])
        require(digest(out / "smoke-final.ppm") == result["cases"]["smoke"][0]["images"][120],
                "filename/sequence snapshots differ")
        verify_formats(out / "smoke-final.ppm", out / "smoke-final.png")

        for name, options in (
                ("soft-input", ["--render", "soft", "--interactive"]),
                ("null-snapshot", ["--snapshot", out / "bad.ppm"]),
                ("bad-extension", ["--render", "soft", "--snapshot", out / "bad.jpg"]),
                ("bad-frame", ["--render", "soft", "--snapshot", "2"]),
                ("duplicate-frame", ["--render", "soft", "--snapshot", "1,1"]),
                ("ppm-write-failure", ["--render", "soft", "--snapshot", out / "absent/bad.ppm"]),
                ("png-write-failure", ["--render", "soft", "--snapshot", out / "absent/bad.png"])):
            directory = out / name
            execute(name, [args.host, "--project", manifest, "--lane", "c", "--frames", 1,
                           "--out", directory, *options], expected=2)
            require(not (directory / "summary.json").exists(), name + " left success artifacts")
        source.write_text(original.replace(anchor, 'hosted.rendered(<int>backend.snapshot(' +
                                          json.dumps(str(out / "absent/gfx.png")) + '));'))
        directory = out / "gfx-write-failure"
        execute("gfx-write-failure", [args.host, "--project", manifest, "--lane", "dod", "--frames", 1,
                                     "--render", "soft", "--out", directory], expected=2)
        require(not (directory / "summary.json").exists(), "gfx write failure left successful summary")

        if not args.capture:
            verify_goldens(result["candidates"])
        result["status"] = "CAPTURED" if args.capture else "OK"
        return 0
    except (ValueError, OSError, subprocess.TimeoutExpired, KeyError, struct.error, zlib.error) as exc:
        result["error"] = str(exc)
        return 1
    finally:
        (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
        print("GD-H5 " + result["status"] + " evidence=" + str(out))
        if "error" in result:
            print(result["error"])


if __name__ == "__main__":
    raise SystemExit(main())
