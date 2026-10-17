#!/usr/bin/env python3
"""Build deterministic filtered and shuffled benchmark execution plans."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Sequence

from benchmark_statistics import (
    SPLITMIX64_ALGORITHM,
    SPLITMIX64_VERSION,
    SplitMix64,
)


# 计划把洗牌算法与随机源版本显式序列化，避免将来实现变化重排旧种子。
EXECUTION_PLAN_SCHEMA_VERSION = 1
SHUFFLE_ALGORITHM = "fisher_yates_splitmix64"
SHUFFLE_VERSION = 1


def _validate_identity(value: Any, location: str) -> str:
    if not isinstance(value, str) or not value or value.strip() != value:
        raise ValueError(f"{location} must be a non-empty string without surrounding whitespace")
    return value


def validate_jobs(jobs: Any) -> tuple[dict[str, str], ...]:
    # case 与 implementation 的组合是作业身份，重复项会破坏执行次数契约。
    if not isinstance(jobs, list):
        raise ValueError("jobs must be a non-empty JSON array")
    if not jobs:
        raise ValueError("jobs must be a non-empty JSON array")

    validated: list[dict[str, str]] = []
    identities: set[tuple[str, str]] = set()
    for index, job in enumerate(jobs):
        location = f"jobs[{index}]"
        if not isinstance(job, dict):
            raise ValueError(f"{location} must be an object")
        fields = set(job)
        expected_fields = {"case", "implementation"}
        if fields != expected_fields:
            missing = sorted(expected_fields - fields)
            unknown = sorted(fields - expected_fields)
            details: list[str] = []
            if missing:
                details.append(f"missing {', '.join(missing)}")
            if unknown:
                details.append(f"unknown {', '.join(unknown)}")
            raise ValueError(f"{location} has invalid fields ({'; '.join(details)})")
        case = _validate_identity(job["case"], f"{location}.case")
        implementation = _validate_identity(
            job["implementation"],
            f"{location}.implementation",
        )
        identity = (case, implementation)
        if identity in identities:
            raise ValueError(
                f"duplicate job identity case={case!r}, implementation={implementation!r}"
            )
        identities.add(identity)
        validated.append({"case": case, "implementation": implementation})
    return tuple(validated)


def _validate_filter(values: Any, name: str) -> tuple[str, ...] | None:
    if values is None:
        return None
    if not isinstance(values, (list, tuple)) or not values:
        raise ValueError(f"{name} filter must be a non-empty array")
    validated: list[str] = []
    seen: set[str] = set()
    for index, value in enumerate(values):
        identity = _validate_identity(value, f"{name}[{index}]")
        if identity in seen:
            raise ValueError(f"{name} filter contains duplicate identity {identity!r}")
        seen.add(identity)
        validated.append(identity)
    return tuple(validated)


def _filter_validated_jobs(
    jobs: Sequence[dict[str, str]],
    cases: tuple[str, ...] | None,
    implementations: tuple[str, ...] | None,
) -> list[dict[str, str]]:
    # 先过滤再洗牌，种子对同一筛选结果才有稳定含义；返回新记录保护输入。
    case_filter = set(cases) if cases is not None else None
    implementation_filter = (
        set(implementations) if implementations is not None else None
    )
    filtered = [
        dict(job)
        for job in jobs
        if (case_filter is None or job["case"] in case_filter)
        and (
            implementation_filter is None
            or job["implementation"] in implementation_filter
        )
    ]
    if not filtered:
        raise ValueError("filters select no benchmark jobs")
    return filtered


def filter_jobs(
    jobs: Any,
    *,
    cases: Any = None,
    implementations: Any = None,
) -> list[dict[str, str]]:
    validated_jobs = validate_jobs(jobs)
    validated_cases = _validate_filter(cases, "cases")
    validated_implementations = _validate_filter(
        implementations,
        "implementations",
    )
    return _filter_validated_jobs(
        validated_jobs,
        validated_cases,
        validated_implementations,
    )


def _shuffle_validated_jobs(
    jobs: Sequence[dict[str, str]],
    seed: int,
) -> list[dict[str, str]]:
    # 使用版本化 SplitMix64 驱动 Fisher–Yates，顺序须与 CMake 执行入口一致。
    generator = SplitMix64(seed)
    shuffled = [dict(job) for job in jobs]
    for index in range(len(shuffled) - 1, 0, -1):
        swap_index = generator.next_uint64() % (index + 1)
        shuffled[index], shuffled[swap_index] = (
            shuffled[swap_index],
            shuffled[index],
        )
    return shuffled


def shuffle_jobs(jobs: Any, *, seed: int) -> list[dict[str, str]]:
    return _shuffle_validated_jobs(validate_jobs(jobs), seed)


def create_execution_plan(
    jobs: Any,
    *,
    seed: int,
    cases: Any = None,
    implementations: Any = None,
) -> dict[str, Any]:
    # 计划保留筛选条件、随机算法版本与结果顺序，供套件复跑和审计。
    validated_jobs = validate_jobs(jobs)
    validated_cases = _validate_filter(cases, "cases")
    validated_implementations = _validate_filter(
        implementations,
        "implementations",
    )
    filtered_jobs = _filter_validated_jobs(
        validated_jobs,
        validated_cases,
        validated_implementations,
    )
    shuffled_jobs = _shuffle_validated_jobs(filtered_jobs, seed)
    return {
        "schema_version": EXECUTION_PLAN_SCHEMA_VERSION,
        "algorithm": SHUFFLE_ALGORITHM,
        "version": SHUFFLE_VERSION,
        "rng": {
            "algorithm": SPLITMIX64_ALGORITHM,
            "version": SPLITMIX64_VERSION,
        },
        "seed": seed,
        "filters": {
            "cases": list(validated_cases) if validated_cases is not None else None,
            "implementations": (
                list(validated_implementations)
                if validated_implementations is not None
                else None
            ),
        },
        "job_count": len(shuffled_jobs),
        "jobs": shuffled_jobs,
    }


# 兼容调用方保留旧入口名，与主构造函数共享同一实现。
build_execution_plan = create_execution_plan


def _serialize_payload(payload: Any) -> bytes:
    rendered = json.dumps(
        payload,
        ensure_ascii=True,
        allow_nan=False,
        sort_keys=True,
        separators=(",", ":"),
    )
    return (rendered + "\n").encode("utf-8")


def serialize_execution_plan(
    jobs: Any,
    *,
    seed: int,
    cases: Any = None,
    implementations: Any = None,
) -> bytes:
    return _serialize_payload(
        create_execution_plan(
            jobs,
            seed=seed,
            cases=cases,
            implementations=implementations,
        )
    )


def _reject_json_constant(value: str) -> None:
    raise ValueError(f"invalid JSON constant {value!r}")


def _object_without_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON field {key!r}")
        result[key] = value
    return result


def _parse_request(raw: bytes) -> tuple[Any, int, Any, Any]:
    # 拒绝重复键、未知字段和非标准常量，避免 CLI 与 API 解读不一致。
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ValueError("input is not valid UTF-8") from exc
    try:
        request = json.loads(
            text,
            parse_constant=_reject_json_constant,
            object_pairs_hook=_object_without_duplicate_keys,
        )
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid JSON: {exc.msg}") from exc
    if not isinstance(request, dict):
        raise ValueError("input must be a JSON object")
    allowed_fields = {"jobs", "seed", "filters"}
    unknown_fields = sorted(set(request) - allowed_fields)
    if unknown_fields:
        raise ValueError(f"unknown input field(s): {', '.join(unknown_fields)}")
    missing_fields = sorted({"jobs", "seed"} - set(request))
    if missing_fields:
        raise ValueError(f"missing input field(s): {', '.join(missing_fields)}")

    filters = request.get("filters", {})
    if not isinstance(filters, dict):
        raise ValueError("filters must be an object")
    unknown_filter_fields = sorted(set(filters) - {"cases", "implementations"})
    if unknown_filter_fields:
        raise ValueError(
            f"unknown filter field(s): {', '.join(unknown_filter_fields)}"
        )
    return (
        request["jobs"],
        request["seed"],
        filters.get("cases"),
        filters.get("implementations"),
    )


def _read_input(path: str) -> bytes:
    if path == "-":
        return sys.stdin.buffer.read()
    return Path(path).read_bytes()


def _write_output(path: str, payload: bytes) -> None:
    if path == "-":
        sys.stdout.buffer.write(payload)
        return
    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(payload)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Filter and deterministically shuffle benchmark jobs from JSON."
    )
    parser.add_argument(
        "--input",
        default="-",
        help="Input JSON path, or - for stdin (default: -).",
    )
    parser.add_argument(
        "--output",
        default="-",
        help="Output JSON path, or - for stdout (default: -).",
    )
    args = parser.parse_args()
    try:
        jobs, seed, cases, implementations = _parse_request(_read_input(args.input))
        payload = serialize_execution_plan(
            jobs,
            seed=seed,
            cases=cases,
            implementations=implementations,
        )
        _write_output(args.output, payload)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# @unit EXECUTION_PLAN_SCHEMA_VERSION
# ## Summary
# 标识执行计划JSON对象的字段协议，供计划生产者与CMake消费方核对版本。
# ## Constraints and lifecycle
# 当前值为1；它是显式维护的协议常量，不是运行时能力探测。更改字段协议须联动消费方校验与固定场景，不能仅改标记而假定兼容。普通整数不持有外部资源；一次构造/序列化期间不应重绑定模块配置。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:20
# - scripts/benchmark/benchmark_execution_plan.py:170
# - tests/benchmarks/test_benchmark_execution_plan.py:20
# - tests/benchmarks/test_benchmark_execution_plan.py:54
# - tests/cmake/benchmark_task3_suite.cmake:4
# - tests/cmake/benchmark_task3_suite.cmake:143

# @unit SHUFFLE_ALGORITHM
# ## Summary
# 给计划记录确定性洗牌算法名称，防止消费方把作业顺序当作未声明的任意排列。
# ## Constraints and lifecycle
# 当前名称fisher_yates_splitmix64对应本模块的逆序Fisher–Yates交换及借用的SplitMix64序列；名称本身不验证实现、加密强度或统计无偏性。CMake另与自己的预期字符串比较，调用期间应保持配置稳定。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:21
# - scripts/benchmark/benchmark_execution_plan.py:171
# - scripts/benchmark/benchmark_execution_plan.py:134
# - scripts/benchmark/benchmark_execution_plan.py:137
# - scripts/benchmark/benchmark_statistics.py:37
# - tests/cmake/benchmark_task3_suite.cmake:5
# - tests/cmake/benchmark_task3_suite.cmake:144
# - tests/benchmarks/test_benchmark_execution_plan.py:55

# @unit SHUFFLE_VERSION
# ## Summary
# 记录当前洗牌过程的版本，与随机源版本分别进入执行计划。
# ## Constraints and lifecycle
# 当前值为1；计划version与rng.version是两个字段，后者借用benchmark_statistics的常量。算法、迭代或取下标规则变化须核对版本和固定向量；配置按只读使用，不承担文件资源生命周期。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:22
# - scripts/benchmark/benchmark_execution_plan.py:172
# - scripts/benchmark/benchmark_execution_plan.py:175
# - scripts/benchmark/benchmark_statistics.py:20
# - tests/cmake/benchmark_task3_suite.cmake:6
# - tests/cmake/benchmark_task3_suite.cmake:145
# - tests/benchmarks/test_benchmark_execution_plan.py:56

# @unit _validate_identity
# ## Summary
# 在作业和过滤条件入口统一检查身份字符串，保留调用方提供的上下文位置用于错误信息。
# ## Parameters
# @param[in] value 待验证的普通JSON形状值。
# @param[in] location 诊断位置字符串，仅用于构造失败文本。
# ## Returns
# @return 成功返回原str对象；空、非str或带首尾空白的普通输入抛ValueError。
# ## Constraints and lifecycle
# 不规范化或复制字符串，不移除空白；字符串内部内容和字符集不受额外限制，不认证路径、已注册case或实现名称。只读参数且无I/O；自定义Python子类操作引发的其他异常不统一包装。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:25
# - scripts/benchmark/benchmark_execution_plan.py:26
# - scripts/benchmark/benchmark_execution_plan.py:55
# - scripts/benchmark/benchmark_execution_plan.py:56
# - scripts/benchmark/benchmark_execution_plan.py:78
# - tests/benchmarks/test_benchmark_execution_plan.py:104
# - tests/benchmarks/test_benchmark_execution_plan.py:106
# - tests/benchmarks/test_benchmark_execution_plan.py:108

# @unit validate_jobs
# ## Summary
# 在过滤或洗牌前验证非空作业列表及唯一复合身份，建立与输入字典分离的记录集合。
# ## Parameters
# @param[in] jobs 普通JSON数组形状的list；各元素应为含字符串键的dict。
# ## Returns
# @return 成功返回tuple[dict[str,str], ...]，每项是新字典；普通形状不满足非空、精确字段、身份或唯一性时抛ValueError。
# ## Constraints and lifecycle
# 只接受list而非任意Sequence，每项必须恰有case和implementation；复合身份按两字符串组合去重，单独case或implementation可在不同组合重复。保留原输入顺序，不修改列表或记录；值通过字符串校验后可安全共享不可变字符串引用。检查中构造的部分局部结果不会返回。任意非JSON键或自定义对象仍可能在排序、集合或join操作抛TypeError等，不能承诺所有Python Any输入都转成ValueError；调用期间输入须稳定。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:31
# - scripts/benchmark/benchmark_execution_plan.py:33
# - scripts/benchmark/benchmark_execution_plan.py:42
# - scripts/benchmark/benchmark_execution_plan.py:46
# - scripts/benchmark/benchmark_execution_plan.py:55
# - scripts/benchmark/benchmark_execution_plan.py:61
# - scripts/benchmark/benchmark_execution_plan.py:66
# - scripts/benchmark/benchmark_execution_plan.py:116
# - scripts/benchmark/benchmark_execution_plan.py:146
# - scripts/benchmark/benchmark_execution_plan.py:157
# - tests/benchmarks/test_benchmark_execution_plan.py:98
# - tests/benchmarks/test_benchmark_execution_plan.py:118

# @unit _validate_filter
# ## Summary
# 把可选过滤条件验证为顺序稳定的身份tuple，供筛选和计划元数据共同使用。
# ## Parameters
# @param[in] values None或非空list/tuple身份序列。
# @param[in] name 诊断字段名，例如cases或implementations。
# ## Returns
# @return None返回None；合法序列返回新tuple[str,...]；普通非法形状、无效身份或重复项抛ValueError。
# ## Constraints and lifecycle
# None表示不限制该维度，空序列被拒绝而不表示选择零项。保留过滤条件原顺序，尚不检查是否存在匹配作业；无匹配由后续过滤器拒绝。对输入只读，无资源句柄或缓存；调用期间序列保持稳定。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:70
# - scripts/benchmark/benchmark_execution_plan.py:71
# - scripts/benchmark/benchmark_execution_plan.py:73
# - scripts/benchmark/benchmark_execution_plan.py:78
# - scripts/benchmark/benchmark_execution_plan.py:79
# - scripts/benchmark/benchmark_execution_plan.py:117
# - scripts/benchmark/benchmark_execution_plan.py:118
# - scripts/benchmark/benchmark_execution_plan.py:158
# - scripts/benchmark/benchmark_execution_plan.py:159
# - tests/benchmarks/test_benchmark_execution_plan.py:126
# - tests/benchmarks/test_benchmark_execution_plan.py:137

# @unit _filter_validated_jobs
# ## Summary
# 在洗牌前按两个维度的可选身份集合筛选作业，并复制保留的记录。
# ## Parameters
# @param[in] jobs 已通过validate_jobs的稳定作业Sequence。
# @param[in] cases 已验证的case tuple或None。
# @param[in] implementations 已验证的implementation tuple或None。
# ## Returns
# @return 返回保持原作业相对顺序的新list和新dict；没有任何匹配作业时抛ValueError。
# ## Constraints and lifecycle
# 两个非None条件以AND组合；过滤序列的排列不重排jobs。只读输入，不再次执行身份或重复检查，依赖上层已验证普通记录；dict复制是浅复制，而已验证记录仅含字符串值。部分候选局部构造失败不发布结果，输入和其他已存在计划不被修改。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:86
# - scripts/benchmark/benchmark_execution_plan.py:92
# - scripts/benchmark/benchmark_execution_plan.py:97
# - scripts/benchmark/benchmark_execution_plan.py:99
# - scripts/benchmark/benchmark_execution_plan.py:105
# - scripts/benchmark/benchmark_execution_plan.py:122
# - scripts/benchmark/benchmark_execution_plan.py:163
# - tests/benchmarks/test_benchmark_execution_plan.py:69
# - tests/benchmarks/test_benchmark_execution_plan.py:140

# @unit filter_jobs
# ## Summary
# 提供独立筛选API，先验证作业和两个过滤维度，再返回筛选结果。
# ## Parameters
# @param[in] jobs 待验证的非空作业list。
# @param[in] cases 关键字可选过滤条件，None为不限制。
# @param[in] implementations 关键字可选过滤条件，None为不限制。
# ## Returns
# @return 返回筛选后新作业list；验证或零匹配的ValueError等异常直接传播。
# ## Constraints and lifecycle
# 此入口不洗牌、不使用seed、不生成协议元数据，也不执行作业。按照作业、cases、implementations、实际筛选的顺序检查；不修改调用方容器。当前仓内未找到调用该公开拼写的程序入口，外部Python消费未知，不据此断言无用。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:110
# - scripts/benchmark/benchmark_execution_plan.py:116
# - scripts/benchmark/benchmark_execution_plan.py:117
# - scripts/benchmark/benchmark_execution_plan.py:118
# - scripts/benchmark/benchmark_execution_plan.py:122

# @unit _shuffle_validated_jobs
# ## Summary
# 用一次局部SplitMix64随机源对已验证作业执行版本化的逆序Fisher–Yates交换。
# ## Parameters
# @param[in] jobs 已验证的稳定作业Sequence；每项只含两个字符串字段。
# @param[in] seed 整数uint64范围且非bool；由借用SplitMix64构造器验证。
# ## Returns
# @return 成功返回新list及复制的作业dict；无效seed在创建随机源时抛ValueError。
# ## Constraints and lifecycle
# 随机源局部创建，不消耗全局random状态；先复制作业，再从末下标向1递减，以next_uint64()%(index+1)选交换位置。相同输入顺序、seed及相同算法/随机源实现产生相同排列，不承诺不同seed必不同，也不是密码随机或统计无偏认证。此私有入口不复查作业身份，空Sequence会返回空list；公开入口另拒绝空作业。失败不返回局部候选或修改输入，无I/O。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:129
# - scripts/benchmark/benchmark_execution_plan.py:134
# - scripts/benchmark/benchmark_execution_plan.py:135
# - scripts/benchmark/benchmark_execution_plan.py:136
# - scripts/benchmark/benchmark_execution_plan.py:137
# - scripts/benchmark/benchmark_execution_plan.py:146
# - scripts/benchmark/benchmark_execution_plan.py:168
# - scripts/benchmark/benchmark_statistics.py:34
# - scripts/benchmark/benchmark_statistics.py:45
# - scripts/benchmark/benchmark_statistics.py:48
# - tests/benchmarks/test_benchmark_execution_plan.py:94
# - tests/benchmarks/test_benchmark_execution_plan.py:96

# @unit shuffle_jobs
# ## Summary
# 提供不带过滤或计划元数据的独立洗牌API，先验证作业再调用固定算法。
# ## Parameters
# @param[in] jobs 待验证的非空作业list。
# @param[in] seed 必需关键字seed，非bool的uint64整数。
# ## Returns
# @return 返回新作业list及新dict；作业检查或随机源seed检查异常传播。
# ## Constraints and lifecycle
# 验证发生在构造随机源之前；原输入顺序和容器不改。此入口不执行作业，不生成schema或filters字段。确定性依赖相同输入及借用随机源版本；调用方保持输入稳定。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:145
# - scripts/benchmark/benchmark_execution_plan.py:146
# - scripts/benchmark/benchmark_statistics.py:45
# - tests/benchmarks/test_benchmark_execution_plan.py:94
# - tests/benchmarks/test_benchmark_execution_plan.py:96

# @unit create_execution_plan
# ## Summary
# 构造先过滤后洗牌的执行计划，同时记录筛选条件、seed及版本化顺序协议供套件复跑。
# ## Parameters
# @param[in] jobs 待验证非空作业list。
# @param[in] seed 必需关键字seed，非bool的uint64整数。
# @param[in] cases 可选非空case过滤list/tuple；None表示不限制。
# @param[in] implementations 可选非空implementation过滤list/tuple；None表示不限制。
# ## Returns
# @return 成功返回新dict，含schema_version、algorithm/version、rng版本、seed、filters、job_count及jobs；验证或筛选/随机源失败不返回计划。
# ## Constraints and lifecycle
# 依次检查作业、两个过滤条件、非空筛选结果，最后验证seed并洗牌；较早错误可先于seed错误出现。记录数组与过滤条件列表均新建，原容器不修改，字符串标量可共享；结果普通dict/list可由调用方修改，不是冻结对象。只生成顺序，不运行基准、不验证case在registry中存在，也不写文件或取得多文件快照。外部修改输入或模块配置时无同步保证。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:149
# - scripts/benchmark/benchmark_execution_plan.py:157
# - scripts/benchmark/benchmark_execution_plan.py:158
# - scripts/benchmark/benchmark_execution_plan.py:163
# - scripts/benchmark/benchmark_execution_plan.py:168
# - scripts/benchmark/benchmark_execution_plan.py:170
# - scripts/benchmark/benchmark_execution_plan.py:174
# - scripts/benchmark/benchmark_execution_plan.py:177
# - scripts/benchmark/benchmark_execution_plan.py:179
# - scripts/benchmark/benchmark_execution_plan.py:186
# - scripts/benchmark/benchmark_execution_plan.py:192
# - scripts/benchmark/benchmark_execution_plan.py:214
# - tests/benchmarks/test_benchmark_execution_plan.py:49
# - tests/benchmarks/test_benchmark_execution_plan.py:50
# - tests/benchmarks/test_benchmark_execution_plan.py:61
# - tests/benchmarks/test_benchmark_execution_plan.py:62
# - tests/benchmarks/test_benchmark_execution_plan.py:69
# - tests/benchmarks/test_benchmark_execution_plan.py:118
# - tests/benchmarks/test_benchmark_execution_plan.py:124
# - tests/benchmarks/test_benchmark_execution_plan.py:137
# - tests/benchmarks/test_benchmark_execution_plan.py:140

# @unit build_execution_plan
# ## Summary
# 保留旧入口名称，把它绑定到create_execution_plan的同一函数对象，供兼容消费方使用。
# ## Constraints and lifecycle
# 这是模块加载时的callable alias赋值，不是第二个函数定义、包装器或独立验证层；参数、返回和失败合同由被绑定的create_execution_plan承担。当前仓内未检索到此别名的调用，不能排除仓外导入。后续若重新绑定某个模块名称，普通Python赋值不会自动重定向另一个已绑定名称。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:191
# - scripts/benchmark/benchmark_execution_plan.py:192
# - scripts/benchmark/benchmark_execution_plan.py:149

# @unit _serialize_payload
# ## Summary
# 把已构造的载荷编码为确定JSON字节，供执行计划序列化复用。
# ## Parameters
# @param[in] payload 普通可JSON编码载荷；调用期间保持稳定。
# ## Returns
# @return 返回UTF-8 bytes，以一个LF结束；JSON使用ASCII转义、键排序及紧凑分隔符。
# ## Constraints and lifecycle
# allow_nan=False拒绝非有限浮点常量；不校验payload是否符合执行计划schema。不可序列化对象、循环容器或其他JSON编码问题可抛TypeError/ValueError等，异常不包装。不写流或文件；返回bytes独立于输入容器后续更改，编码前并发修改无同步。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:195
# - scripts/benchmark/benchmark_execution_plan.py:196
# - scripts/benchmark/benchmark_execution_plan.py:198
# - scripts/benchmark/benchmark_execution_plan.py:199
# - scripts/benchmark/benchmark_execution_plan.py:200
# - scripts/benchmark/benchmark_execution_plan.py:201
# - scripts/benchmark/benchmark_execution_plan.py:203
# - scripts/benchmark/benchmark_execution_plan.py:213
# - tests/benchmarks/test_benchmark_execution_plan.py:143
# - tests/benchmarks/test_benchmark_execution_plan.py:146
# - tests/benchmarks/test_benchmark_execution_plan.py:147
# - tests/benchmarks/test_benchmark_execution_plan.py:148

# @unit serialize_execution_plan
# ## Summary
# 串联完整计划构造与固定字节编码，作为CLI和机器消费方的单次序列化入口。
# ## Parameters
# @param[in] jobs 待验证的非空作业list。
# @param[in] seed 必需关键字uint64整数，拒绝bool。
# @param[in] cases 可选过滤list/tuple或None。
# @param[in] implementations 可选过滤list/tuple或None。
# ## Returns
# @return 返回完整新bytes载荷；构造计划或编码异常直接传播，不返回部分bytes。
# ## Constraints and lifecycle
# 先完整生成计划再编码；不运行基准、不写输入容器或输出文件。确定字节依赖相同输入顺序、过滤、seed、算法及编码规则；对输出文件的事务性不由此纯值入口保证。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:206
# - scripts/benchmark/benchmark_execution_plan.py:213
# - scripts/benchmark/benchmark_execution_plan.py:214
# - scripts/benchmark/benchmark_execution_plan.py:308
# - tests/benchmarks/test_benchmark_execution_plan.py:143
# - tests/benchmarks/test_benchmark_execution_plan.py:144
# - tests/benchmarks/test_benchmark_execution_plan.py:146

# @unit _reject_json_constant
# ## Summary
# 作为json.loads回调拒绝NaN或Infinity等非标准JSON常量，使请求不能从宽松扩展入口进入计划构造。
# ## Parameters
# @param[in] value json解码器给出的非标准常量拼写。
# ## Returns
# 没有正常返回；始终抛ValueError，不生成替代值。
# ## Constraints and lifecycle
# 通过parse_constant注册，回调异常直接穿过_parse_request再由main的ValueError分支处理。它不检查普通数值字面量的全部语义或seed范围；这些仍由后续层负责。无I/O和对外状态发布。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:223
# - scripts/benchmark/benchmark_execution_plan.py:224
# - scripts/benchmark/benchmark_execution_plan.py:243
# - scripts/benchmark/benchmark_execution_plan.py:245
# - scripts/benchmark/benchmark_execution_plan.py:315

# @unit _object_without_duplicate_keys
# ## Summary
# 作为JSON对象解码hook在保留键值时拒绝同一对象内的重复字段，避免后值静默覆盖先值。
# ## Parameters
# @param[in] pairs 解码器按对象文本顺序给出的键值对list；JSON对象键为str。
# ## Returns
# @return 成功返回新dict；遇到重复键时抛ValueError，局部部分dict不作为返回值发布。
# ## Constraints and lifecycle
# object_pairs_hook对嵌套对象也生效，而不是只检查根request；值对象引用按普通容器构造保存，不做深复制或schema检查。手工调用须提供可哈希键，其他Python操作异常不统一包装。无文件资源或并发锁。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:227
# - scripts/benchmark/benchmark_execution_plan.py:229
# - scripts/benchmark/benchmark_execution_plan.py:230
# - scripts/benchmark/benchmark_execution_plan.py:231
# - scripts/benchmark/benchmark_execution_plan.py:246

# @unit _parse_request
# ## Summary
# 把原始UTF-8 JSON请求解码为作业、seed和两个过滤参数，并拒绝请求/filters结构中的未知或缺失字段。
# ## Parameters
# @param[in] raw 完整输入bytes；调用期间稳定，不包含文件句柄。
# ## Returns
# @return 成功返回(jobs, seed, cases, implementations)四元组；UTF-8、JSON语法、对象结构和字段错误抛ValueError。
# ## Constraints and lifecycle
# 先UTF-8解码，解码错误转ValueError；JSON语法错误转带消息的ValueError，两个严格JSON回调的ValueError直接传播。根须为dict且只允许jobs/seed/filters，jobs和seed必需；filters省略为{}，显式null或非对象被拒绝，且只允许cases/implementations。返回的jobs和seed仍未完成语义校验，int返回注解不保证seed已合法；过滤内容、空作业和seed范围留给create/随机源检查。只建立Python值，不执行作业或I/O，不承诺转换所有内存/递归/自定义输入异常。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:236
# - scripts/benchmark/benchmark_execution_plan.py:239
# - scripts/benchmark/benchmark_execution_plan.py:240
# - scripts/benchmark/benchmark_execution_plan.py:243
# - scripts/benchmark/benchmark_execution_plan.py:245
# - scripts/benchmark/benchmark_execution_plan.py:246
# - scripts/benchmark/benchmark_execution_plan.py:248
# - scripts/benchmark/benchmark_execution_plan.py:250
# - scripts/benchmark/benchmark_execution_plan.py:252
# - scripts/benchmark/benchmark_execution_plan.py:256
# - scripts/benchmark/benchmark_execution_plan.py:260
# - scripts/benchmark/benchmark_execution_plan.py:263
# - scripts/benchmark/benchmark_execution_plan.py:268
# - scripts/benchmark/benchmark_execution_plan.py:307
# - tests/benchmarks/test_benchmark_execution_plan.py:199
# - tests/benchmarks/test_benchmark_execution_plan.py:200
# - tests/benchmarks/test_benchmark_execution_plan.py:202
# - tests/benchmarks/test_benchmark_execution_plan.py:203

# @unit _read_input
# ## Summary
# 为CLI一次读完标准输入或指定文件，提供后续严格解析所需的完整字节。
# ## Parameters
# @param[in] path "-"表示借用sys.stdin.buffer；其他字符串作为Path直接读取。
# ## Returns
# @return 成功返回bytes；文件或流读取异常直接传播。
# ## Constraints and lifecycle
# stdin分支不关闭调用方标准流；Path.read_bytes管理自己打开的文件。没有大小上限、路径沙箱或目录创建，按当前工作目录解释相对路径。调用方须提供二进制接口并协调共享流读者；此函数不解析JSON，不修改输入文件。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:276
# - scripts/benchmark/benchmark_execution_plan.py:277
# - scripts/benchmark/benchmark_execution_plan.py:278
# - scripts/benchmark/benchmark_execution_plan.py:279
# - scripts/benchmark/benchmark_execution_plan.py:307
# - tests/benchmarks/test_benchmark_execution_plan.py:159
# - tests/benchmarks/test_benchmark_execution_plan.py:183
# - tests/benchmarks/test_benchmark_execution_plan.py:185
# - tests/cmake/benchmark_task3_suite.cmake:123

# @unit _write_output
# ## Summary
# 把已完成序列化的字节送到标准输出或指定路径，作为CLI的最后发布步骤。
# ## Parameters
# @param[in] path "-"表示借用sys.stdout.buffer；其他字符串按Path解释。
# @param[in] payload 调用方准备好的bytes，本函数不校验其schema或编码。
# ## Returns
# 正常完成返回None；文件/流操作异常传播。文件分支可能创建父目录并覆盖已有输出。
# ## Constraints and lifecycle
# 标准流不关闭也不显式flush，单次write的计数字段不在此验证。文件分支mkdir后直接write_bytes，不采用临时文件替换、回滚或旧文件保留事务；失败时可能留下父目录或已截断/部分输出。路径由调用方授权，不做E根、沙箱或归属认证；应使用独占输出路径并避免并发写者。原始payload不修改。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:282
# - scripts/benchmark/benchmark_execution_plan.py:283
# - scripts/benchmark/benchmark_execution_plan.py:284
# - scripts/benchmark/benchmark_execution_plan.py:286
# - scripts/benchmark/benchmark_execution_plan.py:287
# - scripts/benchmark/benchmark_execution_plan.py:288
# - scripts/benchmark/benchmark_execution_plan.py:314
# - tests/benchmarks/test_benchmark_execution_plan.py:186
# - tests/benchmarks/test_benchmark_execution_plan.py:193
# - tests/benchmarks/test_benchmark_execution_plan.py:194
# - tests/benchmarks/test_benchmark_execution_plan.py:195
# - tests/cmake/benchmark_task3_suite.cmake:123

# @unit main
# ## Summary
# 解析CLI输入输出路径，完成读取、严格请求解析、计划生成和最后写出，并把已捕获错误转为稳定退出码。
# ## Parameters
# 无显式参数；由CLI读取进程sys.argv。
# ## Returns
# @return 正常写出返回0；try内捕获OSError或ValueError后向stderr写error:前缀并返回2。
# ## Constraints and lifecycle
# 参数从sys.argv读取，input/output默认"-"；argparse在try之外，可用SystemExit处理帮助或参数错误。请求和序列化全部成功后才调用输出函数，因此此前失败不写目标文件；写出阶段自身失败不回滚旧目标。输入输出同路径时先完整读取再覆盖。这里只捕获两类异常，其他异常及stderr自身写失败可能向外传播；不关闭借用stdio、不强制超时或缓存策略。直接执行可能导入benchmark_statistics并产生字节码缓存，禁缓存应由启动环境明确设置。
# ## References
# - scripts/benchmark/benchmark_execution_plan.py:291
# - scripts/benchmark/benchmark_execution_plan.py:295
# - scripts/benchmark/benchmark_execution_plan.py:300
# - scripts/benchmark/benchmark_execution_plan.py:305
# - scripts/benchmark/benchmark_execution_plan.py:307
# - scripts/benchmark/benchmark_execution_plan.py:308
# - scripts/benchmark/benchmark_execution_plan.py:314
# - scripts/benchmark/benchmark_execution_plan.py:315
# - scripts/benchmark/benchmark_execution_plan.py:316
# - scripts/benchmark/benchmark_execution_plan.py:317
# - scripts/benchmark/benchmark_execution_plan.py:318
# - scripts/benchmark/benchmark_execution_plan.py:322
# - tests/benchmarks/test_benchmark_execution_plan.py:159
# - tests/benchmarks/test_benchmark_execution_plan.py:165
# - tests/benchmarks/test_benchmark_execution_plan.py:183
# - tests/benchmarks/test_benchmark_execution_plan.py:209
# - tests/cmake/benchmark_task3_suite.cmake:122
