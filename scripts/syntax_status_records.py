from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


# 冻结的 55 份语法里程碑记录分布；校验器以它检测文档新增、删除或迁移造成的漂移。
EXPECTED_DIRECTORY_COUNTS = {
    "01": 5,
    "02": 6,
    "03": 5,
    "04": 7,
    "05": 6,
    "06": 2,
    "07": 1,
    "10": 5,
    "12": 15,
    "13": 3,
}
EXPECTED_RECORD_COUNT = 55

# 该任务是 05 目录的支撑计划，不作为独立里程碑状态记录计数。
_EXCLUDED_SUPPORT_RECORD = (
    "05-property-unified-ast/m5-task4-property-import-bootstrap.md"
)
# 状态与完成时间仅从 Markdown 列表行读取；中英文标签共用报告协议。
_STATUS_PATTERN = re.compile(
    r"^\s*-\s*(?:Status|\u72b6\u6001)\s*[:\uff1a]\s*(?P<value>.+?)\s*$",
    re.IGNORECASE,
)
_TIME_PATTERN = re.compile(
    r"^\s*-\s*(?:Completion\s+time|Completed\s+at|\u5b8c\u6210\u65f6\u95f4)"
    r"\s*[:\uff1a]\s*(?P<value>.+?)\s*$",
    re.IGNORECASE,
)
# 允许已完成状态带限定说明，仍由下方 validate 检查时间字段与总数。
_ENGLISH_COMPLETION_PATTERN = re.compile(r"^completed(?:\b|_)", re.IGNORECASE)


@dataclass(frozen=True)
class SyntaxStatusRecord:
    # 单个 Markdown 记录的相对路径、分组及原始状态字段；缺失值保留为 None 供校验报告。
    relative_path: str
    directory: str
    status: str | None
    completion_time: str | None

    @property
    def is_complete(self) -> bool:
        # 仅把明确写有“已完成”或 completed 前缀的状态计入冻结集合。
        if self.status is None:
            return False
        normalized = self.status.strip().strip("`").strip()
        return "\u5df2\u5b8c\u6210" in normalized or _ENGLISH_COMPLETION_PATTERN.match(
            normalized
        ) is not None


@dataclass(frozen=True)
class SyntaxStatusReport:
    # 收集结果同时保留缺失/未完成路径，使文本、JSON 与退出码共享同一组问题。
    records: tuple[SyntaxStatusRecord, ...]
    directory_counts: dict[str, int]
    missing_status: tuple[str, ...]
    non_complete: tuple[str, ...]
    missing_time: tuple[str, ...]

    @property
    def complete_count(self) -> int:
        # 汇总使用 record 的同一完成定义，避免主报告与逐记录判定分叉。
        return sum(record.is_complete for record in self.records)

    def to_json(self) -> str:
        # 机器消费方需要完整记录与分类问题；排序由 collect 的路径选择阶段确定。
        payload = {
            "schemaVersion": 1,
            "total": len(self.records),
            "complete": self.complete_count,
            "missingStatus": list(self.missing_status),
            "nonComplete": list(self.non_complete),
            "missingTime": list(self.missing_time),
            "directoryCounts": self.directory_counts,
            "records": [
                {
                    "path": record.relative_path,
                    "directory": record.directory,
                    "status": record.status,
                    "completionTime": record.completion_time,
                }
                for record in self.records
            ],
        }
        return json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n"


# 只选编号目录中的里程碑记录；implementation plan 和指定支撑记录不在 55 份合同内。
def _selected_markdown_paths(status_root: Path) -> Iterable[Path]:
    if not status_root.is_dir():
        return ()

    selected: list[Path] = []
    for child in sorted(status_root.iterdir(), key=lambda path: path.name):
        if not child.is_dir() or not child.name or not child.name[0].isdigit():
            continue
        for path in child.rglob("*.md"):
            relative_path = path.relative_to(status_root).as_posix()
            if path.name.endswith("-implementation-plan.md"):
                continue
            if relative_path == _EXCLUDED_SUPPORT_RECORD:
                continue
            selected.append(path)
    return sorted(selected, key=lambda path: path.relative_to(status_root).as_posix())


