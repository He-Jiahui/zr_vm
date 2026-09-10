# Wiki reference expansion acceptance

Date: 2026-09-10

Scope: the `docs/wiki` reference expansion for current ZR declaration/statement
grammar, ownership/pattern recipes, official library APIs, native registry call
binding, dynamic descriptor plugins, native callback recipes, Parser/Compiler C
APIs, and Wiki authoring/build rules. This record validates the documentation
source contract and generated site; it does not claim a new runtime feature or
ABI version.

## Delivered surface

- Added a complete declaration/statement reference with parser boundaries,
  EBNF summaries, AST mapping, diagnostics, and C parser/compiler entry points.
- Added pattern/ownership/resource recipes covering destructuring, `var`/`let`,
  ownership intrinsics, `using`, `in`/`out`/`ref`, union cases, async frames,
  closures, and thread transfer constraints.
- Added official provider/API catalog and a call-binding registry reference,
  including module identity, phase/tier, descriptor hash inputs, target kinds,
  generation, relocation, and failure statuses.
- Added native plugin loading, native callback recipes, and deep Parser/Compiler
  C API pages with ownership, GC root, exception, writer, and reload boundaries.
- Added dedicated Core host/state/budget, Core value/object/module, GC/exception,
  project/file/ZRM, and artifact writer/loader references. These turn the former
  overview-level C API material into navigable, ownership-aware API chapters.
- Added Wiki authoring rules and wired all new pages into directory indexes,
  root navigation, `manifest.json`, and the page-count contract test.

## Verification evidence

Commands run from the repository root:

```text
python scripts/validate_wiki.py --root .
  wiki validation: 87 Markdown files, 86 manifest pages, 354 local links

python -m unittest tests/scripts/test_validate_wiki.py tests/scripts/test_wiki_theme_assets.py tests/scripts/test_zr_pygments.py -v
  Ran 12 tests ... OK

zensical build --clean --strict
  Build started / No issues found / Build finished

python scripts/validate_wiki.py --root . --site-dir site
  wiki validation: 87 Markdown files, 86 manifest pages, 354 local links, 101 site files
```

The generated `site/` directory is a build artifact and remains ignored. The
source validator and strict build both pass with the expanded page inventory.

## Continuation verification (2026-09-10)

The reference set was extended with dedicated chapters for generic parameter
declarations and instantiation, exception unwinding and cleanup, and the
combined System/Container/Reflection/Pooling runtime surface. Existing generic
examples were corrected to match the current parser: constraints belong in
`where` clauses rather than in an ordinary generic parameter declaration.

Commands run from the repository root:

```text
python scripts/validate_wiki.py --root .
  wiki validation: 116 Markdown files, 115 manifest pages, 644 local links

python -m unittest tests.scripts.test_validate_wiki -v
  Ran 5 tests ... OK

C:\\Users\\HeJiahui\\AppData\\Local\\Temp\\zrvm_wiki_verify_20260910_01\\Scripts\\zensical.exe build --clean --strict
  Build started / No issues found / Build finished in 4.54s

python scripts/validate_wiki.py --root . --site-dir site
  wiki validation: 116 Markdown files, 115 manifest pages, 644 local links, 130 site files
```

An HTTP smoke check against the generated site returned `200` and the expected
document title and H1 for the root page and all three new deep-reference routes:
`/02-language/generic-parameter-instantiation-reference/`,
`/02-language/exception-cleanup-runtime-reference/`, and
`/03-modules/system-container-reflection-runtime-reference/`.
