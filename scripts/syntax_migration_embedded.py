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

# @unit EmbeddedStringPayload [type]
# ## Summary
# 保存一个宿主引号片段的有限解码结果和逐字符边界表，供迁移清单把 decoded finding 的起点、尾后终点分别投回宿主文本。
# ## Fields
# text 是解码后的 Python str；literal_start 是开引号之后的宿主字符索引，literal_end 是闭引号索引，未闭合时为宿主文本长度。
# source_offsets 对每个解码字符保存所消费宿主字符或反斜杠的起始索引，并在末尾保存闭引号或 EOF 哨兵；长度等于 len(text)+1。
# line_offsets 从 0 开始，并记录每个解码换行之后的位置；末尾换行也会产生空尾行。字段构成的是字符坐标表，不是字节或 UTF-16 坐标表。
# ## Constraints and lifecycle
# frozen dataclass 的自动构造器不验证上述关联不变量；手工构造者必须提供一致的文本、索引和表。普通字段赋值受 frozen 限制，字段注解并不执行运行时类型校验。
# 生成器发布的 str、tuple 自成快照；继续、关闭或丢弃生成器不会使已持有的 payload 失效。不持有待关闭的文件或需要显式 Free 的资源。
# 输入文本的坐标域是调用方提供的 Python str；read_text 的换行规范化、文件编码到字节的位置和 LSP UTF-16 单位不由本记录还原。
# ## References
# scripts/syntax_migration_embedded.py:6
# scripts/syntax_migration_embedded.py:53
# scripts/syntax_migration_embedded.py:55
# scripts/syntax_migration_embedded.py:56
# scripts/syntax_migration_inventory.py:861
# scripts/syntax_migration_inventory.py:882
# scripts/syntax_migration_inventory.py:883

# @unit EmbeddedStringPayload.source_offset [function]
# ## Summary
# 在扫描器已给出有效的一基 decoded 行列后，查询该边界在宿主 str 中的字符索引；起点和尾后终点使用同一映射。
# ## Parameters
# @param[in] self 满足 EmbeddedStringPayload 表关联不变量的记录。
# @param[in] line 有效 decoded 行号，从 1 开始，须对应 line_offsets 中的一行。
# @param[in] column 该行内的有效一基字符边界列；可表示整个 decoded 文本的尾后终点，但不能借列号跨入另一行。
# ## Returns
# @return 返回 source_offsets 中的宿主字符索引；尾后终点可以命中闭引号或 EOF 哨兵。
# ## Constraints and lifecycle
# 此函数只作两次索引，不验证参数范围或构造表。无效正索引可能抛 IndexError，零值或负值还可能按 Python 负索引选择其他位置，调用方不能把它当作受控的参数拒绝接口。
# 查询不修改记录、原文本或输出对象，也不分配独立资源；结果整数按产生它的同一宿主 str 解释，不能直接当作 UTF-8 字节或 UTF-16 偏移。
# ## References
# scripts/syntax_migration_embedded.py:14
# scripts/syntax_migration_embedded.py:16
# scripts/syntax_migration_embedded.py:53
# scripts/syntax_migration_embedded.py:55
# scripts/syntax_migration_inventory.py:882
# scripts/syntax_migration_inventory.py:883

# @unit iter_embedded_string_payloads [function]
# ## Summary
# 惰性遍历宿主文本中的单引号、双引号及反引号片段，保留清单扫描器有限的转义规则，并为每个片段生成独立的宿主字符边界表。
# ## Parameters
# @param[in] text 待扫描的 Python str；调用方决定它如何由文件解码及规范化换行。
# ## Returns
# @return 返回可迭代的生成器对象；迭代时逐项 yield EmbeddedStringPayload，耗尽时结束。调用本身不先收集所有片段。
# ## Constraints and lifecycle
# 只在引号外跳过 // 到换行及 /* 到 */ 的宿主注释；未闭合注释消耗余下文本，不报告语法错误。引号内的注释标记保留给下游 Zr 扫描器。
# 反斜杠加 n 解为换行；其余反斜杠加一个字符只保留该字符，例如反斜杠加 t 得到字母 t，不是制表符。不执行宿主求值、完整 C/JavaScript 转义、插值或相邻字符串拼接。
# 转义引号不会结束片段；没有闭引号时仍发布到 EOF 的片段，最后单独的反斜杠作为普通字符保留。每个片段都有闭引号/EOF 的尾后哨兵。
# 生成器暂停期间保留输入 str 和局部构造状态；应串行推进同一个迭代器。已 yield 的冻结记录可独立持有，无文件句柄或显式释放协议；关闭或丢弃迭代器释放其剩余 Python 引用。
# 分配失败等 Python 异常在迭代推进时传播；此前已发布的记录仍有效。本模块不写文件，也不决定片段是否属于 Zr 输入，上下文筛选由 _scan_path 完成。
# ## References
# scripts/syntax_migration_embedded.py:19
# scripts/syntax_migration_embedded.py:25
# scripts/syntax_migration_embedded.py:29
# scripts/syntax_migration_embedded.py:33
# scripts/syntax_migration_embedded.py:43
# scripts/syntax_migration_embedded.py:46
# scripts/syntax_migration_embedded.py:53
# scripts/syntax_migration_embedded.py:56
# scripts/syntax_migration_inventory.py:15
# scripts/syntax_migration_inventory.py:859
# scripts/syntax_migration_inventory.py:872
