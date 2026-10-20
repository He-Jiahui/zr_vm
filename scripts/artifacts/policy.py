"""Conservative release policy for zr_vm output maintenance (no deletions)."""
from __future__ import annotations

from dataclasses import dataclass
import datetime as dt
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import tomllib

OUTPUT_ROOT = Path("E:/cargo-targets/zr_vm")
DEFAULT_SKILL = Path("D:/CodexData/.codex/skills/artifact-lifecycle")
CATEGORIES = ("build", "tests", "reports", "cache", "tmp")
DELETABLE = ("build", "tests", "tmp")
COMPILED_SUFFIXES = frozenset(".o .obj .exe .dll .lib .a .so .pdb .ilk .exp .d .pch .gch .idb .ipdb .iobj .res .wasm".split())
DATA_SUFFIXES = frozenset(".tmp .bin .dat .out".split())
KINDS = {"compiled-output": COMPILED_SUFFIXES, "generated-fixture": DATA_SUFFIXES, "temporary-file": DATA_SUFFIXES}
UNKNOWN_OWNERS = {"", "unknown", "unknown-owner", "tbd", "none", "null", "legacy", "unassigned"}
TICKS_AT_UNIX_EPOCH = 621355968000000000


def canonical(value):
    text = str(value)
    if not re.match(r"^[A-Za-z]:[\\/]", text) or ":" in text[2:]:
        raise ValueError(f"Absolute drive path without ADS required: {text}")
    return Path(os.path.abspath(text))


def is_reparse(st):
    return bool(getattr(st, "st_file_attributes", 0) & 0x400) or stat.S_ISLNK(st.st_mode)


def no_alias(value):
    path = canonical(value)
    for node in (path, *path.parents):
        try:
            if is_reparse(node.lstat()):
                raise ValueError(f"Reparse/symlink path is protected: {node}")
        except FileNotFoundError:
            pass
    return path


def within(path, root, equal=True):
    return (equal or path != root) and path.is_relative_to(root)