# 对状态/时间取首个匹配列表项，避免正文中后续重复提及覆盖文件头记录。
def _first_match(lines: Iterable[str], pattern: re.Pattern[str]) -> str | None:
    for line in lines:
        match = pattern.match(line)
        if match is not None:
            return match.group("value").strip()
    return None


# CLI 和测试共用的只读收集入口；保留缺失字段而不在遍历中提前报错，方便一次显示全部漂移。
def collect_syntax_status_records(repository_root: Path) -> SyntaxStatusReport:
    status_root = repository_root / "docs" / "plans" / "syntax"
    records: list[SyntaxStatusRecord] = []
    directory_counts: dict[str, int] = {}

    for path in _selected_markdown_paths(status_root):
        relative_path = path.relative_to(status_root).as_posix()
        directory = relative_path.split("/", 1)[0].split("-", 1)[0]
        lines = path.read_text(encoding="utf-8").splitlines()
        record = SyntaxStatusRecord(
            relative_path=relative_path,
            directory=directory,
            status=_first_match(lines, _STATUS_PATTERN),
            completion_time=_first_match(lines, _TIME_PATTERN),
        )
        records.append(record)
        directory_counts[directory] = directory_counts.get(directory, 0) + 1

    missing_status = tuple(
        record.relative_path for record in records if record.status is None
    )
    non_complete = tuple(
        record.relative_path
        for record in records
        if record.status is not None and not record.is_complete
    )
    missing_time = tuple(
        record.relative_path for record in records if record.completion_time is None
    )
    return SyntaxStatusReport(
        records=tuple(records),
        directory_counts=dict(sorted(directory_counts.items())),
        missing_status=missing_status,
        non_complete=non_complete,
        missing_time=missing_time,
    )


# 将冻结数量/目录分布与每份状态合同一并核验；返回所有问题供 CLI 设置非零退出码。
def validate_syntax_status_records(report: SyntaxStatusReport) -> tuple[str, ...]:
    issues: list[str] = []
    if len(report.records) != EXPECTED_RECORD_COUNT:
        issues.append(
            f"expected {EXPECTED_RECORD_COUNT} records, found {len(report.records)}"
        )
    if report.directory_counts != EXPECTED_DIRECTORY_COUNTS:
        issues.append(
            "directory distribution differs: "
            f"expected {EXPECTED_DIRECTORY_COUNTS}, found {report.directory_counts}"
        )
    if report.missing_status:
        issues.append("missing status: " + ", ".join(report.missing_status))
    if report.non_complete:
        issues.append("non-complete status: " + ", ".join(report.non_complete))
    if report.missing_time:
        issues.append("missing completion time: " + ", ".join(report.missing_time))
    return tuple(issues)


# 人工模式用固定键和 ERROR 行输出，不隐藏任何发现的问题。
def _format_text(report: SyntaxStatusReport, issues: tuple[str, ...]) -> str:
    distribution = " ".join(
        f"{directory}={count}"
        for directory, count in report.directory_counts.items()
    )
    lines = [
        f"TOTAL={len(report.records)}",
        f"COMPLETE={report.complete_count}",
        f"MISSING_STATUS={len(report.missing_status)}",
        f"NON_COMPLETE={len(report.non_complete)}",
        f"MISSING_TIME={len(report.missing_time)}",
        distribution,
    ]
    lines.extend(f"ERROR={issue}" for issue in issues)
    return "\n".join(lines) + "\n"


# 文档状态 gate 的命令行入口：JSON/text 仅改变呈现，退出码始终反映同一次 validate 结果。
def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Verify the frozen docs/plans/syntax 55-record status set."
    )
    parser.add_argument(
        "--repository",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root (defaults to the parent of scripts/)",
    )
    parser.add_argument("--format", choices=("text", "json"), default="text")
    arguments = parser.parse_args(argv)

    report = collect_syntax_status_records(arguments.repository.resolve())
    issues = validate_syntax_status_records(report)
    if arguments.format == "json":
        sys.stdout.write(report.to_json())
    else:
        sys.stdout.write(_format_text(report, issues))
    return 1 if issues else 0


