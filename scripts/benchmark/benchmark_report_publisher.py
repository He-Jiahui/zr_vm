#!/usr/bin/env python3
"""Publish report-only benchmark bundles through a destination-local staging dir."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


_REPORT_EXTENSIONS = {".csv", ".html", ".json", ".md", ".txt"}


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _is_report_file(source: Path, path: Path) -> bool:
    relative = path.relative_to(source)
    if path.suffix.lower() not in _REPORT_EXTENSIONS:
        return False
    if len(relative.parts) == 1:
        return True
    return relative.parts[0].startswith("performance") and not relative.parts[0].startswith(
        "performance_suite"
    )


def _default_run_id() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def publish_report_bundle(
    source_directory: str | os.PathLike[str],
    destination_root: str | os.PathLike[str],
    *,
    run_id: str | None = None,
) -> dict[str, Any]:
    source = Path(source_directory).resolve()
    destination = Path(destination_root).resolve()
    if not source.is_dir():
        raise ValueError(f"report source is not a directory: {source}")
    selected_run_id = run_id or _default_run_id()
    if not selected_run_id or selected_run_id in {".", ".."} or Path(selected_run_id).name != selected_run_id:
        raise ValueError("run_id must be one path component")
    destination.mkdir(parents=True, exist_ok=True)
    run_directory = destination / selected_run_id
    if run_directory.exists():
        raise FileExistsError(f"published run already exists: {run_directory}")

    files = [path for path in source.rglob("*") if path.is_file() and _is_report_file(source, path)]
    if not files:
        raise ValueError(f"no report files found under {source}")

    stage = Path(tempfile.mkdtemp(prefix=".benchmark-stage-", dir=destination))
    try:
        manifest_lines: list[str] = []
        for path in sorted(files, key=lambda item: item.relative_to(source).as_posix()):
            relative = path.relative_to(source)
            target = stage / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
            manifest_lines.append(f"{_sha256(target)}  {relative.as_posix()}")
        (stage / "SHA256SUMS").write_text("\n".join(manifest_lines) + "\n", encoding="ascii")
        os.replace(stage, run_directory)
    except BaseException:
        shutil.rmtree(stage, ignore_errors=True)
        raise

    latest_fd, latest_name = tempfile.mkstemp(prefix=".LATEST-", dir=destination)
    try:
        with os.fdopen(latest_fd, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(selected_run_id + "\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(latest_name, destination / "LATEST")
    except BaseException:
        try:
            os.unlink(latest_name)
        except FileNotFoundError:
            pass
        raise
    return {
        "run_id": selected_run_id,
        "run_directory": str(run_directory),
        "manifest": str(run_directory / "SHA256SUMS"),
        "file_count": len(files),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True)
    parser.add_argument("--destination", required=True)
    parser.add_argument("--run-id")
    args = parser.parse_args()
    try:
        result = publish_report_bundle(args.source, args.destination, run_id=args.run_id)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=__import__("sys").stderr)
        return 2
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
