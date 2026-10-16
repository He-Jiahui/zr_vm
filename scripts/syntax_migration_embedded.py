"""Decode inventory string fragments while retaining host source boundaries."""
from dataclasses import dataclass
from typing import Iterable


@dataclass(frozen=True)
class EmbeddedStringPayload:
    text: str
    literal_start: int
    literal_end: int
    source_offsets: tuple[int, ...]
    line_offsets: tuple[int, ...]

    def source_offset(self, line: int, column: int) -> int:
        """Project a one-based decoded position into the host file."""
        return self.source_offsets[self.line_offsets[line - 1] + column - 1]


def iter_embedded_string_payloads(text: str) -> Iterable[EmbeddedStringPayload]:
    """Keep the inventory's small escape vocabulary, independent of host evaluation."""
    offset = 0
    while offset < len(text):
        current = text[offset]
        following = text[offset + 1] if offset + 1 < len(text) else ""
        if current == "/" and following == "/":
            end = text.find("\n", offset)
            offset = len(text) if end < 0 else end + 1
            continue
        if current == "/" and following == "*":
            end = text.find("*/", offset + 2)
            offset = len(text) if end < 0 else end + 2
            continue
        if current not in {'"', "'", "`"}:
            offset += 1
            continue
        quote = current
        start = offset + 1
        content = []
        source_offsets = []
        offset = start
        while offset < len(text):
            current = text[offset]
            if current == quote:
                break
            source_offsets.append(offset)
            if current == "\\" and offset + 1 < len(text):
                escaped = text[offset + 1]
                content.append("\n" if escaped == "n" else escaped)
                offset += 2
            else:
                content.append(current)
                offset += 1
        source_offsets.append(offset)
        decoded = "".join(content)
        line_offsets = (0,) + tuple(index + 1 for index, char in enumerate(decoded) if char == "\n")
        yield EmbeddedStringPayload(decoded, start, offset, tuple(source_offsets), line_offsets)
        offset += 1