if __name__ == "__main__":
    raise SystemExit(main())

# @unit EXPECTED_DIRECTORY_COUNTS
# ## Summary
# 定义状态记录门禁认可的编号目录分布，供验证阶段发现记录新增、删除或分组变化。
# ## Constraints and lifecycle
# 这是显式维护的预期表，不是从扫描结果推导的计数。当前十组的计数合计55；它是普通可变dict，不因常量式名称而深冻结。调用者应把它作为只读配置，在一次验证及输出期间不得并发修改；变更这份门禁政策须同时核对总数和计划集合。
# ## References
# - scripts/syntax_status_records.py:13
# - scripts/syntax_status_records.py:175
# - scripts/syntax_status_records.py:178
# - tests/scripts/test_syntax_status_records.py:29
# - tests/scripts/test_syntax_status_records.py:131

# @unit EXPECTED_RECORD_COUNT
# ## Summary
# 给验证器提供冻结的记录总数，使目录选择范围漂移产生明确问题文本。
# ## Constraints and lifecycle
# 55是政策预期而非发现值；collect不以该数截断枚举。验证同时检查目录分布及问题分区，不能以总数相同推出记录身份或完成状态正确。整数值不持有文件资源；不得在同一次校验期间重绑定该全局配置。
# ## References
# - scripts/syntax_status_records.py:25
# - scripts/syntax_status_records.py:171
# - scripts/syntax_status_records.py:173
# - tests/scripts/test_syntax_status_records.py:120
# - tests/scripts/test_syntax_status_records.py:129

# @unit _EXCLUDED_SUPPORT_RECORD
# ## Summary
# 从编号目录递归枚举中排除一份指定支撑计划，保持里程碑集合的身份边界。
# ## Constraints and lifecycle
# 按相对于docs/plans/syntax的POSIX路径字符串精确比较；只排除这一条，不排除整个05目录，也不按文件正文或状态判断。字符串值无外部资源；目录迁移或重命名后必须重新核对这条特例和冻结分布。
# ## References
# - scripts/syntax_status_records.py:28
# - scripts/syntax_status_records.py:114
# - tests/scripts/test_syntax_status_records.py:67
# - tests/scripts/test_syntax_status_records.py:69
# - tests/scripts/test_syntax_status_records.py:79

# @unit _STATUS_PATTERN
# ## Summary
# 规定从单行Markdown列表项提取状态字段的中英文标签协议，供首匹配提取器使用。
# ## Constraints and lifecycle
# 匹配列表前缀“-”、Status或状态标签、ASCII或全角冒号和命名组value；英文标签忽略大小写。不是Markdown结构解析器，不辨别标题、代码围栏或语义否定；同文件多次出现时由_first_match取首条。返回值还会strip；不匹配保留None，不把日期或完成语义合并到此正则中。
# ## References
# - scripts/syntax_status_records.py:32
# - scripts/syntax_status_records.py:33
# - scripts/syntax_status_records.py:121
# - scripts/syntax_status_records.py:142
# - tests/scripts/test_syntax_status_records.py:95
# - tests/scripts/test_syntax_status_records.py:96

# @unit _TIME_PATTERN
# ## Summary
# 规定完成时间列表项的可识别标签，保留原始时间文本供报告和缺失分类使用。
# ## Constraints and lifecycle
# 接受Completion time、Completed at或完成时间及两种冒号，英文忽略大小写；同样依赖_first_match取首条列表行。这里只提取并去除首尾空白，不检查时间格式、时区、先后关系或真实性；仅None被collect归入missing_time。正则对象供只读复用，不持有打开的文件。
# ## References
# - scripts/syntax_status_records.py:36
# - scripts/syntax_status_records.py:37
# - scripts/syntax_status_records.py:38
# - scripts/syntax_status_records.py:143
# - scripts/syntax_status_records.py:157
# - tests/scripts/test_syntax_status_records.py:95
# - tests/scripts/test_syntax_status_records.py:105

