from __future__ import annotations

from collections import deque
import sys

MOD = 1_000_000_007
TIER_SCALES = {
    "smoke": 1,
    "core": 4,
    "stress": 16,
}


def parse_scale(argv: list[str]) -> int:
    tier = "core"
    index = 0
    while index < len(argv):
        arg = argv[index]
        if arg == "--tier":
            if index + 1 >= len(argv):
                raise SystemExit("--tier requires smoke, core, or stress")
            tier = argv[index + 1]
            index += 2
            continue
        raise SystemExit(f"unknown argument: {arg}")

    if tier not in TIER_SCALES:
        raise SystemExit(f"unsupported tier: {tier}")
    return TIER_SCALES[tier]


def numeric_loops(scale: int) -> int:
    outer_limit = 24 * scale
    inner_limit = 3000 * scale
    value = 17
    checksum = 0

    for outer in range(outer_limit):
        for inner in range(inner_limit):
            value = (value * 1103 + 97 + outer + inner) % 65521
            if value % 7 == 0:
                checksum += value // 7
            elif value % 5 == 0:
                checksum += value % 97
            else:
                checksum += value % 31
            checksum %= MOD
        checksum = (checksum + outer * 17 + value % 13) % MOD

    return checksum


class DispatchWorker:
    def step(self, delta: int) -> int:
        raise NotImplementedError

    def read(self) -> int:
        raise NotImplementedError


class MultiplyWorker(DispatchWorker):
    def __init__(self, seed: int) -> None:
        self.state = seed

    def step(self, delta: int) -> int:
        self.state = (self.state * 13 + delta + 7) % 10007
        return self.state

    def read(self) -> int:
        return self.state


class ScaleWorker(DispatchWorker):
    def __init__(self, seed: int) -> None:
        self.state = seed

    def step(self, delta: int) -> int:
        self.state = (self.state * 17 + delta * 3 + 11) % 10009
        return self.state

    def read(self) -> int:
        return self.state


class XorWorker(DispatchWorker):
    def __init__(self, seed: int) -> None:
        self.state = seed

    def step(self, delta: int) -> int:
        self.state = ((self.state ^ (delta + 31)) + delta * 5 + 19) % 10037
        return self.state

    def read(self) -> int:
        return self.state


class DriftWorker(DispatchWorker):
    def __init__(self, seed: int) -> None:
        self.state = seed

    def step(self, delta: int) -> int:
        self.state = (self.state + delta * delta + 23) % 10039
        return self.state

    def read(self) -> int:
        return self.state


def dispatch(worker: DispatchWorker, delta: int) -> int:
    return worker.step(delta)


def dispatch_loops(scale: int) -> int:
    workers = [
        MultiplyWorker(17),
        ScaleWorker(29),
        XorWorker(43),
        DriftWorker(61),
    ]
    outer_limit = 120 * scale
    inner_limit = 320 * scale
    checksum = 0

    for outer in range(outer_limit):
        for inner in range(inner_limit):
            index = (outer + inner) & 3
            delta = outer * 7 + inner * 11 + index
            value = dispatch(workers[index], delta)
            checksum = (checksum + value * (index + 1) + delta % 29) % MOD

        worker = workers[outer & 3]
        checksum = (checksum + worker.read() * (outer + 1)) % MOD

    return checksum


def container_label(value: int) -> str:
    if value % 2 == 0:
        return "even"
    if value > 128:
        return "odd_hi"
    return "odd_lo"


def container_pipeline(scale: int) -> int:
    total = 1024 * scale
    queue: deque[tuple[str, int]] = deque()
    seen: set[tuple[int, str]] = set()
    buckets: dict[str, list[int]] = {}
    seed = 41

    for index in range(total):
        seed = (seed * 29 + 17 + index) % 257
        queue.append((container_label(seed), seed * scale + (index % 13)))

    while queue:
        label, value = queue.popleft()
        seen.add((value, label))

    for value, label in seen:
        buckets.setdefault(label, []).append(value)

    odd_lo_sum = sum(buckets.get("odd_lo", []))
    odd_hi_sum = sum(buckets.get("odd_hi", []))
    even_sum = sum(buckets.get("even", []))
    unique_count = len(seen)
    checksum = even_sum * 100000 + odd_hi_sum * 100 + odd_lo_sum + unique_count
    return checksum % MOD


def insertion_sort(values: list[int]) -> None:
    for index in range(1, len(values)):
        key = values[index]
        cursor = index - 1
        while cursor >= 0 and values[cursor] > key:
            values[cursor + 1] = values[cursor]
            cursor -= 1
        values[cursor + 1] = key


