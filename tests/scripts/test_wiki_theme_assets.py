"""Contract tests for the Wiki handbook and rendered theme assets."""

from __future__ import annotations

import json
import tomllib
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
WIKI = ROOT / "docs" / "wiki"


class WikiThemeAssetsTests(unittest.TestCase):
    def test_handbook_pages_are_manifested(self) -> None:
        manifest = json.loads((WIKI / "manifest.json").read_text(encoding="utf-8"))
        pages = {page["path"] for page in manifest["pages"]}
        expected = {
            "02-language/index.md",
            "02-language/lexical-structure.md",
            "02-language/declarations.md",
            "02-language/expressions.md",
            "02-language/control-flow.md",
            "02-language/modules-concurrency.md",
            "02-language/diagnostics.md",
            "02-language/cookbook.md",
            "02-language/semantics-implementation.md",
        }
        self.assertTrue(expected.issubset(pages))
        for relative in expected:
            self.assertTrue((WIKI / relative).is_file(), relative)

    def test_theme_exposes_system_light_and_dark_palettes(self) -> None:
        config = tomllib.loads((ROOT / "zensical.toml").read_text(encoding="utf-8"))
        palettes = config["project"]["theme"]["palette"]
        self.assertIsInstance(palettes, list)
        self.assertEqual(
            {palette.get("scheme") for palette in palettes},
            {None, "default", "slate"},
        )
        self.assertTrue(any(palette.get("media") == "(prefers-color-scheme)" for palette in palettes))
        self.assertTrue(any(palette.get("media") == "(prefers-color-scheme: light)" for palette in palettes))
        self.assertTrue(any(palette.get("media") == "(prefers-color-scheme: dark)" for palette in palettes))
        for palette in palettes:
            self.assertIn("toggle", palette)
            self.assertIn("icon", palette["toggle"])
            self.assertIn("name", palette["toggle"])

    def test_code_highlighting_and_local_assets_are_enabled(self) -> None:
        config = tomllib.loads((ROOT / "zensical.toml").read_text(encoding="utf-8"))
        project = config["project"]
        self.assertIn("stylesheets/extra.css", project["extra_css"])
        extensions = project["markdown_extensions"]["pymdownx"]
        self.assertEqual(extensions["highlight"]["pygments_lang_class"], True)
        self.assertEqual(extensions["highlight"]["line_spans"], "__span")
        self.assertIn("inlinehilite", extensions)
        self.assertIn("superfences", extensions)

        css = (WIKI / "stylesheets" / "extra.css").read_text(encoding="utf-8")
        self.assertIn('[data-md-color-scheme="slate"]', css)
        self.assertIn(".zr-token-keyword", css)
        package = ROOT / "docs" / "pygments_zr"
        self.assertTrue((package / "pyproject.toml").is_file())
        self.assertTrue((package / "zr_pygments" / "lexer.py").is_file())

    def test_handbook_contains_annotated_zr_examples(self) -> None:
        handbook = (WIKI / "02-language" / "cookbook.md").read_text(encoding="utf-8")
        self.assertGreaterEqual(handbook.count("```zr"), 5)
        self.assertIn("预期结果", handbook)
        self.assertIn("错误用法", handbook)


if __name__ == "__main__":
    unittest.main()