# @unit _ENGLISH_COMPLETION_PATTERN
# ## Summary
# 统一英文完成状态前缀判定，并允许前缀之后保留限定说明。
# ## Constraints and lifecycle
# 忽略大小写，要求开头为completed且其后是单词边界或下划线；不接受任意位置含completed的字符串。中文“已完成”的包含判定由is_complete另行执行，且完成时间不参与该属性。共享模式没有文件资源，调用期间按只读配置使用。
# ## References
# - scripts/syntax_status_records.py:42
# - scripts/syntax_status_records.py:58
# - scripts/syntax_status_records.py:59
# - tests/scripts/test_syntax_status_records.py:54
# - tests/scripts/test_syntax_status_records.py:58
# - tests/scripts/test_syntax_status_records.py:83

# @unit SyntaxStatusRecord
# ## Summary
# 保存一份选中Markdown的路径、分组及提取出的两个字段，供汇总、验证和序列化共享同一值快照。
# ## Fields
# - relative_path：相对于docs/plans/syntax的POSIX路径字符串。
# - directory：第一段路径在首个连字符之前的分组；collect计算它，类型本身不校验。
# - status、completion_time：首条匹配列表项的strip结果；None表示未匹配，不表示日期或状态合法。
# ## Constraints and lifecycle
# frozen数据类限制常规字段重绑定，不执行路径、字段类型或状态内容的运行期认证；调用者直接构造时应维持字段约定。collect持有读出的Python字符串，记录不借用打开文件或原文件缓冲，临时源目录删除不使已返回字符串失效。此类型不拥有文件句柄，也不自动验证完成时间。
# ## References
# - scripts/syntax_status_records.py:45
# - scripts/syntax_status_records.py:48
# - scripts/syntax_status_records.py:139
# - scripts/syntax_status_records.py:142
# - scripts/syntax_status_records.py:143
# - scripts/syntax_status_records.py:90
# - scripts/syntax_status_records.py:93
# - tests/scripts/test_syntax_status_records.py:76
# - tests/scripts/test_syntax_status_records.py:78

# @unit SyntaxStatusRecord.is_complete
# ## Summary
# 在汇总完成数和划分未完成记录时复用同一完成状态判定。
# ## Parameters
# @param[in] self 状态记录；status应为str或None，读取期间保持稳定。
# ## Returns
# @return 返回bool：缺失状态为False；否则去首尾空白、边缘反引号后，中文包含“已完成”或英文匹配完成前缀时为True。
# ## Constraints and lifecycle
# 此属性只读status，不检查completion_time、日期真实性或文本中的否定语义。中文是子串包含，英文模式从开头匹配；不应把它当自然语言完成事实认证。没有I/O或缓存；错误的手工字段类型可使字符串操作抛出异常。
# ## References
# - scripts/syntax_status_records.py:54
# - scripts/syntax_status_records.py:56
# - scripts/syntax_status_records.py:58
# - scripts/syntax_status_records.py:59
# - scripts/syntax_status_records.py:76
# - scripts/syntax_status_records.py:154
# - tests/scripts/test_syntax_status_records.py:83
# - tests/scripts/test_syntax_status_records.py:102