def build_sort_pattern(pattern: int, length: int) -> list[int]:
    values: list[int] = []
    seed = 97

    if pattern == 0:
        for index in range(length):
            seed = (seed * 1103515245 + 12345 + index) % 2147483647
            values.append(seed % 100000)
        return values

    if pattern == 1:
        for index in range(length):
            values.append(length - index)
        return values

    if pattern == 2:
        for index in range(length):
            values.append((index * 17 + 3) % (length // 8 + 5))
        return values

    for index in range(length):
        values.append(index * 3 + (index % 7))
    for index in range(0, length, 7):
        swap_index = (index * 13 + 5) % length
        values[index], values[swap_index] = values[swap_index], values[index]
    return values


def sort_array(scale: int) -> int:
    length = 16 * scale
    step = length // 7
    checksum = 0
    if step < 1:
        step = 1

    for pattern in range(4):
        values = build_sort_pattern(pattern, length)
        insertion_sort(values)
        cursor = 0
        subtotal = 0
        while cursor < length:
            subtotal = (subtotal + values[cursor] * (cursor + 1)) % MOD
            cursor += step
        subtotal = (subtotal
                    + values[0] * 3
                    + values[length // 2] * 5
                    + values[length - 1] * 7
                    + pattern * 11) % MOD
        checksum = (checksum * 131 + subtotal) % MOD

    return checksum


def prime_trial_division(scale: int) -> int:
    limit = 5000 * scale
    checksum = 0
    count = 0

    for candidate in range(2, limit + 1):
        is_prime = True
        divisor = 2
        while divisor * divisor <= candidate:
            if candidate % divisor == 0:
                is_prime = False
                break
            divisor += 1

        if is_prime:
            count += 1
            checksum = (checksum + candidate * ((count % 97) + 1)) % MOD

    return checksum


def matrix_add_2d(scale: int) -> int:
    rows = 24 * scale
    cols = 32 * scale
    cells = rows * cols
    lhs = [0] * cells
    rhs = [0] * cells
    dst = [0] * cells
    scratch = [0] * cells
    checksum = 0

    for index in range(cells):
        lhs[index] = (index * 13 + 7) % 997
        rhs[index] = (index * 17 + 11) % 991

    for row in range(rows):
        row_sum = 0
        for col in range(cols):
            index = row * cols + col
            dst[index] = lhs[index] + rhs[index] + ((row + col) % 7)
            scratch[index] = dst[index] - lhs[index] // 3 + (rhs[index] % 11)
            row_sum += scratch[index] * (col + 1)
        checksum = (checksum + row_sum * (row + 1)) % MOD

    for index in range(cells):
        checksum = (checksum + scratch[index] * ((index % 17) + 1)) % MOD

    return checksum


def string_build(scale: int) -> int:
    fragments = ["al", "be", "cy", "do", "ex", "fu"]
    counts: dict[str, int] = {}
    keys: list[str] = []
    assembled = ""
    assembled_score = 0
    checksum = 0
    seed = 17
    iterations = 180 * scale

    for index in range(iterations):
        seed = (seed * 73 + 19 + index) % 997
        token_id = (seed + index) % 23
        token = (fragments[seed % len(fragments)]
                 + "-"
                 + fragments[token_id % len(fragments)]
                 + fragments[(token_id + 2) % len(fragments)])
        token_score = (seed % 211) + token_id * 17 + index
        assembled = assembled + token
        assembled_score = (assembled_score * 41 + token_score) % MOD
        if index % 4 == 0:
            assembled = assembled + "|"
            assembled_score = (assembled_score + 3) % MOD
        else:
            assembled = assembled + ":"
            assembled_score = (assembled_score + 7) % MOD

        if index % 9 == 8:
            if assembled not in counts:
                counts[assembled] = 0
                keys.append(assembled)
            counts[assembled] = (counts[assembled] + assembled_score + index + 1) % MOD
            checksum = (checksum + counts[assembled] + (seed % 97)) % MOD
            assembled = token
            assembled_score = token_score % MOD

    if assembled:
        if assembled not in counts:
            counts[assembled] = 0
            keys.append(assembled)
        counts[assembled] = (counts[assembled] + assembled_score + iterations) % MOD

    for index, key in enumerate(keys):
        checksum = (checksum + counts[key] * (index + 1)) % MOD

    return checksum


def map_object_access(scale: int) -> int:
    labels = ["aa", "bb", "cc", "dd"]
    buckets: dict[str, int] = {}
    checksum = 0
    left = 3
    right = 7
    hits = 0
    outer_limit = 64 * scale

    for outer in range(outer_limit):
        for inner in range(32):
            left = (left * 31 + outer + inner + hits) % 10007
            right = (right + left + inner * 3 + 5) % 10009
            hits += 1
            label = labels[(outer + inner) % len(labels)]
            key = label + "_slot"
            buckets[key] = (buckets.get(key, 0) + left + right + hits) % MOD
            checksum = (checksum + buckets[key] + left + hits) % MOD

    checksum = (
        checksum
        + buckets["aa_slot"]
        + buckets["bb_slot"]
        + buckets["cc_slot"]
        + buckets["dd_slot"]
    ) % MOD
    return checksum


CASE_HANDLERS = {
    "numeric_loops": ("BENCH_NUMERIC_LOOPS_PASS", numeric_loops),
    "dispatch_loops": ("BENCH_DISPATCH_LOOPS_PASS", dispatch_loops),
    "container_pipeline": ("BENCH_CONTAINER_PIPELINE_PASS", container_pipeline),
    "sort_array": ("BENCH_SORT_ARRAY_PASS", sort_array),
    "prime_trial_division": ("BENCH_PRIME_TRIAL_DIVISION_PASS", prime_trial_division),
    "matrix_add_2d": ("BENCH_MATRIX_ADD_2D_PASS", matrix_add_2d),
    "string_build": ("BENCH_STRING_BUILD_PASS", string_build),
    "map_object_access": ("BENCH_MAP_OBJECT_ACCESS_PASS", map_object_access),
}


def run_main(case_name: str) -> None:
    if case_name not in CASE_HANDLERS:
        raise SystemExit(f"unknown benchmark case: {case_name}")

    banner, handler = CASE_HANDLERS[case_name]
    scale = parse_scale(sys.argv[1:])
    checksum = handler(scale)
    print(banner)
    print(checksum)
