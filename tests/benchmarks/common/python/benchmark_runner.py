from __future__ import annotations

from collections import deque
import sys

MOD = 1_000_000_007
TIER_SCALES = {
    "smoke": 1,
    "core": 4,
    "stress": 16,
    "profile": 1,
}


def parse_scale(argv: list[str]) -> int:
    tier = "core"
    explicit_scale: int | None = None
    index = 0
    while index < len(argv):
        arg = argv[index]
        if arg == "--tier":
            if index + 1 >= len(argv):
                raise SystemExit("--tier requires smoke, core, or stress")
            tier = argv[index + 1]
            index += 2
            continue
        if arg == "--scale":
            if index + 1 >= len(argv):
                raise SystemExit("--scale requires a positive integer")
            try:
                explicit_scale = int(argv[index + 1], 10)
            except ValueError as exc:
                raise SystemExit("--scale requires a positive integer") from exc
            if explicit_scale < 1:
                raise SystemExit("--scale requires a positive integer")
            index += 2
            continue
        raise SystemExit(f"unknown argument: {arg}")

    if explicit_scale is not None:
        return explicit_scale
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


def fib_recursive_value(n: int) -> int:
    if n <= 1:
        return n
    return fib_recursive_value(n - 1) + fib_recursive_value(n - 2)


def fib_recursive(scale: int) -> int:
    rounds = 18 * scale
    checksum = 0

    for index in range(rounds):
        n = 13 + (index % 6)
        value = fib_recursive_value(n)
        checksum = (checksum * 131 + value * (index + 3) + n) % MOD

    return checksum


class PolyAdder:
    def __init__(self, base: int) -> None:
        self.base = base

    def __call__(self, value: int, delta: int) -> int:
        return (value + delta + self.base) % 100003


class PolyMultiply:
    def __init__(self, factor: int) -> None:
        self.factor = factor

    def __call__(self, value: int, delta: int) -> int:
        return (value * (self.factor + 3) + delta + 7) % 100003


class PolyXor:
    def __init__(self, mask: int) -> None:
        self.mask = mask

    def __call__(self, value: int, delta: int) -> int:
        return ((value ^ (delta + self.mask)) + self.mask * 5 + delta) % 100003


def call_leaf(value: int, salt: int) -> int:
    return (value * 17 + salt * 13 + 19) % 100003


def call_chain_a(value: int, salt: int) -> int:
    return call_leaf(value + 3, salt + 1)


def call_chain_b(value: int, salt: int) -> int:
    return call_leaf(call_chain_a(value + (salt % 5), salt + 7), salt + 11)


def call_chain_c(value: int, salt: int) -> int:
    return call_leaf(call_chain_b(value ^ salt, salt + 13), salt + 17)


def tail_accumulate(steps: int, acc: int) -> int:
    if steps == 0:
        return acc
    return tail_accumulate(steps - 1, (acc * 3 + steps + 5) % 100003)


def dispatch_callable(callable_obj, value: int, delta: int) -> int:
    return callable_obj(value, delta)


def call_chain_polymorphic(scale: int) -> int:
    rounds = 320 * scale
    adder = PolyAdder(17)
    multiply = PolyMultiply(23)
    xor_call = PolyXor(31)
    state = 17
    checksum = 0

    for outer in range(rounds):
        delta = outer * 7 + (state % 13)
        selector = outer % 3
        if selector == 0:
            state = dispatch_callable(adder, call_chain_a(state, delta), delta)
        elif selector == 1:
            state = dispatch_callable(multiply, call_chain_b(state, delta), delta)
        else:
            state = dispatch_callable(xor_call, call_chain_c(state, delta), delta)

        checksum = (checksum + state * (selector + 1) + tail_accumulate((outer % 5) + 1, state)) % MOD

    return checksum


class HotRecord:
    def __init__(self, seed: int) -> None:
        self.a = seed
        self.b = seed + 3
        self.c = seed + 7
        self.d = seed + 11


def object_field_hot(scale: int) -> int:
    rounds = 12000 * scale
    record = HotRecord(5)
    checksum = 0

    for index in range(rounds):
        record.a = (record.a + record.b + index) % 10007
        record.b = (record.b + record.c + record.a + 3) % 10009
        record.c = (record.c + record.d + record.b + (index % 7)) % 10037
        record.d = (record.d + record.a + record.c + 5) % 10039
        snapshot = record.a * 3 + record.b * 5 + record.c * 7 + record.d * 11
        if snapshot % 2 == 0:
            checksum = (checksum + snapshot + record.b) % MOD
        else:
            checksum = (checksum + snapshot + record.c) % MOD

    return checksum