# @unit SyntaxStatusReport
# ## Summary
# 持有收集结果、分组计数和问题路径分区，使两种输出与验证入口可共享一次收集的值快照。
# ## Fields
# - records：按collect选中的相对路径排序的记录tuple；手工构造时不自动排序。
# - directory_counts：各记录分组计数；collect按键排序构造普通dict。
# - missing_status、non_complete、missing_time：各问题的相对路径tuple；分别对应status为None、非None但未完成、completion_time为None。
# ## Constraints and lifecycle
# collect产生彼此一致的字段，但公开构造器并不复算或验证它们。frozen只限制字段重绑定，directory_counts仍是可变dict，使用期间不得并发修改；调用者不能把它当深不可变报告。complete_count每次读取records重算，其余问题分区已预先保存，不会随手工修改自动对齐。报告持有Python值，不延长源文件或目录生命周期，也不持有文件句柄。
# ## References
# - scripts/syntax_status_records.py:64
# - scripts/syntax_status_records.py:67
# - scripts/syntax_status_records.py:159
# - scripts/syntax_status_records.py:161
# - scripts/syntax_status_records.py:169
# - scripts/syntax_status_records.py:190
# - tests/scripts/test_syntax_status_records.py:76
# - tests/scripts/test_syntax_status_records.py:78

# @unit SyntaxStatusReport.complete_count
# ## Summary
# 从每个记录的统一完成属性计算完成数，避免文本与JSON各自解释状态。
# ## Parameters
# @param[in] self 报告；records及记录字段须符合记录类型约定并在读取期间稳定。
# ## Returns
# @return 返回int，为records中is_complete为True的个数；空records返回0。
# ## Constraints and lifecycle
# 每次访问重新遍历records，不使用missing_status或non_complete分区推算，不进行文件读取、日期校验或缓存更新。手工构造的报告若分区不一致，此结果不会自动修正分区；底层属性异常直接传播。
# ## References
# - scripts/syntax_status_records.py:74
# - scripts/syntax_status_records.py:76
# - scripts/syntax_status_records.py:83
# - scripts/syntax_status_records.py:197
# - tests/scripts/test_syntax_status_records.py:83
# - tests/scripts/test_syntax_status_records.py:102
# - tests/scripts/test_syntax_status_records.py:130

# @unit SyntaxStatusReport.to_json
# ## Summary
# 将一次报告序列化为机器可读的schemaVersion 1文本，保留记录与问题分区以供调用方决定门禁结果。
# ## Parameters
# @param[in] self 已构造的报告；各字段须可按当前JSON载荷编码，调用期间保持稳定。
# ## Returns
# @return 返回以换行结尾的str，包含总数、完成数、三类问题、目录分布及记录数组；不写文件或stdout。
# ## Constraints and lifecycle
# 不调用validate，也不因存在问题而拒绝输出；main另行决定0或1返回码。json.dumps使用ensure_ascii=False、indent=2、sort_keys=True；对象键排序不重排records数组，记录顺序由报告决定。错误的手工字段类型可能引发序列化异常；返回文本的编码和物理换行转换由后续输出流决定，不能从此方法推出stdout必为UTF-8。
# ## References
# - scripts/syntax_status_records.py:78
# - scripts/syntax_status_records.py:80
# - scripts/syntax_status_records.py:88
# - scripts/syntax_status_records.py:98
# - scripts/syntax_status_records.py:224
# - scripts/syntax_status_records.py:227

# @unit _selected_markdown_paths
# ## Summary
# 枚举状态根下符合编号目录规则的Markdown候选，应用两种排除规则并返回确定路径排序。
# ## Parameters
# @param[in] status_root 应扫描的路径；枚举期间需保持目录结构及可访问性稳定，路径不由本函数resolve。
# ## Returns
# @return 根不是目录时返回空tuple；否则返回按相对POSIX路径排序的list[Path]，接口标注Iterable但实现会在返回前完成枚举，不是generator。
# ## Constraints and lifecycle
# 仅考虑直接子目录且其首字符满足isdigit；不要求两位ASCII编号或文件名以m开头，再递归rglob("*.md")。排除文件名以-implementation-plan.md结尾的候选和指定支撑相对路径。没有末端is_file认证、符号链接信任边界或目录快照保证；不能对不可信路径集合据此声称普通文件或沙箱认证。枚举/相对路径计算异常传播，未返回部分结果；本函数不打开或修改Markdown。
# ## References
# - scripts/syntax_status_records.py:102
# - scripts/syntax_status_records.py:103
# - scripts/syntax_status_records.py:107
# - scripts/syntax_status_records.py:108
# - scripts/syntax_status_records.py:110
# - scripts/syntax_status_records.py:112
# - scripts/syntax_status_records.py:114
# - scripts/syntax_status_records.py:117
# - scripts/syntax_status_records.py:135
# - tests/scripts/test_syntax_status_records.py:76
# - tests/scripts/test_syntax_status_records.py:79

