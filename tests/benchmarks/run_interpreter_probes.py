#!/usr/bin/env python3
"""Run the review's focused interpreter probes, with isolated peak RSS readings.

Generated projects and JSON belong under a build directory. These are process
measurements; use performance_report's persistent scope for steady-state ratios.
"""
import argparse
import json
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path


def worker(command):
    started = time.perf_counter()
    run = subprocess.run(command, capture_output=True, text=True, timeout=180)
    peak = None
    try:
        import resource
        peak = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
        peak *= 1 if sys.platform == "darwin" else 1024
    except ImportError:
        pass
    print(json.dumps(dict(ms=(time.perf_counter() - started) * 1000,
                         peak_rss_bytes=peak, returncode=run.returncode,
                         stdout=run.stdout, stderr=run.stderr)))


def expected_checksum(name, smoke):
    # Enough allocation to finish a sliced cycle without adding explicit GC.
    trips = (20000 if name == "objauto" else 2000) if smoke else {
        "objauto": 2000000, "strdrop": 200000, "calls": 3000000,
        "methods": 2000000, "loop_lt": 20000000, "loop_le": 20000000,
        "floatloop": 5000000, "fields": 3000000, "arrays": 3000000,
        "strcat": 300000, "intloop": 10000000, "closure": 2000000,
    }.get(name, 0)
    if name in ("deep_alloc0", "deep_alloc"):
        return 2000 if smoke else 300000
    if name == "fib":
        return 196418
    if name == "methods":
        return 0
    if name in ("loop_lt", "loop_le"):
        return trips * (trips - 1) // 2
    if name == "floatloop":
        x, acc = .5, 0.
        for _ in range(trips):
            acc += x * 1.0001
            x = x * .99999 + .00001
        return int(acc)
    if name == "fields":
        x, y = 1, 2
        for _ in range(trips):
            x += y
            y = (x & 15) + 1
        return x
    if name == "arrays":
        # Each residue j is visited q[j] times; sum (j + 1)..(j + q[j]).
        return sum((q := (trips + 1023 - j) // 1024) * j + q * (q + 1) // 2
                   for j in range(1024))
    if name == "intloop":
        return (trips // 8) * 28 + sum(range(trips % 8))
    return trips


def validate_sample(name, sample, smoke):
    output = sample["stdout"]
    expected = str(expected_checksum(name, smoke))
    if not output.splitlines() or output.splitlines()[0] != expected:
        raise RuntimeError(f"{name}: expected checksum {expected}, got {output[:200]!r}")
    match = re.search(r"gc collections minor=(\d+) major=(\d+) full=(\d+)", output)
    if not match:
        raise RuntimeError(f"{name}: missing heap collection summary")
    if name in ("objauto", "strdrop") and sum(map(int, match.groups())) == 0:
        raise RuntimeError(f"{name}: interpreter did not collect")
    if name == "strdrop":
        strings = re.search(r"type string count (\d+)", output)
        if not strings or int(strings[1]) >= 5000:
            raise RuntimeError(f"strdrop: weak strings were retained: {output}")
    # ASan quarantines freed allocations; its RSS is not a managed-heap bound.
    if not smoke and name == "objauto" and sample["peak_rss_bytes"] is not None:
        if sample["peak_rss_bytes"] >= 64 * 1024 * 1024:
            raise RuntimeError(f"objauto: peak RSS exceeds 64 MiB: {sample['peak_rss_bytes']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--cases", default="objauto,strdrop,calls,fib,methods,loop_lt,loop_le,floatloop,closure,fields,arrays,strcat,intloop,deep_alloc0,deep_alloc")
    parser.add_argument("--repeat", type=int, default=3)
    parser.add_argument("--smoke", action="store_true", help="Reduce loop trips for GC stress builds")
    parser.add_argument("--check-deep-ratio", action="store_true", help="Require depth-400 allocation time <= twice depth-0")
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    results = {}
    sources = Path(__file__).parent / "probes" / "interpreter"
    for name in args.cases.split(","):
        source = (sources / (name + ".zr")).read_text()
        if args.smoke:
            for trips in (20000000, 10000000, 5000000, 3000000, 2000000, 300000, 200000):
                source = source.replace(str(trips), "20000" if name == "objauto" else "2000")
        project = args.work_dir / name
        (project / "src").mkdir(parents=True, exist_ok=True)
        (project / "src" / "main.zr").write_text(source)
        manifest = project / (name + ".zrp")
        manifest.write_text(json.dumps(dict(name=name, source="src", binary="bin", entry="main")))
        samples = []
        for _ in range(args.repeat):
            command = [str(args.cli.resolve()), str(manifest.resolve()), "--heap-summary"]
            run = subprocess.run([sys.executable, __file__, "--worker", *command],
                                 capture_output=True, text=True, check=True)
            sample = json.loads(run.stdout)
            if sample["returncode"]:
                raise RuntimeError(f"{name}: {sample}")
            validate_sample(name, sample, args.smoke)
            samples.append(sample)
        results[name] = dict(median_ms=statistics.median(s["ms"] for s in samples), samples=samples)
        print(f"{name}: {results[name]['median_ms']:.2f} ms, peak RSS {samples[-1]['peak_rss_bytes']}", flush=True)
    (args.work_dir / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    if args.check_deep_ratio:
        ratio = results["deep_alloc"]["median_ms"] / results["deep_alloc0"]["median_ms"]
        print(f"deep allocation depth-400 / depth-0: {ratio:.3f}", flush=True)
        if ratio > 2.0:
            raise RuntimeError(f"deep allocation ratio {ratio:.3f} exceeds 2.0")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--worker":
        worker(sys.argv[2:])
    else:
        main()
