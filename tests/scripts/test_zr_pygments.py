"""Unit tests for the local Pygments lexer used by the Wiki build."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path


PACKAGE_ROOT = Path(__file__).resolve().parents[2] / "docs" / "pygments_zr"
if str(PACKAGE_ROOT) not in sys.path:
    sys.path.insert(0, str(PACKAGE_ROOT))


class ZrPygmentsTests(unittest.TestCase):
    def test_lexer_exposes_zr_aliases_and_file_patterns(self) -> None:
        from zr_pygments.lexer import ZrLexer

        self.assertEqual(ZrLexer.aliases, ["zr", "zro", "zrs", "zrp"])
        self.assertEqual(ZrLexer.filenames, ["*.zr", "*.zro", "*.zrs", "*.zrp"])

    def test_lexer_classifies_language_tokens(self) -> None:
        from pygments.token import Comment, Keyword, Name, Number, Operator, String
        from zr_pygments.lexer import ZrLexer

        tokens = list(ZrLexer().get_tokens('fn add(value: int): int { return value + 1; } // ok'))
        token_types = [token_type for token_type, _ in tokens]
        values = [value for _, value in tokens]
        self.assertIn(Keyword, token_types)
        self.assertIn(Name.Function, token_types)
        self.assertIn(Number.Integer, token_types)
        self.assertIn(Number.Float, [token_type for token_type, _ in ZrLexer().get_tokens("3.14f")])
        for literal in ("1.", "3.14f", "1e-3", "2d"):
            literal_tokens = list(ZrLexer().get_tokens(literal))
            self.assertEqual("".join(value for _, value in literal_tokens), literal + "\n")
            self.assertTrue(any(token_type is Number.Float for token_type, _ in literal_tokens))
        self.assertIn(Operator, token_types)
        self.assertIn(Comment.Single, token_types)
        self.assertIn("add", values)

    def test_lexer_handles_strings_and_attributes_without_html_injection(self) -> None:
        from pygments import highlight, lex
        from pygments.formatters import HtmlFormatter
        from pygments.token import Keyword, Name, Operator, String
        from zr_pygments.lexer import ZrLexer

        html = highlight(
            'let text = "<script>alert(1)</script>";\n#zr.testing.test#',
            ZrLexer(),
            HtmlFormatter(nowrap=True),
        )
        self.assertIn("&lt;script&gt;", html)
        self.assertNotIn("<script>alert", html)

        template_tokens = list(ZrLexer().get_tokens("`hello ${name}`"))
        self.assertTrue(any(token_type is String.Backtick for token_type, _ in template_tokens))

        declaration_source = "resource class FileHandle { property value: int; }"
        declaration_tokens = list(lex(declaration_source, ZrLexer()))
        self.assertIn((Keyword, "resource"), declaration_tokens)
        self.assertIn((Keyword, "class"), declaration_tokens)
        self.assertIn((Name.Class, "FileHandle"), declaration_tokens)
        self.assertIn((Keyword, "property"), declaration_tokens)
        self.assertEqual("".join(value for _, value in declaration_tokens), declaration_source + "\n")

        function_source = "fn add(value: int): int { return value; }"
        function_tokens = list(lex(function_source, ZrLexer()))
        self.assertEqual("".join(value for _, value in function_tokens), function_source + "\n")

        function_type_tokens = list(lex("let f: fn(int) -> int;", ZrLexer()))
        self.assertIn((Keyword, "fn"), function_type_tokens)
        self.assertIn((Operator, "..."), list(lex("...values", ZrLexer())))


if __name__ == "__main__":
    unittest.main()
