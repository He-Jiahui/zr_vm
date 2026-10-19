"""Drive the real CLI stdin loop; no REPL/parser substitutes or source copies."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess


CASES = (
    "short_control", "cross_1023", "cross_2046", "exact_boundary",
    "crlf_split", "crlf_control",
)


def scenario(case: str) -> tuple[bytes, bytes]:
    newline = "\r\n" if case.startswith("crlf_") else "\n"
    length = {"short_control": 32, "cross_1023": 1100,
              "cross_2046": 2200, "crlf_control": 32}.get(case)
    if length is not None:
        payload = "REPL_PAYLOAD_BEGIN_" + "Q" * length + "_PAYLOAD_END"
        source = '"' + payload + '"' + newline + newline
    else:
        # A complete fgets chunk followed only by LF must not submit a cell.
        # The next physical line completes this still-unfinished declaration.
        prefix = 'var boundary = "'
        first_length = 1022 if case == "crlf_split" else 1023
        payload = "BOUNDARY_BEGIN_" + "Q" * (
            first_length - len(prefix) - 1 - len("BOUNDARY_BEGIN__END")
        ) + "_END"
        first = prefix + payload + '"'
        assert len(first.encode("ascii")) == first_length
        source = (first + newline + ' + "TAIL";' + newline + newline
                  + "boundary" + newline + newline)
        payload += "TAIL"
    source += "7 + 8" + newline + newline + ":quit" + newline
    return source.encode("ascii"), payload.encode("ascii")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--temp-root", type=Path, required=True)
    parser.add_argument("--case", choices=CASES, required=True)
    args = parser.parse_args()
    cli = args.cli.resolve(strict=True)
    if not args.temp_root.is_absolute():
        raise ValueError("temp-root must be absolute")
    root = args.temp_root.resolve()
    root.mkdir(parents=True, exist_ok=True)
    temporary = root / args.case
    temporary.mkdir()  # Never reuse/delete a previous process's unknown files.
    environment = os.environ.copy()
    environment.update(TEMP=str(temporary), TMP=str(temporary),
                       TMPDIR=str(temporary), PYTHONDONTWRITEBYTECODE="1")
    source, expected = scenario(args.case)
    child = subprocess.Popen(
        [str(cli)], cwd=cli.parent, stdin=subprocess.PIPE,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=environment,
    )
    try:
        output, _ = child.communicate(source, timeout=20)
    except subprocess.TimeoutExpired:
        child.kill()
        output, _ = child.communicate()  # Reap this exact owned child.
        raise AssertionError(f"REPL timeout; exit={child.returncode}\n{output!r}")
    finally:
        if child.poll() is None:
            child.kill()
            child.communicate()
    normalized = output.replace(b"\r\n", b"\n")
    assert child.returncode == 0, (child.returncode, output)
    assert b"ZR VM REPL" in normalized, output
    assert normalized.count(expected) == 1, output
    assert b"\n15\n" in normalized, output
    assert not re.search(rb"\b(?:error|failed|fatal)\b", normalized,
                         re.IGNORECASE), output
    print(f"{args.case}: intact payload ({len(expected)} bytes), retry expression, exit=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