# @unit _first_match
# ## Summary
# 在同一文件的行序列中提取某字段的第一条匹配值，使后续重复提及不覆盖先出现的记录。
# ## Parameters
# @param[in] lines 可迭代str行；若为一次性iterator，本函数会消费到首个匹配或耗尽，不负责关闭它。
# @param[in] pattern 支持match且含命名组value的编译模式；调用期间保持可用。
# ## Returns
# @return 首个match的value组经strip后返回str；遍历结束仍未匹配则返回None。
# ## Constraints and lifecycle
# 不解析Markdown章节或围栏，也不继续查找更晚的同名字段。匹配值strip后可能为空字符串；缺失分类只认None。迭代、匹配、命名组或字符串操作异常原样传播。生产调用以可重复遍历的splitlines列表分别提取状态与时间，不能把这种两次遍历前提推广到任意一次性iterator。
# ## References
# - scripts/syntax_status_records.py:121
# - scripts/syntax_status_records.py:122
# - scripts/syntax_status_records.py:123
# - scripts/syntax_status_records.py:125
# - scripts/syntax_status_records.py:126
# - scripts/syntax_status_records.py:138
# - scripts/syntax_status_records.py:142
# - scripts/syntax_status_records.py:143

# @unit collect_syntax_status_records
# ## Summary
# 只读收集仓库状态目录的记录，建立分组计数与缺失/未完成分区，供验证和输出共用一次扫描结果。
# ## Parameters
# @param[in] repository_root 仓库根Path；函数直接拼接docs/plans/syntax，不在此处resolve；相关目录和UTF-8文件在收集期间应稳定可读。
# ## Returns
# @return 成功返回新SyntaxStatusReport值快照。缺少状态根时选择集为空，仍返回空报告，冻结数量问题留给validate处理。
# ## Constraints and lifecycle
# 按选择器的路径顺序逐个UTF-8读取，分别查找首条状态和时间列表项；目录分组取首段路径在首个连字符之前的部分。只以None区分缺失，时间字符串不经格式验证；non_complete只纳入非None且完成属性为False的记录。目录计数按键排序构造。枚举、读取、解码或解析异常传播，不发布部分报告，不修改输入文件；多文件读取没有原子快照或并发同步。返回对象持有Python字符串和容器，源文件关闭或删除不使这些值失效。
# ## References
# - scripts/syntax_status_records.py:130
# - scripts/syntax_status_records.py:131
# - scripts/syntax_status_records.py:135
# - scripts/syntax_status_records.py:138
# - scripts/syntax_status_records.py:142
# - scripts/syntax_status_records.py:143
# - scripts/syntax_status_records.py:148
# - scripts/syntax_status_records.py:151
# - scripts/syntax_status_records.py:156
# - scripts/syntax_status_records.py:159
# - scripts/syntax_status_records.py:161
# - scripts/syntax_status_records.py:221
# - tests/scripts/test_syntax_status_records.py:76
# - tests/scripts/test_syntax_status_records.py:100
# - tests/scripts/test_syntax_status_records.py:116
# - tests/scripts/test_syntax_status_records.py:126

