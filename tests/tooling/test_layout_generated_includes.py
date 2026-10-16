"""Safety tests for ordered generated-include classification."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
MODULE = REPO / "scripts" / "layout" / "generated_includes.py"
SPEC = importlib.util.spec_from_file_location("generated_includes", MODULE)
assert SPEC and SPEC.loader
classifier = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = classifier
SPEC.loader.exec_module(classifier)

FIXTURE_ROOT = Path("E:/cargo-targets/zr_vm/tests/layout-01a13033/generated-includes-fixtures")
REAL_BUILD = Path("E:/cargo-targets/zr_vm/build/layout-01a13033/msvc-static-debug")


class GeneratedIncludeTests(unittest.TestCase):
    def setUp(self) -> None:
        FIXTURE_ROOT.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="case-", dir=FIXTURE_ROOT)
        self.base = Path(self.temp.name)
        self.root = self.base / "repo"
        self.build = self.base / "build"
        self.root.mkdir()
        (self.build / "tests_generated").mkdir(parents=True)
        (self.root / "tests" / "synthetic_case").mkdir(parents=True)
        self.including = "tests/synthetic_case/source.c"
        (self.root / self.including).write_text("/* fixture source */\n", encoding="utf-8")

    def tearDown(self) -> None:
        # TemporaryDirectory owns only this test's unique child under registered E: tests root.
        self.temp.cleanup()

    def classify(self, name: str, variants: tuple[tuple[Path, ...], ...]):
        return classifier.classify_include(self.root, self.build, self.including, name, variants)

    def test_real_build_generated_header_is_classified_without_reading_contents(self) -> None:
        path = REAL_BUILD / "tests_generated" / classifier.GENERATED_HEADER
        if not path.is_file():
            self.skipTest("real configured build header is unavailable")
        roots = (REAL_BUILD / "tests_generated",)
        results = classifier.classify_include(
            REPO,
            REAL_BUILD,
            "tests/ffi/test_ffi_module.c",
            classifier.GENERATED_HEADER,
            (roots,),
        )
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0].kind, "generated")
        self.assertEqual(results[0].path, path.resolve())
        self.assertIsNone(results[0].relative_path)

    def test_exact_generated_output_is_accepted_and_same_ordered_result_kept_per_variant(self) -> None:
        generated = self.build / "tests_generated" / classifier.GENERATED_HEADER
        generated.write_bytes(b"opaque generated contents\x00")
        roots = (self.build / "tests_generated",)
        results = self.classify(classifier.GENERATED_HEADER, (roots, roots))
        self.assertEqual([result.kind for result in results], ["generated", "generated"])
        self.assertEqual([result.path for result in results], [generated.resolve(), generated.resolve()])

    def test_source_directory_shadow_wins_before_generated_root(self) -> None:
        local = self.root / "tests/synthetic_case" / classifier.GENERATED_HEADER
        local.write_text("local repository fixture\n", encoding="utf-8")
        (self.build / "tests_generated" / classifier.GENERATED_HEADER).write_text("generated", encoding="utf-8")
        results = self.classify(classifier.GENERATED_HEADER, ((self.build / "tests_generated",),))
        self.assertEqual(results[0].kind, "repository")
        self.assertEqual(results[0].path, local.resolve())
        self.assertEqual(results[0].relative_path, "tests/synthetic_case/" + classifier.GENERATED_HEADER)

    def test_first_existing_ordered_repository_root_wins(self) -> None:
        first = self.root / "tests/first"
        second = self.root / "tests/second"
        first.mkdir()
        second.mkdir()
        a = first / "synthetic_probe.h"
        b = second / "synthetic_probe.h"
        a.write_text("first\n", encoding="utf-8")
        b.write_text("second\n", encoding="utf-8")
        result = self.classify("synthetic_probe.h", ((first, second),))[0]
        self.assertEqual(result.kind, "repository")
        self.assertEqual(result.path, a.resolve())

    def test_repository_parent_relative_include_resolves_without_external_escape(self) -> None:
        parent = self.root / "tests/parent_probe.h"
        parent.write_text("repository parent target\n", encoding="utf-8")
        result = classifier.classify_include(
            self.root, self.build, self.including, "../parent_probe.h", ((),)
        )[0]
        self.assertEqual(result.kind, "repository")
        self.assertEqual(result.path, parent.resolve())
        self.assertEqual(result.relative_path, "tests/parent_probe.h")

    def test_generated_header_alias_is_not_treated_as_the_exact_generated_name(self) -> None:
        generated = self.build / "tests_generated" / classifier.GENERATED_HEADER
        generated.write_text("opaque\n", encoding="utf-8")
        result = self.classify("nested/../" + classifier.GENERATED_HEADER,
                               ((self.build / "tests_generated",),))[0]
        self.assertEqual(result.kind, "unsafe")
    def test_wrong_external_name_and_wrong_build_generated_root_are_unsafe(self) -> None:
        generated_root = self.build / "tests_generated"
        (generated_root / "other_generated.h").write_text("x", encoding="utf-8")
        other_build = self.base / "other-build"
        (other_build / "tests_generated").mkdir(parents=True)
        (other_build / "tests_generated" / classifier.GENERATED_HEADER).write_text("x", encoding="utf-8")
        wrong_name = self.classify("other_generated.h", ((generated_root,),))[0]
        wrong_build = self.classify(classifier.GENERATED_HEADER, ((other_build / "tests_generated",),))[0]
        self.assertEqual(wrong_name.kind, "unsafe")
        self.assertEqual(wrong_build.kind, "unsafe")

    def test_missing_header_is_unresolved_and_variants_are_not_collapsed(self) -> None:
        generated_root = self.build / "tests_generated"
        generated = generated_root / classifier.GENERATED_HEADER
        repo_header = self.root / "tests/synthetic_case" / classifier.GENERATED_HEADER
        results = self.classify(classifier.GENERATED_HEADER, ((generated_root,), ()))
        self.assertEqual([result.kind for result in results], ["unresolved", "unresolved"])
        generated.write_text("generated", encoding="utf-8")
        alternate_root = self.root / "tests/alternate"
        alternate_root.mkdir()
        alternate_header = alternate_root / classifier.GENERATED_HEADER
        alternate_header.write_text("repository variant", encoding="utf-8")
        mixed = self.classify(classifier.GENERATED_HEADER, ((generated_root,), (alternate_root,)))
        self.assertEqual([result.kind for result in mixed], ["generated", "repository"])
        alternate_header.unlink()
        partial = self.classify(classifier.GENERATED_HEADER, ((generated_root,), (self.root / "tests" / "absent",)))
        self.assertEqual([result.kind for result in partial], ["generated", "unresolved"])

    def test_acceptance_snapshot_and_vendor_paths_are_rejected(self) -> None:
        for relative in (
            "tests/acceptance/synthetic_case.h",
            "tests/snapshots/synthetic_case.h",
            "tests/third_party/synthetic_vendor/synthetic_case.h",
        ):
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture\n", encoding="utf-8")
            include = path.relative_to(self.root).as_posix()
            source = "tests/synthetic_case/source.c"
            result = classifier.classify_include(self.root, self.build, source, include, ((self.root,),))[0]
            self.assertEqual(result.kind, "unsafe", relative)

    def test_reparse_candidate_is_rejected_when_symlink_creation_is_supported(self) -> None:
        target = self.root / "tests/synthetic_case/real_probe.h"
        target.write_text("safe target\n", encoding="utf-8")
        link = self.root / "tests/synthetic_case/symlink_probe.h"
        try:
            link.symlink_to(target)
        except (OSError, NotImplementedError) as exc:
            self.skipTest(f"symlink unavailable: {exc}")
        result = self.classify("symlink_probe.h", ((),))[0]
        self.assertEqual(result.kind, "unsafe")


if __name__ == "__main__":
    unittest.main()
