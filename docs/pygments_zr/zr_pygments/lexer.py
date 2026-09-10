"""A small, conservative Pygments lexer for production ZR source examples."""

from __future__ import annotations

import re

from pygments.lexer import RegexLexer, bygroups
from pygments.token import Comment, Keyword, Name, Number, Operator, Punctuation, String, Text


class ZrLexer(RegexLexer):
    """Highlight ZR syntax without attempting semantic name resolution."""

    name = "ZR"
    aliases = ["zr", "zro", "zrs", "zrp"]
    filenames = ["*.zr", "*.zro", "*.zrs", "*.zrp"]
    mimetypes = ["text/x-zr"]
    flags = re.MULTILINE | re.UNICODE

    tokens = {
        "root": [
            (r"//[^\n]*", Comment.Single),
            (r"(?s:/\*.*?\*/)", Comment.Multiline),
            (r"(?s:#[^#]*#)", Name.Decorator),
            (
                r"\b(fn)(\s+)([A-Za-z_][A-Za-z0-9_]*)",
                bygroups(Keyword, Text.Whitespace, Name.Function),
            ),
            (
                r"\b(resource)(\s+)(class)(\s+)([A-Za-z_][A-Za-z0-9_]*)",
                bygroups(Keyword, Text.Whitespace, Keyword, Text.Whitespace, Name.Class),
            ),
            (
                r"\b(struct|class|interface|enum|union)(\s+)([A-Za-z_][A-Za-z0-9_]*)",
                bygroups(Keyword, Text.Whitespace, Name.Class),
            ),
            (r'"(?:\\.|[^"\\])*"', String.Double),
            (r"'(?:\\.|[^'\\])*'", String.Char),
            (r"\b(?:true|false|null|Infinity|NegativeInfinity|NaN)\b", Keyword.Constant),
            (
                r"\b(?:bool|byte|char|short|int|i8|i16|i32|i64|u8|u16|u32|u64|usize|float|double|"
                r"string|void|object|Any|Task|Iterator|AsyncIterator|Span|Unique|Shared|Weak)\b",
                Keyword.Type,
            ),
            (
                r"\b(?:fn|struct|class|interface|enum|union|module|abstract|virtual|override|final|shadow|test|intermediate|var|let|using|"
                r"pub|pri|pro|if|else|switch|while|for|break|continue|return|yield|super|new|set|get|"
                r"static|const|in|out|throw|try|catch|finally|ref|typeid|typeof|share|degrade|wake|intoGc|"
                r"drop|import|init|own|readonly|scoped|async|await|comptime|native|extern|resource|"
                r"property|where|move|borrow|unique|shared|weak|default)\b",
                Keyword,
            ),
            (
                r"(?<![\w.])(?:(?:\d+\.\d*(?:[eE][+-]?\d+)?|\d+[eE][+-]?\d+)[fFdD]?|\d+[fFdD])(?![\w.])",
                Number.Float,
            ),
            (r"(?<![\w.])(?:0[xX][0-9A-Fa-f]+|0[0-7]+|\d+)(?![\w.])", Number.Integer),
            (r"`(?:\\.|[^`\\])*`", String.Backtick),
            (r"(?:===|!==|==|!=|<=|>=|\+=|-=|\*=|/=|%=|&&|\|\||<<|>>|=>|->|\.\.\.|\.\.|\?\.|[+*/%<>=!&|^~-])", Operator),
            (r"[{}()\[\],;:.?@]", Punctuation),
            (r"[A-Za-z_][A-Za-z0-9_]*", Name),
            (r"\s+", Text.Whitespace),
            (r".", Text),
        ]
    }

    @staticmethod
    def analyse_text(text: str) -> float:
        if re.search(r"(?m)^\s*module\s+[A-Za-z_][A-Za-z0-9_.]*\s*;", text):
            return 0.8
        if re.search(r"\b(?:fn|struct|class|interface|union)\b", text):
            return 0.5
        return 0.0