# @unit validate_syntax_status_records
# ## Summary
# 把已收集报告与冻结总数、目录分布及三类问题分区对照，一次返回所有发现的问题文本。
# ## Parameters
# @param[in] report 通常为collect返回的自洽报告；调用期间records、directory_counts和问题tuple须稳定。
# ## Returns
# @return 返回tuple[str, ...]；无所列问题则为空tuple。问题按总数、目录分布、缺失状态、非完成状态、缺失时间的检查顺序追加。
# ## Constraints and lifecycle
# 信任报告已经分类的路径分区，不重新读取文件、不重算这些分区或认证每条记录身份，也不检查时间格式、重复路径或任意手工报告的内部一致性。空tuple只表示所列检查均未报告问题，不能扩展为Markdown内容或完成事实的全面认证。此函数不修改报告或全局表；访问不符合类型约定的输入时异常可传播。
# ## References
# - scripts/syntax_status_records.py:169
# - scripts/syntax_status_records.py:171
# - scripts/syntax_status_records.py:175
# - scripts/syntax_status_records.py:180
# - scripts/syntax_status_records.py:182
# - scripts/syntax_status_records.py:184
# - scripts/syntax_status_records.py:186
# - scripts/syntax_status_records.py:222
# - tests/scripts/test_syntax_status_records.py:117
# - tests/scripts/test_syntax_status_records.py:120
# - tests/scripts/test_syntax_status_records.py:121
# - tests/scripts/test_syntax_status_records.py:122
# - tests/scripts/test_syntax_status_records.py:127
# - tests/scripts/test_syntax_status_records.py:132

# @unit _format_text
# ## Summary
# 将报告和已得到的问题集合格式化为固定统计键与ERROR行，供CLI人工模式使用。
# ## Parameters
# @param[in] report 与issues对应的报告；记录和计数在格式化期间稳定。
# @param[in] issues 通常来自同一次validate的tuple；按传入顺序呈现。
# ## Returns
# @return 返回含TOTAL、COMPLETE、MISSING_STATUS、NON_COMPLETE、MISSING_TIME、目录分布行及逐项ERROR行的str，末尾追加换行。
# ## Constraints and lifecycle
# 只格式化，不执行validate或写输出流。目录分布采用dict当前迭代顺序；collect构造时按键排序，但本函数不对手工dict重新排序。空分布仍留下一个空行。返回文本不拥有输入文件；下游stdout编码、短写或异常不由该函数控制。
# ## References
# - scripts/syntax_status_records.py:190
# - scripts/syntax_status_records.py:191
# - scripts/syntax_status_records.py:193
# - scripts/syntax_status_records.py:195
# - scripts/syntax_status_records.py:203
# - scripts/syntax_status_records.py:204
# - scripts/syntax_status_records.py:226

# @unit main
# ## Summary
# 提供状态门禁命令行入口，解析仓库根和输出格式，串联一次收集、验证和文本或JSON输出。
# ## Parameters
# @param[in] argv 可选参数字符串列表；None交由argparse读取sys.argv，传入列表本身不由本函数删改。
# ## Returns
# @return 正常完成输出后返回int：验证问题非空为1，否则为0；标准入口将该值交给SystemExit。
# ## Constraints and lifecycle
# 默认根为脚本所在scripts目录的父级，显式根也在收集前resolve。--format仅允许text/json；JSON同样会在存在问题时输出报告，不消除非零返回码。argparse帮助或参数错误可抛出SystemExit；文件访问、解码、序列化或stdout异常不转换为上述1返回码，输出流可能已部分写入。没有--output选项，也不强设stdout编码/换行。输入目录和全局政策须在调用期间稳定；普通导入不会调用main，但未禁字节码的Python导入仍可能产生缓存。
# ## References
# - scripts/syntax_status_records.py:208
# - scripts/syntax_status_records.py:213
# - scripts/syntax_status_records.py:215
# - scripts/syntax_status_records.py:218
# - scripts/syntax_status_records.py:219
# - scripts/syntax_status_records.py:221
# - scripts/syntax_status_records.py:222
# - scripts/syntax_status_records.py:224
# - scripts/syntax_status_records.py:226
# - scripts/syntax_status_records.py:227
# - scripts/syntax_status_records.py:231
# - docs/parser-and-semantics/current-syntax-convergence.md:198
