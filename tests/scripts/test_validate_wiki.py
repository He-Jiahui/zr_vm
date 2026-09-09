import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = REPO_ROOT / "scripts" / "validate_wiki.py"
SPEC = importlib.util.spec_from_file_location("validate_wiki", SCRIPT_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot load {SCRIPT_PATH}")
validate_wiki = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validate_wiki)


def write_document(path: Path, body: str, *, title: str = "Test page") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "---\n"
        "related_code: []\n"
        "implementation_files: []\n"
        "plan_sources: []\n"
        "tests: []\n"
        "doc_type: test\n"
        "---\n\n"
        f"# {title}\n\n"
        f"{body}\n",
        encoding="utf-8",
    )


def write_manifest(root: Path, paths: list[str]) -> None:
    manifest = {
        "schema": 1,
        "title": "Test Wiki",
        "sections": [],
        "pages": [
            {"id": path.replace("/", "-").removesuffix(".md"), "path": path, "kind": "guide", "parent": None}
            for path in paths
        ],
    }
    (root / "docs/wiki").mkdir(parents=True, exist_ok=True)
    (root / "docs/wiki/manifest.json").write_text(
        json.dumps(manifest), encoding="utf-8"
    )


class WikiValidationTests(unittest.TestCase):
    def test_repository_wiki_is_valid(self) -> None:
        result = validate_wiki.validate(REPO_ROOT)

        self.assertEqual(result.errors, ())
        self.assertEqual(result.markdown_files, 47)
        self.assertEqual(result.manifest_pages, 46)
        self.assertGreaterEqual(result.local_links, 100)

    def test_missing_manifest_page_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_document(root / "docs/wiki/index.md", "A valid page.")
            write_manifest(root, ["index.md", "missing.md"])

            result = validate_wiki.validate(root)

            self.assertTrue(
                any("manifest page target does not exist" in error for error in result.errors)
            )

    def test_missing_required_front_matter_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs = root / "docs/wiki"
            docs.mkdir(parents=True)
            (docs / "index.md").write_text(
                "---\nrelated_code: []\n---\n\n# Incomplete\n", encoding="utf-8"
            )
            write_manifest(root, ["index.md"])

            result = validate_wiki.validate(root)

            self.assertTrue(any("missing front matter field" in error for error in result.errors))

    def test_local_link_and_anchor_failures_are_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_document(
                root / "docs/wiki/index.md",
                "[missing](missing.md) and [bad anchor](target.md#does-not-exist) "
                "and [local anchor](#does-not-exist).\n\n"
                "```markdown\n[fenced](missing-from-example.md)\n```",
            )
            write_document(root / "docs/wiki/target.md", "A target.", title="Target")
            write_manifest(root, ["index.md", "target.md"])

            result = validate_wiki.validate(root)

            self.assertTrue(any("missing link target" in error for error in result.errors))
            self.assertTrue(any("missing link anchor" in error for error in result.errors))
            self.assertFalse(any("missing-from-example.md" in error for error in result.errors))

    def test_site_output_requires_index(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_document(root / "docs/wiki/index.md", "A valid page.")
            write_manifest(root, ["index.md"])
            site_dir = root / "site"
            site_dir.mkdir()

            result = validate_wiki.validate(root, site_dir=site_dir)

            self.assertTrue(any("site output missing index.html" in error for error in result.errors))


if __name__ == "__main__":
    unittest.main(verbosity=2)