def array_index_dense(scale: int) -> int:
    length = 128 * scale
    rounds = 48 * scale
    values = [0] * length
    checksum = 0

    for index in range(length):
        values[index] = (index * 13 + 7) % 997

    for round_index in range(rounds):
        for cursor in range(1, length - 1):
            left = values[cursor - 1]
            mid = values[cursor]
            right = values[cursor + 1]
            updated = (left + mid * 3 + right * 5 + round_index + cursor) % 1000003
            values[cursor] = updated
            checksum = (checksum + updated * (cursor + 1)) % MOD

        checksum = (checksum + values[0] + values[length - 1] + round_index) % MOD

    return checksum


def branch_jump_dense(scale: int) -> int:
    outer_limit = 180 * scale
    inner_limit = 180
    state = 23
    checksum = 0

    for outer in range(outer_limit):
        for inner in range(inner_limit):
            state = (state * 97 + outer * 13 + inner * 17 + 19) % 65521
            if state % 11 == 0:
                checksum += state // 11 + outer
            elif state % 7 == 0:
                checksum += state % 97 + inner * 3
            elif state % 5 == 0:
                checksum += (state // 5) % 89 + outer * 5
            elif state % 3 == 0:
                checksum += (state ^ (outer + inner)) + 17
            else:
                checksum += state % 31 + outer + inner

            checksum %= MOD
            if checksum % 2 == 0:
                checksum = (checksum + state % 19) % MOD
            else:
                checksum = (checksum + state % 23) % MOD

    return checksum


class Service:
    def __init__(self, weight: int, bias: int) -> None:
        self.weight = weight
        self.bias = bias

    def handle(self, value: int, ticket: int) -> int:
        self.bias = (self.bias + ticket + self.weight) % 10007
        if (ticket + self.weight) % 2 == 0:
            return (value * self.weight + self.bias + ticket) % 1000003
        return (value + self.weight * 7 + self.bias + ticket * 3) % 1000003


def route_service(service: Service, value: int, ticket: int) -> int:
    return service.handle(value, ticket)


def mixed_service_loop(scale: int) -> int:
    length = 24 * scale
    rounds = 320 * scale
    counters = [0] * length
    service0 = Service(3, 11)
    service1 = Service(5, 17)
    service2 = Service(7, 23)
    checksum = 0
    state = 31

    for index in range(length):
        counters[index] = (index * 19 + 5) % 257

    for outer in range(rounds):
        for inner in range(32):
            slot = (outer + inner + state) % length
            current = counters[slot]
            selector = slot % 3
            ticket = outer * 11 + inner * 7 + selector

            if selector == 0:
                state = route_service(service0, current + state, ticket)
            elif selector == 1:
                state = route_service(service1, current + state, ticket)
            else:
                state = route_service(service2, current + state, ticket)

            counters[slot] = (current + state + selector + inner) % 1000003
            if counters[slot] % 4 == 0:
                checksum = (checksum + counters[slot] + state + current) % MOD
            else:
                checksum = (checksum + counters[slot] * (selector + 1) + state) % MOD

    return (
        checksum
        + service0.bias
        + service1.bias
        + service2.bias
        + counters[0]
        + counters[length // 2]
        + counters[length - 1]
    ) % MOD


GC_FRAGMENTS = ["amber", "birch", "cedar", "dune", "ember", "frost", "grove", "harbor"]


def gc_fragment_for(slot: int) -> str:
    return GC_FRAGMENTS[slot % len(GC_FRAGMENTS)]


def gc_payload_for(seed: int, cycle: int, slot: int) -> str:
    head = gc_fragment_for(seed + cycle)
    middle = gc_fragment_for(seed // 3 + slot * 5)
    tail = gc_fragment_for(seed + cycle * 7 + slot * 11)
    return f"{head}-{middle}-{tail}:{seed}:{cycle}:{slot}"


def gc_fragment_stress(scale: int) -> int:
    survivors: list[str] = []
    scratch: list[str] = []
    anchors: dict[str, int] = {}
    old_archive: list[str] = []
    old_lookup: dict[str, str] = {}
    seed = 29
    checksum = 0
    anchor_count = 0

    for cycle in range(36 * scale):
        scratch.clear()
        probe_key = ""
        probe_fallback = 0

        for slot in range(320):
            seed = (seed * 73 + 19 + cycle + slot) % 10007
            payload = gc_payload_for(seed, cycle, slot)
            scratch.append(payload)
            scratch.append(payload + "|" + gc_fragment_for(seed + slot + 3))

            if slot % 5 == 0:
                survivors.append(payload + "#hold#" + str(cycle))
            if slot % 7 == 0:
                anchor_key = payload + "#anchor#" + str(slot)
                if anchor_key not in anchors:
                    anchor_count += 1
                anchors[anchor_key] = seed + cycle + slot
            if slot % 11 == 0:
                shadow_key = payload + "#shadow#" + str(cycle)
                if shadow_key not in anchors:
                    anchor_count += 1
                anchors[shadow_key] = seed * 2 + slot
            if slot % 17 == 0:
                archive_value = payload + "#old#" + gc_fragment_for(seed + cycle + slot)
                old_archive.append(archive_value)
                old_lookup["slot#" + str(slot % 32)] = archive_value
            if slot % 13 == 0 and len(survivors) > 24:
                survivors.pop(0)
            if slot % 19 == 0 and len(old_archive) > 96:
                old_archive.pop(0)

            checksum = (checksum * 131 + seed + cycle * 17 + slot * 29 + len(survivors)) % MOD
            if slot == 0:
                probe_key = payload + "#anchor#0"
                probe_fallback = seed + cycle

        scratch.clear()
        if cycle % 4 == 3:
            anchors.clear()
            anchor_count = 0
        if probe_key in anchors:
            checksum = (checksum * 137 + anchors[probe_key] + len(survivors) + cycle) % MOD
        else:
            checksum = (checksum * 137 + probe_fallback + len(survivors) + cycle) % MOD
        if "slot#" + str(cycle % 32) in old_lookup:
            checksum = (checksum * 149 + len(old_archive) * 7 + anchor_count + cycle + 31) % MOD
        else:
            checksum = (checksum * 149 + len(old_archive) * 7 + anchor_count + cycle) % MOD
        if cycle % 9 == 8:
            survivors.clear()

    return (checksum + len(survivors) * 17 + anchor_count * 19 + len(old_archive) * 23 + seed) % MOD


CASE_HANDLERS = {
    "numeric_loops": ("BENCH_NUMERIC_LOOPS_PASS", numeric_loops),
    "dispatch_loops": ("BENCH_DISPATCH_LOOPS_PASS", dispatch_loops),
    "container_pipeline": ("BENCH_CONTAINER_PIPELINE_PASS", container_pipeline),
    "sort_array": ("BENCH_SORT_ARRAY_PASS", sort_array),
    "prime_trial_division": ("BENCH_PRIME_TRIAL_DIVISION_PASS", prime_trial_division),
    "matrix_add_2d": ("BENCH_MATRIX_ADD_2D_PASS", matrix_add_2d),
    "string_build": ("BENCH_STRING_BUILD_PASS", string_build),
    "map_object_access": ("BENCH_MAP_OBJECT_ACCESS_PASS", map_object_access),
    "fib_recursive": ("BENCH_FIB_RECURSIVE_PASS", fib_recursive),
    "call_chain_polymorphic": ("BENCH_CALL_CHAIN_POLYMORPHIC_PASS", call_chain_polymorphic),
    "object_field_hot": ("BENCH_OBJECT_FIELD_HOT_PASS", object_field_hot),
    "array_index_dense": ("BENCH_ARRAY_INDEX_DENSE_PASS", array_index_dense),
    "branch_jump_dense": ("BENCH_BRANCH_JUMP_DENSE_PASS", branch_jump_dense),
    "mixed_service_loop": ("BENCH_MIXED_SERVICE_LOOP_PASS", mixed_service_loop),
    "gc_fragment_baseline": ("BENCH_GC_FRAGMENT_BASELINE_PASS", gc_fragment_stress),
    "gc_fragment_stress": ("BENCH_GC_FRAGMENT_STRESS_PASS", gc_fragment_stress),
}


def run_main(case_name: str) -> None:
    if case_name not in CASE_HANDLERS:
        raise SystemExit(f"unknown benchmark case: {case_name}")

    banner, handler = CASE_HANDLERS[case_name]
    scale = parse_scale(sys.argv[1:])
    checksum = handler(scale)
    print(banner)
    print(checksum)
