"""Safety acceptance for explicitly released output; fixtures stay on E:.

No production source tree is copied. Each synthetic file is registered before
creation and disposed using its explicit path and verified SHA256.
"""
from __future__ import annotations

import datetime as dt
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import unittest

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "scripts/artifacts"))
import policy  # noqa: E402 -- missing implementation is the initial red check

OUTPUT = Path("E:/cargo-targets/zr_vm")
REPORT = OUTPUT / "reports/artifact-maintenance/policy-validation"
FIXTURES = OUTPUT / "tests/artifact-maintenance/policy-fixtures"
FIXTURE_ROOTS = {key: OUTPUT / key / "artifact-maintenance/policy-fixtures" for key in policy.CATEGORIES}
SKILL = Path("D:/CodexData/.codex/skills/artifact-lifecycle")
POWERSHELL = Path(os.environ.get("ARTIFACT_TEST_PWSH", "C:/Users/HeJiahui/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/pwsh.exe"))


class ReleasePolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        REPORT.mkdir(parents=True, exist_ok=True)
        cls.case_number = 0
        cls.ledger = {
            "schema_version": 1, "task_id": "artifact-maintenance-policy-validation",
            "owner": "artifact-maintenance unittest process", "status": "active",
            "source_root": str(REPO), "output_roots": {**{key: str(path) for key, path in FIXTURE_ROOTS.items()}, "validation_reports": str(REPORT)},
            "processes": [], "retained": [], "unknown": [], "registered_files": [], "cleanup": [],
        }
        cls.save_ledger()

    @classmethod
    def save_ledger(cls):
        (REPORT / "artifacts.json").write_text(json.dumps(cls.ledger, indent=2), encoding="utf-8")

    @classmethod
    def tearDownClass(cls):
        for root in FIXTURE_ROOTS.values():
            if root.exists():
                tree = policy.load_toolkit().scan_tree(root)
                cls.ledger["unknown"].extend({"path": item["path"], "reason": "Fixture teardown did not finish"} for item in tree["files"] + tree["errors"] + tree["reparse_points"])
        cls.ledger["status"] = "closed" if not cls.ledger["unknown"] else "needs-review"
        cls.ledger["retained"] = [{"path": str(REPORT), "kind": "validation-evidence", "reason": "Finite safety test receipts", "depends_on": [], "review_after": None}]
        cls.save_ledger()
        for path in FIXTURE_ROOTS.values():
            if path.exists() and not list(path.iterdir()):
                path.rmdir()

    def setUp(self):
        type(self).case_number += 1
        case = f"{os.getpid()}-{type(self).case_number:03d}"
        self.namespace = Path("artifact-maintenance/policy-fixtures") / case
        self.roots = {key: path / case for key, path in FIXTURE_ROOTS.items()}
        self.root = self.roots["tests"]
        self.assertFalse(self.root.exists(), "Previous fixture requires review")
        self.files = []
        self.links = []
        self.ledger["registered_files"].extend({"path": str(path), "kind": "synthetic-fixture-root", "reason": self.id()} for path in self.roots.values())
        self.save_ledger()
        for path in self.roots.values():
            path.mkdir(parents=True)
        self.base = OUTPUT
        self.config = self.root / "checkout/.codex/config.toml"
        self.write(self.config, ('default_permissions="test"\n[permissions.test.filesystem]\n' + '\n'.join(f'"{path.as_posix()}"="write"' for path in self.roots.values()) + '\n').encode())
        (self.config.parent / "sessions").mkdir()
        self.settings = policy.load_config(self.config, self.base, namespace=self.namespace)
        self.file = self.write(self.roots["build"] / "owner/stale.obj", b"synthetic compiled output\x00")
        self.manifest = {
            "schema_version": 1, "task_id": "finished-owner", "owner": "explicit fixture owner", "status": "closed",
            "source_root": str(self.config.parent.parent),
            "output_roots": {"build": str(self.file.parent)},
            "processes": [], "unknown": [], "retained": [], "cleanup_candidates": [self.release(self.file)],
        }
        self.manifest_path = self.roots["reports"] / "finished-owner/artifacts.json"
        self.census = {"complete": True, "processes": [], "errors": []}

    def write(self, path, content):
        path = Path(path)
        self.ledger["registered_files"].append({"path": str(path), "kind": "synthetic-file", "reason": self.id()})
        self.save_ledger()
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        if path not in self.files:
            self.files.append(path)
        return path

    def release(self, path):
        st = path.stat()
        return {"path": str(path), "kind": "compiled-output", "reason": "Fixture owner explicitly releases regenerated output",
                "released_at": dt.datetime.now(dt.timezone.utc).isoformat(), "not_before": dt.datetime.now(dt.timezone.utc).isoformat(),
                "bytes": st.st_size, "mtime_utc_ticks": st.st_mtime_ns // 100 + 621355968000000000,
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}

    def evaluate(self, extra=(), sessions=()):
        self.write(self.manifest_path, json.dumps(self.manifest).encode())
        docs = [policy.read_manifest(self.manifest_path)]
        for index, data in enumerate(extra):
            path = self.write(self.roots["reports"] / f"extra-{index}/artifacts.json", json.dumps(data).encode())
            docs.append(policy.read_manifest(path))
        return policy.evaluate(self.settings, docs, list(sessions), self.census)

    def assert_rejected(self, result, reason):
        self.assertEqual(result["files"], [])
        self.assertIn(reason, json.dumps([item["reason"] for item in result["rejected"]] + result["blockers"]).lower())
        self.assertTrue(self.file.exists())

    def tearDown(self):
        # run_once pre-registers these exact namespaces before creating them.
        for namespace in (self.roots["reports"] / "artifact-maintenance", self.roots["tmp"] / "artifact-maintenance"):
            if namespace.exists():
                tree = policy.load_toolkit().scan_tree(namespace)
                self.assertFalse(tree["errors"])
                self.assertFalse(tree["reparse_points"])
                for item in tree["files"]:
                    path = Path(item["path"])
                    if path.name in {"summary.json", "cleanup.json", "preview.json", "preapply-recheck.json"}:
                        self.ledger.setdefault("guard_evidence", []).append({"test": self.id(), "path": str(path), "data": json.loads(path.read_text(encoding="utf-8-sig"))})
                    if path not in self.files:
                        self.files.append(path)
        plan = []
        for path in self.files:
            if path.exists():
                policy.no_alias(path)
                self.assertTrue(any(path.is_relative_to(root) for root in self.roots.values()))
                plan.append({"path": str(path), "bytes": path.stat().st_size, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
        self.ledger["cleanup"].append({"test": self.id(), "plan": plan, "deleted_files": 0, "deleted_bytes": 0})
        self.save_ledger()
        receipt = self.ledger["cleanup"][-1]
        for item in plan:
            path = Path(item["path"])
            policy.no_alias(path)
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), item["sha256"])
            path.unlink()
            receipt["deleted_files"] += 1
            receipt["deleted_bytes"] += item["bytes"]
        for link in self.links:
            self.assertTrue(bool(link.lstat().st_file_attributes & 0x400))
            os.rmdir(link)
        directories = []
        for root in self.roots.values():
            for directory, children, _ in os.walk(root, followlinks=False):
                directories.append(Path(directory))
        for path in sorted(directories, key=lambda p: len(p.parts), reverse=True):
            policy.no_alias(path)
            path.rmdir()
        self.save_ledger()

    def test_explicit_closed_unchanged_release_is_selected(self):
        result = self.evaluate()
        self.assertEqual([item["path"] for item in result["files"]], [str(self.file)])
        self.assertEqual(result["blockers"], [])

    def test_closed_without_explicit_release_does_not_select(self):
        self.manifest["cleanup_candidates"] = []
        self.assertEqual(self.evaluate()["files"], [])

    def test_modified_fingerprint_is_rejected(self):
        self.file.write_bytes(b"rebuilt output")
        self.assert_rejected(self.evaluate(), "fingerprint")

    def test_unknown_owner_is_rejected(self):
        self.manifest["owner"] = "unknown"
        self.assert_rejected(self.evaluate(), "owner")

    def test_release_requires_versioned_manifest_schema(self):
        del self.manifest["schema_version"]
        self.assert_rejected(self.evaluate(), "schema")

    def test_conflicting_closed_owner_claim_is_rejected(self):
        other = {"schema_version": 1, "task_id": "different-owner", "owner": "another explicit owner", "status": "closed", "output_roots": {"build": str(self.file.parent)}}
        self.assert_rejected(self.evaluate([other]), "ownership")

    def test_active_owner_is_rejected(self):
        self.manifest["status"] = "active"
        self.assert_rejected(self.evaluate(), "closed")

    def test_retained_parent_blocks_even_for_closed_task(self):
        self.manifest["retained"] = [{"path": str(self.file.parent), "depends_on": []}]
        self.assert_rejected(self.evaluate(), "retained")

    def test_active_task_dependency_blocks_closed_owner(self):
        other = {"task_id": "consumer", "owner": "another owner", "status": "active", "depends_on": ["finished-owner"]}
        self.assert_rejected(self.evaluate([other]), "depend")

    def test_closed_nested_delivery_dependency_is_protected(self):
        other = {"task_id": "delivered-consumer", "owner": "delivery owner", "status": "closed",
                 "artifacts": [{"path": str(self.roots["reports"] / "delivery.bin"), "dependencies": [{"path": str(self.file)}]}]}
        self.assert_rejected(self.evaluate([other]), "depend")

    def test_dependency_owner_metadata_is_not_a_dependency_reference(self):
        other = {"task_id": "unrelated-consumer", "owner": "another owner", "status": "active",
                 "artifacts": [{"dependencies": [{"path": str(self.roots["build"] / "unrelated.obj"), "owner": "descriptive owner metadata"}]}]}
        self.assertEqual(len(self.evaluate([other])["files"]), 1)

    def test_generic_output_scope_is_not_exclusive_ownership(self):
        other = {"task_id": "reader", "owner": "another owner", "status": "active", "output_roots": {"build": str(self.roots["build"])}}
        self.assertEqual(len(self.evaluate([other])["files"]), 1)

    def test_active_task_specific_output_root_blocks(self):
        other = {"task_id": "consumer", "owner": "another owner", "status": "active", "output_roots": {"build": str(self.file.parent)}}
        self.assert_rejected(self.evaluate([other]), "active")

    def test_unfinished_owner_process_blocks(self):
        self.manifest["processes"] = [{"pid": 123, "state": "running"}]
        self.assert_rejected(self.evaluate(), "process")

    def test_incomplete_process_census_disables_deletion(self):
        self.census = {"complete": False, "processes": [], "errors": ["visibility denied"]}
        self.assert_rejected(self.evaluate(), "process")

    def test_live_process_command_blocks(self):
        self.census["processes"] = [{"pid": 123, "command_line": f'link.exe /OUT:"{self.file}"', "executable_path": "E:/tools/link.exe"}]
        self.assert_rejected(self.evaluate(), "process")

    def test_active_session_build_reference_blocks(self):
        session = {"path": "active-session.md", "text": f"---\nstatus: active\n---\nbuild: {self.file.parent.as_posix()}"}
        self.assert_rejected(self.evaluate(sessions=[session]), "session")

    def test_active_frontmatter_overrides_historical_closed_example(self):
        session = {"path": "active-session.md", "text": f"---\nstatus: active\n---\nbuild: {self.file.parent.as_posix()}\n```yaml\nstatus: closed\n```"}
        self.assert_rejected(self.evaluate(sessions=[session]), "session")

    def test_ambiguous_frontmatter_stays_active(self):
        session = {"path": "ambiguous.md", "text": f"---\nstatus: closed\nstatus: 'active'\n---\nbuild: {self.file.parent.as_posix()}"}
        self.assert_rejected(self.evaluate(sessions=[session]), "session")

    def test_closed_frontmatter_ignores_body_active_example(self):
        session = {"path": "closed.md", "text": f"---\nstatus: closed\n---\nbuild: {self.file.parent.as_posix()}\nstatus: active"}
        self.assertEqual(len(self.evaluate(sessions=[session])["files"]), 1)

    def test_future_release_date_blocks(self):
        self.manifest["cleanup_candidates"][0]["not_before"] = "2099-01-01T00:00:00+00:00"
        self.assert_rejected(self.evaluate(), "time")

    def test_source_cache_and_git_files_are_never_selected(self):
        paths = [self.roots["build"] / "owner/source.c", self.roots["cache"] / "owner/cache.obj", self.roots["build"] / "owner/.git/index.obj"]
        for path in paths:
            self.write(path, b"synthetic protected content")
        self.manifest["cleanup_candidates"] = [self.release(path) for path in paths]
        self.assertEqual(self.evaluate()["files"], [])

    def test_config_child_deny_is_respected(self):
        with self.config.open("a", encoding="utf-8") as out:
            out.write(f'"{self.file.parent.as_posix()}"="read"\n')
        self.settings = policy.load_config(self.config, self.base, namespace=self.namespace)
        self.assert_rejected(self.evaluate(), "permission")

    def test_owner_cannot_release_an_unowned_sibling(self):
        self.manifest["output_roots"]["build"] = str(self.roots["build"] / "different-owner")
        self.assert_rejected(self.evaluate(), "ownership")

    def test_junction_candidate_is_rejected(self):
        link = self.roots["build"] / "alias"
        command = "$ErrorActionPreference='Stop'; New-Item -ItemType Junction -Path $args[0] -Target $args[1] | Out-Null"
        result = subprocess.run([str(POWERSHELL), "-NoProfile", "-Command", command, str(link), str(self.file.parent)], capture_output=True, text=True)
        # Powershell -Command positional passing differs from -File; use a fixed,
        # locally generated script if this host does not bind $args as expected.
        if result.returncode:
            script = self.write(self.root / "junction.ps1", b"param($Link,$Target)\n$ErrorActionPreference='Stop'\nNew-Item -ItemType Junction -Path $Link -Target $Target | Out-Null\n")
            result = subprocess.run([str(POWERSHELL), "-NoProfile", "-File", str(script), str(link), str(self.file.parent)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.links.append(link)
        self.manifest["cleanup_candidates"][0]["path"] = str(link / self.file.name)
        self.manifest["output_roots"]["build"] = str(link)
        self.assert_rejected(self.evaluate(), "reparse")

    def run_maintenance(self, mode="apply", **kwargs):
        import maintenance
        self.write(self.manifest_path, json.dumps(self.manifest).encode())
        return maintenance.run_once(self.settings, mode, SKILL, POWERSHELL,
                                    census_provider=lambda: self.census, **kwargs)

    def test_apply_deletes_only_released_file_and_reuses_current_state(self):
        sibling = self.write(self.file.parent / "kept.obj", b"synthetic kept compiled output")
        before = hashlib.sha256(sibling.read_bytes()).hexdigest()
        preview = self.run_maintenance("preview")
        self.assertTrue(self.file.exists())
        self.assertEqual(preview["eligible_files"], 1)
        first = self.run_maintenance()
        self.assertTrue(first["success"], first)
        self.assertEqual(first["deleted_files"], 1)
        self.assertEqual(first["deleted_bytes"], len(b"synthetic compiled output\x00"))
        self.assertFalse(self.file.exists())
        self.assertEqual(hashlib.sha256(sibling.read_bytes()).hexdigest(), before)
        second = self.run_maintenance()
        self.assertTrue(second["success"], second)
        self.assertEqual(second["deleted_files"], 0)
        state = self.roots["reports"] / "artifact-maintenance"
        self.assertTrue((state / "baseline.json").exists())
        self.assertTrue((state / "current.sqlite3").exists())
        self.assertEqual(len(list(state.glob("baseline*"))), 1)
        self.assertEqual(list((self.roots["tmp"] / "artifact-maintenance").iterdir()), [])

    def test_task_local_lock_rejects_overlap(self):
        import maintenance
        self.run_maintenance("scan")
        with maintenance.RunLock(self.roots["reports"] / "artifact-maintenance/maintenance.lock"):
            result = self.run_maintenance()
        self.assertEqual(result["outcome"], "already-running")
        self.assertTrue(self.file.exists())

    def test_denied_state_artifact_stops_before_deletion(self):
        database = self.roots["reports"] / "artifact-maintenance/current.sqlite3"
        with self.config.open("a", encoding="utf-8") as out:
            out.write(f'"{database.as_posix()}"="read"\n')
        self.settings = policy.load_config(self.config, self.base, namespace=self.namespace)
        result = self.run_maintenance()
        self.assertFalse(result["success"], result)
        self.assertFalse(database.exists())
        self.assertTrue(self.file.exists())

    def test_occupied_candidate_fails_without_deletion(self):
        import ctypes
        from ctypes import wintypes
        create = ctypes.WinDLL("kernel32", use_last_error=True).CreateFileW
        create.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
        create.restype = wintypes.HANDLE
        close = ctypes.WinDLL("kernel32", use_last_error=True).CloseHandle
        close.argtypes = [wintypes.HANDLE]
        handle = create(str(self.file), 0x80000000, 1, None, 3, 0, None)
        self.assertNotEqual(handle, wintypes.HANDLE(-1).value)
        try:
            result = self.run_maintenance()
        finally:
            close(handle)
        self.assertFalse(result["success"], result)
        self.assertEqual(result["deleted_files"], 0)
        self.assertTrue(self.file.exists())

    def test_changed_ownership_between_preview_and_apply_fails_closed(self):
        calls = []
        def census():
            calls.append(1)
            if len(calls) > 1:
                self.manifest["status"] = "active"
                self.write(self.manifest_path, json.dumps(self.manifest).encode())
            return self.census
        import maintenance
        self.write(self.manifest_path, json.dumps(self.manifest).encode())
        result = maintenance.run_once(self.settings, "apply", SKILL, POWERSHELL, census_provider=census)
        self.assertEqual(result["deleted_files"], 0)
        self.assertIn("changed", json.dumps(result).lower())
        self.assertTrue(self.file.exists())


if __name__ == "__main__":
    unittest.main()
