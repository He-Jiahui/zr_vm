#!/usr/bin/env python3
"""Exercise the CLI debug token handoff against its real loopback debug service."""

from __future__ import annotations

import argparse
import json
import queue
import re
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path


HANDSHAKE = re.compile(r"debug_endpoint=(\S+) debug_token=([0-9a-f]{64})")
PROJECT = '{"name":"debug_cli_auth","source":"src","binary":"bin","entry":"main"}\n'
SOURCE = 'return "debug auth smoke";\n'


def start_pipe_reader(stream, lines: queue.Queue[str], collected: list[str]) -> threading.Thread:
    def read() -> None:
        for line in iter(stream.readline, ""):
            collected.append(line)
            lines.put(line)
        lines.put("")

    thread = threading.Thread(target=read, name="debug-cli-pipe-reader", daemon=True)
    thread.start()
    return thread


def receive_handshake(stdout_lines: queue.Queue[str], visible_stdout: list[str], process: subprocess.Popen[str]):
    deadline = time.monotonic() + 15.0
    while time.monotonic() < deadline:
        if process.poll() is not None and stdout_lines.empty():
            break
        try:
            line = stdout_lines.get(timeout=min(0.2, max(0.01, deadline - time.monotonic())))
        except queue.Empty:
            continue
        if line == "":
            break
        match = HANDSHAKE.search(line)
        if match:
            return match.group(1), match.group(2)
        visible_stdout.append(line)
    raise RuntimeError(
        "CLI did not print a complete debug handshake; "
        f"exit={process.poll()} stdout={''.join(visible_stdout)!r}"
    )


def read_exact(sock: socket.socket, length: int) -> bytes:
    data = bytearray()
    while len(data) < length:
        chunk = sock.recv(length - len(data))
        if not chunk:
            raise RuntimeError("debug service closed before returning an initialize response")
        data.extend(chunk)
    return bytes(data)


def initialize(endpoint: str, token: str | None) -> dict:
    host, port_text = endpoint.rsplit(":", 1)
    params = {"authToken": token} if token is not None else {}
    payload = json.dumps(
        {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": params},
        separators=(",", ":"),
    ).encode("utf-8")
    with socket.create_connection((host, int(port_text)), timeout=5.0) as sock:
        sock.settimeout(5.0)
        sock.sendall(struct.pack(">I", len(payload)) + payload)
        deadline = time.monotonic() + 8.0
        while time.monotonic() < deadline:
            header = read_exact(sock, 4)
            (length,) = struct.unpack(">I", header)
            response = json.loads(read_exact(sock, length).decode("utf-8"))
            if response.get("id") == 1:
                return response
    raise RuntimeError("debug service did not answer initialize within eight seconds")


def terminate(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=5.0)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5.0)


def run_case(cli: Path, project: Path, token_mode: str | None, expected_success: bool) -> None:
    stdout_lines: queue.Queue[str] = queue.Queue()
    stderr_lines: queue.Queue[str] = queue.Queue()
    collected_stdout: list[str] = []
    collected_stderr: list[str] = []
    visible_stdout: list[str] = []
    process = subprocess.Popen(
        [
            str(cli),
            str(project),
            "--execution-mode",
            "interp",
            "--debug",
            "--debug-address",
            "127.0.0.1:0",
            "--debug-wait",
            "--debug-print-endpoint",
        ],
        cwd=project.parent,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
    )
    assert process.stdout is not None and process.stderr is not None
    stdout_reader = start_pipe_reader(process.stdout, stdout_lines, collected_stdout)
    stderr_reader = start_pipe_reader(process.stderr, stderr_lines, collected_stderr)
    try:
        endpoint, generated_token = receive_handshake(stdout_lines, visible_stdout, process)
        supplied_token = generated_token if token_mode == "generated" else token_mode
        response = initialize(endpoint, supplied_token)
        if expected_success:
            assert "error" not in response, f"correct token was rejected: {response}"
            assert response.get("result", {}).get("protocol") == "zrdbg/1", response
        else:
            assert response.get("error", {}).get("code") == -32001, response
        terminate(process)
        stdout_reader.join(timeout=1.0)
        stderr_reader.join(timeout=1.0)
        # The secret is present only in the private handshake line; all other CLI output is safe to forward.
        ordinary_stdout = [line for line in collected_stdout if HANDSHAKE.search(line) is None]
        assert sum(HANDSHAKE.search(line) is not None for line in collected_stdout) == 1
        assert all(generated_token not in line for line in ordinary_stdout), "token leaked into ordinary stdout"
        assert all(generated_token not in line for line in collected_stderr), "token leaked into stderr"
    finally:
        terminate(process)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", required=True, type=Path, help="existing zr_vm_cli executable")
    parser.add_argument("--tmp-dir", required=True, type=Path, help="task-owned tests/tmp directory")
    args = parser.parse_args()
    cli = args.cli.resolve(strict=True)
    task_tmp = args.tmp_dir.resolve(strict=True)
    fixture_root = task_tmp / "debug-cli-auth-fixture"
    if fixture_root.exists():
        raise FileExistsError(f"refusing to reuse an existing fixture path: {fixture_root}")

    fixture_root.mkdir()
    try:
        project_root = fixture_root / "project"
        source_dir = project_root / "src"
        (project_root / "bin").mkdir(parents=True)
        source_dir.mkdir()
        project = project_root / "debug_cli_auth.zrp"
        project.write_text(PROJECT, encoding="utf-8")
        (source_dir / "main.zr").write_text(SOURCE, encoding="utf-8")

        print("case: correct generated token")
        run_case(cli, project, "generated", expected_success=True)
        print("case: missing token")
        run_case(cli, project, None, expected_success=False)
        print("case: incorrect token")
        run_case(cli, project, "wrong-token", expected_success=False)
        print("CLI debug authentication integration: PASS")
        return 0
    finally:
        shutil.rmtree(fixture_root)


if __name__ == "__main__":
    sys.exit(main())
