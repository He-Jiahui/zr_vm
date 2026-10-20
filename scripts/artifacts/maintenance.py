"""One-shot inventory and explicitly released artifact maintenance on Windows."""
from __future__ import annotations

import argparse
from contextlib import closing
import datetime as dt
import json
import msvcrt
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import uuid

import policy


def utc():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def output_path(settings, path):
    path = policy.no_alias(path)
    if not settings.can_write(path):
        raise ValueError(f"Actual permission denies output artifact: {path}")
    return path


class AlreadyRunning(Exception):
    pass


class RunLock:
    """Windows releases this byte lock even if the worker crashes."""
    def __init__(self, path):
        self.path, self.stream = policy.no_alias(path), None

    def __enter__(self):
        self.stream = self.path.open("a+b")
        self.stream.seek(0, os.SEEK_END)
        if not self.stream.tell():
            self.stream.write(b"0")
            self.stream.flush()
        self.stream.seek(0)
        try:
            msvcrt.locking(self.stream.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError as exc:
            self.stream.close()
            raise AlreadyRunning(str(self.path)) from exc
        return self

    def __exit__(self, *_):
        self.stream.seek(0)
        msvcrt.locking(self.stream.fileno(), msvcrt.LK_UNLCK, 1)
        self.stream.close()


def process_census(powershell):
    # Commands are inspected in memory only; reports contain counts and PIDs,
    # never unrelated process command lines (which may contain credentials).
    script = r"""
$ErrorActionPreference='Stop'
try {
    $rows=@(Get-CimInstance Win32_Process | ForEach-Object {
        @{pid=[long]$_.ProcessId;name=$_.Name;executable_path=$_.ExecutablePath;command_line=$_.CommandLine}
    })
    $unknown=@($rows | Where-Object {$_.pid -gt 4 -and (-not $_.command_line -or -not $_.executable_path)})
    @{complete=($unknown.Count -eq 0);processes=$rows;inaccessible_pids=@($unknown | ForEach-Object {$_.pid});errors=@()} | ConvertTo-Json -Depth 5 -Compress
} catch {
    @{complete=$false;processes=@();inaccessible_pids=@();errors=@($_.Exception.Message)} | ConvertTo-Json -Depth 5 -Compress
}
"""
    try:
        completed = subprocess.run([str(powershell), "-NoProfile", "-NonInteractive", "-Command", script], capture_output=True, text=True, encoding="utf-8", timeout=45)
        if completed.returncode:
            raise ValueError("Process census command failed")
        data = json.loads(completed.stdout.lstrip("\ufeff"))
        if not isinstance(data.get("processes"), list):
            raise ValueError("Malformed process census")
        return data
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        return {"complete": False, "processes": [], "errors": [str(exc)]}


def read_sessions(repo):
    root = policy.no_alias(repo / ".codex/sessions")
    if not root.is_dir():
        return [{"path": str(root), "error": "Active session directory is unavailable"}]
    result = []
    try:
        for path in root.glob("*.md"):
            try:
                raw = policy.no_alias(path).read_bytes()
                result.append({"path": str(path), "sha256": __import__("hashlib").sha256(raw).hexdigest(), "text": raw.decode("utf-8-sig")})
            except (OSError, ValueError) as exc:
                result.append({"path": str(path), "error": str(exc)})
    except OSError as exc:
        result.append({"path": str(root), "error": str(exc)})
    return result


def ensure_state(settings):
    state = policy.no_alias(settings.roots["reports"] / "artifact-maintenance")
    ledger = output_path(settings, state / "artifacts.json")
    if not settings.can_write(state):
        raise ValueError(f"No state write permission: {state}")
    if state.exists() and not ledger.exists():
        raise ValueError(f"Existing state has no ownership ledger: {state}")
    if not state.exists():
        state.mkdir(parents=True)
        data = {"schema_version": 1, "task_id": "artifact-maintenance", "owner": "zr_vm artifact-maintenance worker",
                "status": "active", "source_root": str(settings.repo), "permission_config": str(settings.config),
                "output_roots": {"reports": str(state), "tmp": str(settings.roots["tmp"] / "artifact-maintenance"), "tests": str(settings.roots["tests"] / "artifact-maintenance")},
                "processes": [], "retained": [{"path": str(state), "kind": "recurring-maintenance-state", "reason": "Baseline, current state and immutable run evidence", "depends_on": [], "review_after": "after-explicit-task-uninstall"}],
                "unknown": [], "cleanup_candidates": [], "planned_artifacts": ["baseline.json", "current.sqlite3", "current.sqlite3-journal", "maintenance.lock", "runs/<run-id>/*", "tmp/artifact-maintenance/<run-id>/*.pending"]}
        with ledger.open("x", encoding="utf-8") as stream:
            json.dump(data, stream, indent=2)
    doc = policy.read_manifest(ledger)
    if doc.get("error") or doc["data"].get("task_id") != "artifact-maintenance" or policy.canonical(doc["data"].get("source_root", "")) != settings.repo:
        raise ValueError(f"Foreign or invalid maintenance state: {state}")
    return state


def inventory(settings, toolkit):
    trees = []
    exclusions = [settings.roots[key] / "artifact-maintenance" for key in ("reports", "tmp")]
    for root in settings.roots.values():
        tree = toolkit.scan_tree(root)
        tree["files"] = [item for item in tree["files"] if not any(policy.within(Path(item["path"]), excluded) for excluded in exclusions)]
        tree["directories"] = [item for item in tree["directories"] if not any(policy.within(Path(item), excluded) for excluded in exclusions)]
        tree["bytes"] = sum(item["bytes"] for item in tree["files"])
        tree.pop("extensions", None)
        trees.append(tree)
    return {"schema_version": 1, "scanned_utc": utc(), "config_sha256": settings.config_sha256, "trees": trees, "excluded_own_state": [str(path) for path in exclusions]}


def manifests_from_inventory(snapshot, state):
    paths = {Path(item["path"]) for tree in snapshot["trees"] for item in tree["files"] if Path(item["path"]).name.lower() == "artifacts.json" and Path(item["path"]).is_relative_to(state.parent)}
    paths.add(state / "artifacts.json")
    result = []
    for path in sorted(paths):
        try:
            result.append(policy.read_manifest(path))
        except (OSError, ValueError) as exc:
            result.append({"path": str(path), "data": None, "error": str(exc)})
    return result


def fresh_manifests(settings, toolkit, state):
    # A new manifest introduced since the scan must participate in the check.
    tree = toolkit.scan_tree(settings.roots["reports"])
    docs = manifests_from_inventory({"trees": [tree]}, state)
    for error in tree["errors"]:
        docs.append({"path": error["path"], "data": None, "error": error["error"]})
    return docs


def unknown_build_roots(settings, snapshot, documents):
    claims = []
    for doc in documents:
        data = doc.get("data") or {}
        roots = data.get("output_roots", {})
        if isinstance(roots, dict):
            for text in policy.walk_values(roots):
                path = policy.path_value(text)
                if path and policy.within(path, settings.roots["build"], False):
                    claims.append(path)
    groups = {}
    build = settings.roots["build"]
    for tree in snapshot["trees"]:
        if Path(tree["root"]) != build:
            continue
        for entry in tree["files"]:
            path = Path(entry["path"])
            relative = path.relative_to(build)
            top = build / relative.parts[0]
            if not any(policy.within(top, claim) or policy.within(claim, top) for claim in claims):
                group = groups.setdefault(str(top), {"path": str(top), "files": 0, "bytes": 0, "reason": "No specific owner manifest claim; preserved for review"})
                group["files"] += 1
                group["bytes"] += entry["bytes"]
    return sorted(groups.values(), key=lambda item: item["bytes"], reverse=True)


def update_current(settings, state, snapshot, deleted):
    database = output_path(settings, state / "current.sqlite3")
    output_path(settings, state / "current.sqlite3-journal")
    first = not database.exists()
    rows = {item["path"]: (item["bytes"], item["mtime_ns"]) for tree in snapshot["trees"] for item in tree["files"]}
    for item in deleted:
        rows.pop(item["path"], None)
    with closing(sqlite3.connect(database)) as connection, connection:
        connection.execute("CREATE TABLE IF NOT EXISTS current (path TEXT PRIMARY KEY, bytes INTEGER NOT NULL, mtime_ns INTEGER NOT NULL)")
        previous = {path: (size, mtime) for path, size, mtime in connection.execute("SELECT path, bytes, mtime_ns FROM current")}
        delta = {"first_baseline": first, "created": [], "modified": [], "removed": [], "current_files": len(rows)}
        if not first:
            delta["created"] = [{"path": path, "bytes": value[0], "mtime_ns": value[1]} for path, value in rows.items() if path not in previous]
            delta["modified"] = [{"path": path, "before": previous[path], "after": value} for path, value in rows.items() if path in previous and previous[path] != value]
            delta["removed"] = [{"path": path, "bytes": value[0]} for path, value in previous.items() if path not in rows]
        connection.execute("DELETE FROM current")
        connection.executemany("INSERT INTO current VALUES(?,?,?)", ((path, value[0], value[1]) for path, value in rows.items()))
    return delta


def run_once(settings, mode, skill_root, powershell, census_provider=None):
    if mode not in {"scan", "preview", "apply"}:
        raise ValueError(f"Unknown maintenance mode: {mode}")
    state = ensure_state(settings)
    try:
        lock = RunLock(output_path(settings, state / "maintenance.lock")).__enter__()
    except AlreadyRunning:
        return {"success": True, "outcome": "already-running", "deleted_files": 0, "deleted_bytes": 0}
    run_id = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ") + f"-{os.getpid()}"
    run = state / "runs" / run_id
    temporary = policy.no_alias(settings.roots["tmp"] / "artifact-maintenance" / run_id)
    ledger_path = state / "artifacts.json"
    ledger = policy.read_manifest(ledger_path)["data"]
    summary = {"schema_version": 1, "run_id": run_id, "mode": mode, "started_utc": utc(), "success": True,
               "outcome": "scanned", "eligible_files": 0, "deleted_files": 0, "deleted_bytes": 0,
               "report": str(run / "summary.json"), "errors": [], "child_processes": []}
    try:
        # This stable owner ledger registers the run directory, SQLite journal
        # and atomic-write staging before their first creation.
        ledger.update(status="active", updated_utc=utc(), last_run={"run_id": run_id, "pid": os.getpid(), "reports": str(run), "tmp": str(temporary)},
                      registered_patterns=[str(state / "baseline.json"), str(state / "current.sqlite3"), str(state / "current.sqlite3-journal"), str(run / "*"), str(temporary / "*")])
        output_path(settings, ledger_path)
        ledger_path.write_text(json.dumps(ledger, ensure_ascii=False, indent=2), encoding="utf-8")
        for directory in (run, temporary):
            policy.no_alias(directory)
            if not settings.can_write(directory):
                raise ValueError(f"Actual permission denies output directory: {directory}")
            directory.mkdir(parents=True, exist_ok=False)

        # Check durable state permissions before there can be any deletion.
        for name in ("baseline.json", "current.sqlite3", "current.sqlite3-journal"):
            output_path(settings, state / name)
        for name in ("summary.json", "selection.json", "plan.json", "delta.json", "preview.json", "cleanup.json", "cleanup.json.journal.jsonl", "preview-process.json", "cleanup-process.json", "preapply-recheck.json"):
            output_path(settings, run / name)

        def write(path, data):
            path = output_path(settings, path)
            stage = output_path(settings, temporary / (path.name + "." + uuid.uuid4().hex + ".pending"))
            stage.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
            policy.no_alias(path.parent)
            os.replace(stage, path)

        toolkit = policy.load_toolkit(skill_root)
        snapshot = inventory(settings, toolkit)
        errors = [error for tree in snapshot["trees"] for error in tree["errors"]]
        summary["scan"] = [{"root": tree["root"], "files": len(tree["files"]), "bytes": tree["bytes"], "errors": len(tree["errors"]), "reparse_points": len(tree["reparse_points"])} for tree in snapshot["trees"]]
        summary["errors"].extend(errors)
        if not (state / "baseline.json").exists():
            write(state / "baseline.json", snapshot)
        documents = manifests_from_inventory(snapshot, state)
        census_provider = census_provider or (lambda: process_census(powershell))
        census = census_provider()
        summary["process_visibility"] = {"complete": bool(census.get("complete")), "processes_observed": len(census.get("processes", [])), "inaccessible_pids": census.get("inaccessible_pids", []), "errors": census.get("errors", [])}
        selection = policy.evaluate(settings, documents, read_sessions(settings.repo), census)
        if errors:
            selection["blockers"].append("Inventory incomplete; automatic deletion disabled")
            selection["files"] = []
        summary["unknown_build_roots"] = unknown_build_roots(settings, snapshot, documents)
        summary["eligible_files"] = len(selection["files"])
        summary["rejected_files"] = len(selection["rejected"])
        summary["cleanup_blockers"] = selection["blockers"]
        write(run / "selection.json", selection)
        evidence = list(selection["evidence"])
        for path in [settings.config, *Path(__file__).parent.glob("*.py"), Path(__file__).with_name("ExecutePlan.ps1"), Path(skill_root) / "scripts/artifact_inventory.py", Path(skill_root) / "scripts/Clean-ArtifactPlan.ps1", Path(skill_root) / "scripts/GuardedFileDelete.cs"]:
            evidence.append({"path": str(path), "sha256": policy.sha256(path)})
        evidence = list({item["path"].lower(): item for item in evidence}.values())
        plan = {"schema_version": 1, "reviewed_utc": utc(), "files": selection["files"], "required_preserved_files": evidence, "prune_empty_dirs": [], "review_evidence": {"selection": str(run / "selection.json"), "permission_config_sha256": settings.config_sha256}}
        write(run / "plan.json", plan)
        deleted = []

        def guard(apply):
            name = "cleanup" if apply else "preview"
            receipt = run / f"{name}.json"
            command = [str(powershell), "-NoProfile", "-NonInteractive", "-File", str(Path(__file__).with_name("ExecutePlan.ps1")),
                       "-SkillRoot", str(skill_root), "-BuildRoot", str(settings.roots["build"]), "-TestsRoot", str(settings.roots["tests"]), "-TmpRoot", str(settings.roots["tmp"]), "-PermissionConfig", str(settings.config),
                       "-PlanPath", str(run / "plan.json"), "-ReceiptPath", str(receipt)]
            if apply:
                command.append("-Apply")
            environment = os.environ.copy()
            environment.update(TEMP=str(temporary), TMP=str(temporary), TMPDIR=str(temporary), PYTHONDONTWRITEBYTECODE="1")
            environment["PATH"] = str(Path(sys.executable).parent) + os.pathsep + environment.get("PATH", "")
            started = utc()
            child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", env=environment)
            stdout, stderr = child.communicate()
            summary["child_processes"].append({"pid": child.pid, "kind": name, "started_utc": started, "exited_utc": utc(), "exit_code": child.returncode})
            write(run / f"{name}-process.json", {"pid": child.pid, "exit_code": child.returncode, "stdout": stdout, "stderr": stderr})
            if not receipt.exists():
                raise ValueError(f"Guard did not produce a receipt ({name}); see process log")
            result = json.loads(receipt.read_text(encoding="utf-8-sig"))
            if apply:
                deleted.extend(result.get("deleted_files", []))
            if child.returncode or not result.get("success"):
                raise ValueError(f"Guard rejected {name}: {result.get('errors')}")
            return result

        if mode != "scan" and plan["files"]:
            guard(False)
            if mode == "apply":
                # Recheck new manifests, active sessions, live processes, config
                # and release fingerprints immediately before applying this plan.
                fresh_census = census_provider()
                # Preserve the configured physical category roots in fixture
                # runs; the public CLI always uses the actual five root paths.
                fresh_settings = policy.load_config(settings.config, settings.base, skill_root,
                    namespace=settings.roots["build"].relative_to(settings.base / "build") if settings.roots["build"] != settings.base / "build" else None)
                fresh = policy.evaluate(fresh_settings, fresh_manifests(settings, toolkit, state), read_sessions(settings.repo), fresh_census)
                signature = lambda value: sorted((item["path"], item["sha256"], item["bytes"], item["mtime_utc_ticks"]) for item in value["files"])
                evidence_signature = lambda value: sorted((item["path"], item["sha256"]) for item in value["evidence"])
                if signature(fresh) != signature(selection) or evidence_signature(fresh) != evidence_signature(selection) or fresh_settings.config_sha256 != settings.config_sha256 or any(policy.sha256(item["path"]) != item["sha256"] for item in evidence):
                    summary["outcome"] = "cleanup-skipped-evidence-changed"
                    write(run / "preapply-recheck.json", fresh)
                else:
                    guard(True)
                    summary["outcome"] = "cleaned"
            else:
                summary["outcome"] = "previewed"
        elif mode != "scan":
            summary["outcome"] = "scanned-no-eligible-release"
        summary["deleted_files"] = len(deleted)
        summary["deleted_bytes"] = sum(item["bytes"] for item in deleted)
        write(run / "delta.json", update_current(settings, state, snapshot, deleted))
        summary["success"] = not summary["errors"]
    except Exception as exc:
        summary["success"] = False
        summary["outcome"] = "failed"
        summary["errors"].append(str(exc))
        # A partially completed native batch is reported from its actual receipt.
        receipt = run / "cleanup.json"
        if receipt.exists():
            actual = json.loads(receipt.read_text(encoding="utf-8-sig"))
            summary["deleted_files"] = len(actual.get("deleted_files", []))
            summary["deleted_bytes"] = actual.get("deleted_bytes", 0)
    finally:
        try:
            summary["finished_utc"] = utc()
            if temporary.exists():
                remaining = policy.load_toolkit(skill_root).scan_tree(temporary)
                summary["retained_tmp"] = remaining["files"] + remaining["errors"] + remaining["reparse_points"]
                if not summary["retained_tmp"]:
                    temporary.rmdir()
                else:
                    summary["success"] = False
                    ledger["unknown"] = [{"path": str(temporary), "reason": "Unexpected owned temporary output retained for review"}]
            ledger.update(updated_utc=utc(), last_result=summary["report"], processes=[], cleanup={"receipt": str(run / "cleanup.json") if (run / "cleanup.json").exists() else None, "deleted_files": summary["deleted_files"], "deleted_bytes": summary["deleted_bytes"]})
            if ledger.get("unknown"):
                ledger["status"] = "needs-review"
            # These are owned durable summaries. No foreign manifest is edited.
            if run.is_dir():
                output_path(settings, run / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
            output_path(settings, ledger_path).write_text(json.dumps(ledger, ensure_ascii=False, indent=2), encoding="utf-8")
        finally:
            lock.__exit__(None, None, None)
    return summary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("scan", "preview", "apply"), default="scan")
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--skill-root", type=Path, default=policy.DEFAULT_SKILL)
    parser.add_argument("--powershell", type=Path, default=Path("pwsh.exe"))
    args = parser.parse_args(argv)
    try:
        settings = policy.load_config(args.repo / ".codex/config.toml", skill_root=args.skill_root)
        result = run_once(settings, args.mode, args.skill_root, args.powershell)
        print(json.dumps({key: result.get(key) for key in ("success", "outcome", "eligible_files", "deleted_files", "deleted_bytes", "report", "errors")}, ensure_ascii=False))
        return 0 if result["success"] else 1
    except (OSError, ValueError) as exc:
        print(json.dumps({"success": False, "error": str(exc)}, ensure_ascii=False), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