def sha256(path):
    digest = hashlib.sha256()
    with no_alias(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_toolkit(skill_root=DEFAULT_SKILL):
    source = no_alias(Path(skill_root) / "scripts/artifact_inventory.py")
    spec = importlib.util.spec_from_file_location("artifact_inventory_toolkit", source)
    if not spec or not spec.loader:
        raise ValueError(f"Cannot load installed lifecycle toolkit: {source}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@dataclass
class Settings:
    config: Path
    config_sha256: str
    repo: Path
    base: Path
    roots: dict
    profile: str
    permissions: list

    def can_write(self, path):
        path = canonical(path)
        for root, access in self.permissions:
            if within(path, root):
                return access == "write"
        return False


def load_config(config, output_root=OUTPUT_ROOT, skill_root=DEFAULT_SKILL, namespace=None):
    config = no_alias(config)
    raw = config.read_bytes()
    toolkit = load_toolkit(skill_root)
    profile, scopes = toolkit.effective_permissions(tomllib.loads(raw.decode("utf-8-sig")), None)
    permissions = sorted(((canonical(key), access) for key, access in scopes.items() if not key.startswith(":")), key=lambda row: len(str(row[0])), reverse=True)
    base = no_alias(output_root)
    if not re.match(r"^[DEF]:\\cargo-targets\\", str(base), re.I):
        raise ValueError(f"Physical drive-root cargo-targets output required: {base}")
    if namespace is not None and (Path(namespace).is_absolute() or ".." in Path(namespace).parts):
        raise ValueError("Fixture namespace must stay below configured output categories")
    roots = {key: base / key / namespace if namespace is not None else base / key for key in CATEGORIES}
    result = Settings(config, hashlib.sha256(raw).hexdigest(), config.parent.parent, base, roots, profile, permissions)
    for path in result.roots.values():
        no_alias(path)
        if not result.can_write(path):
            raise ValueError(f"Actual configuration denies output root permission: {path}")
    return result


def read_manifest(path):
    path = no_alias(path)
    try:
        raw = path.read_bytes()
        data = json.loads(raw.decode("utf-8-sig"))
        if not isinstance(data, dict):
            raise ValueError("Expected a manifest object")
        return {"path": str(path), "sha256": hashlib.sha256(raw).hexdigest(), "data": data}
    except (OSError, ValueError) as exc:
        return {"path": str(path), "error": str(exc), "data": None}


def timestamp(value):
    if not isinstance(value, str):
        raise ValueError("Release time must be an ISO timestamp")
    result = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
    if result.tzinfo is None:
        raise ValueError("Release time requires a timezone")
    return result.astimezone(dt.timezone.utc)


def fingerprint(path):
    path = no_alias(path)
    before = path.lstat()
    if not stat.S_ISREG(before.st_mode) or before.st_nlink != 1:
        raise ValueError(f"Expected an unaliased single-link regular output file: {path}")
    digest = sha256(path)
    after = path.lstat()
    identity = lambda item: (item.st_ino, item.st_size, item.st_mtime_ns)
    if identity(before) != identity(after):
        raise ValueError(f"Fingerprint changed while reading: {path}")
    return {"path": str(path), "bytes": before.st_size,
            "mtime_utc_ticks": before.st_mtime_ns // 100 + TICKS_AT_UNIX_EPOCH, "sha256": digest}


def walk_values(value):
    if isinstance(value, dict):
        for child in value.values():
            yield from walk_values(child)
    elif isinstance(value, list):
        for child in value:
            yield from walk_values(child)
    elif isinstance(value, str):
        yield value


def path_value(value):
    if isinstance(value, str) and re.match(r"^[A-Za-z]:[\\/]", value):
        try:
            return canonical(value)
        except ValueError:
            pass
    return None


def normalized_text(text):
    return text.lower().replace("\\\\", "\\").replace("\\", "/")


def references_path(text, path, scope):
    """Match an exact file or a specific ancestor; global roots are scopes."""
    text = normalized_text(text)
    for node in (path, *path.parents):
        if node == scope or not node.is_relative_to(scope):
            break
        escaped = re.escape(node.as_posix().lower())
        if re.search(escaped + r"(?=$|[/\s\"'`\]\[),;:])", text):
            return True
    return False


def active_session(session):
    lines = session.get("text", "").lstrip("\ufeff").splitlines()
    if not lines or lines[0].strip() != "---":
        return True
    try:
        end = lines.index("---", 1)
    except ValueError:
        return True
    # Ambiguous/missing metadata remains active. Body examples never control
    # whether an owner session can release another task's output.
    status_lines = [line for line in lines[1:end] if re.match(r"status[ \t]*:", line)]
    if len(status_lines) != 1:
        return True
    status = re.fullmatch(r"status:[ \t]*([a-zA-Z-]+)[ \t]*(?:#.*)?", status_lines[0])
    return status is None or status.group(1).lower() not in {"completed", "closed", "archived"}


def dependency_leaves(value):
    if isinstance(value, dict):
        if "path" in value:
            yield value["path"] if isinstance(value["path"], str) else "<invalid dependency path>"
        elif "task_id" in value:
            yield value["task_id"] if isinstance(value["task_id"], str) else "<invalid dependency task>"
        else:
            for child in value.values():
                yield from dependency_leaves(child)
    elif isinstance(value, list):
        for child in value:
            yield from dependency_leaves(child)
    elif isinstance(value, str):
        yield value


def declared_dependencies(value):
    if isinstance(value, dict):
        for key, child in value.items():
            if key in {"depends_on", "dependencies"}:
                yield from dependency_leaves(child)
            yield from declared_dependencies(child)
    elif isinstance(value, list):
        for child in value:
            yield from declared_dependencies(child)


def evaluate(settings, documents, sessions, census, now=None):
    """Produce a plan only from explicit releases; unknown data fails closed."""
    now = now or dt.datetime.now(dt.timezone.utc)
    result = {"files": [], "rejected": [], "blockers": [], "protected": [], "evidence": []}
    protections, claims, task_dependencies, uncertain = [], [], set(), []
    manifests = [doc["data"] for doc in documents if doc.get("data") is not None]
    task_ids = {data.get("task_id") for data in manifests if isinstance(data.get("task_id"), str)}

    def protect(value, reason):
        for item in walk_values(value):
            path = path_value(item)
            if path:
                protections.append((path, reason))

    for doc in documents:
        if doc.get("error"):
            result["blockers"].append(f"Unreadable ownership manifest: {doc['path']}: {doc['error']}")
            continue
        result["evidence"].append({"path": doc["path"], "sha256": doc["sha256"]})
        data = doc["data"]
        if isinstance(data.get("output_roots"), dict):
            for item in walk_values(data["output_roots"]):
                path = path_value(item)
                if path and path not in settings.roots.values() and path != settings.base:
                    claims.append((path, data.get("task_id"), data.get("owner")))
        protect(data.get("retained", []), "retained output or dependency")
        protect(data.get("unknown", []), "unknown ownership is protected")
        # Retained deliveries still need their nested path dependencies after
        # owner closure (including legacy artifacts[].dependencies objects).
        for item in declared_dependencies({key: value for key, value in data.items() if key != "cleanup_candidates"}):
            path = path_value(item)
            if path:
                protections.append((path, "declared output dependency"))
            elif item in task_ids:
                task_dependencies.add(item)
            elif data.get("status") != "closed" and item.strip():
                uncertain.append(f"Unresolved active dependency in {doc['path']}: {item}")
        if data.get("status") != "closed":
            roots = data.get("output_roots", {})
            if isinstance(roots, dict):
                for item in walk_values(roots):
                    path = path_value(item)
                    if path and path not in settings.roots.values() and path != settings.base:
                        protections.append((path, "active task output root"))

    for session in sessions:
        if session.get("error"):
            result["blockers"].append(f"Unreadable active session: {session['path']}")
        elif session.get("sha256"):
            result["evidence"].append({"path": session["path"], "sha256": session["sha256"]})
    result["protected"] = [{"path": str(path), "reason": reason} for path, reason in protections]
    candidates = []
    for doc in documents:
        if not doc.get("data"):
            continue
        entries = doc["data"].get("cleanup_candidates", [])
        if not isinstance(entries, list):
            result["blockers"].append(f"Malformed cleanup_candidates: {doc['path']}")
            continue
        candidates.extend((doc, item) for item in entries)
    if candidates:
        result["blockers"].extend(uncertain)
        if census.get("complete") is not True:
            result["blockers"].append("Process visibility incomplete; cleanup disabled")
    seen = set()
    for doc, item in candidates:
        label = item.get("path", "<missing>") if isinstance(item, dict) else "<malformed>"
        try:
            if not isinstance(item, dict):
                raise ValueError("Malformed cleanup candidate")
            data = doc["data"]
            if type(data.get("schema_version")) is not int or data["schema_version"] != 1:
                raise ValueError("Release requires schema_version=1 owner manifest")
            if data.get("status") != "closed":
                raise ValueError("Owner task is not closed")
            owner, task_id = data.get("owner"), data.get("task_id")
            if not isinstance(owner, str) or owner.strip().lower() in UNKNOWN_OWNERS or not isinstance(task_id, str) or task_id.strip().lower() in UNKNOWN_OWNERS:
                raise ValueError("Explicit known owner and task_id required")
            if data.get("unknown"):
                raise ValueError("Closed owner still has unknown artifacts")
            if canonical(data.get("source_root", "")) != settings.repo:
                raise ValueError("Owner source_root differs from this checkout")
            for proc in data.get("processes", []):
                if not isinstance(proc, dict) or not (proc.get("exited_utc") or proc.get("exit_code") is not None or proc.get("state") == "exited"):
                    raise ValueError("Owner process exit is unconfirmed")
            path = no_alias(item["path"])
            category = next((key for key in DELETABLE if within(path, settings.roots[key], False)), None)
            if not category:
                raise ValueError("Outside automatic build/tests/tmp cleanup scope")
            if any(part.lower() in {".git", ".svn", ".hg"} for part in path.parts):
                raise ValueError("Git/version-control metadata is protected")
            if not settings.can_write(path):
                raise ValueError("Actual permission configuration denies file writing")
            roots = data.get("output_roots", {})
            owned = path_value(roots.get(category)) if isinstance(roots, dict) else None
            if not owned or not within(owned, settings.roots[category], False) or not within(path, owned, False):
                raise ValueError("Specific task output ownership is required")
            no_alias(owned)
            for protected, reason in protections:
                if within(path, protected):
                    raise ValueError(reason)
            for claimed, claiming_task, claiming_owner in claims:
                if within(path, claimed) and (claiming_task != task_id or claiming_owner != owner):
                    raise ValueError("Conflicting output ownership claim; review required")
            allowed = KINDS.get(item.get("kind"), ())
            if path.suffix.lower() not in allowed:
                raise ValueError("Source, config, evidence or unrecognized output kind is protected")
            if not isinstance(item.get("reason"), str) or not item["reason"].strip():
                raise ValueError("Explicit release reason required")
            if type(item.get("bytes")) is not int or item["bytes"] < 0 or type(item.get("mtime_utc_ticks")) is not int or not isinstance(item.get("sha256"), str) or not re.fullmatch(r"[0-9a-f]{64}", item["sha256"]):
                raise ValueError("Complete typed release fingerprint required")
            released, earliest = timestamp(item.get("released_at")), timestamp(item.get("not_before"))
            if released > now or earliest > now or earliest < released:
                raise ValueError("Release time window is not eligible")
            if task_id in task_dependencies:
                raise ValueError("Active task depends on this owner")
            for session in sessions:
                if active_session(session) and (references_path(session.get("text", ""), path, settings.roots[category]) or re.search(r"(?<![\w-])" + re.escape(task_id) + r"(?![\w-])", session.get("text", ""))):
                    raise ValueError(f"Active session reference: {session['path']}")
            for proc in census.get("processes", []):
                text = proc.get("command_line", "") or ""
                text += " " + (proc.get("executable_path", "") or "")
                if references_path(text, path, settings.roots[category]):
                    raise ValueError(f"Live process reference: PID {proc.get('pid')}")
            actual = fingerprint(path)
            for key in ("bytes", "mtime_utc_ticks", "sha256"):
                if item.get(key) != actual[key]:
                    raise ValueError(f"Release fingerprint mismatch: {key}")
            if str(path).lower() in seen:
                raise ValueError("Duplicate release ownership; review required")
            seen.add(str(path).lower())
            actual.update(reason=item["reason"], owner=owner, task_id=task_id, manifest=doc["path"])
            result["files"].append(actual)
        except (OSError, ValueError, KeyError, TypeError) as exc:
            result["rejected"].append({"path": str(label), "reason": str(exc)})
    # Any ambiguity invalidates the entire batch, including a duplicate claim.
    duplicates = {str(item["path"]).lower() for item in result["rejected"] if "Duplicate release" in item["reason"]}
    if duplicates:
        result["files"] = [item for item in result["files"] if item["path"].lower() not in duplicates]
    if result["blockers"]:
        for item in result["files"]:
            result["rejected"].append({"path": item["path"], "reason": "Cleanup blocked by incomplete ownership/dependency/process evidence"})
        result["files"] = []
    return result
