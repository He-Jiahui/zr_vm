"use strict";

const MOD = 1000000007;
const TIER_SCALES = {
    smoke: 1,
    core: 4,
    stress: 16,
    profile: 1,
};

function parseScale(argv) {
    let tier = "core";
    let explicitScale = null;

    for (let index = 0; index < argv.length; index += 1) {
        const arg = argv[index];
        if (arg === "--tier") {
            if (index + 1 >= argv.length) {
                throw new Error("--tier requires smoke, core, or stress");
            }
            tier = argv[index + 1];
            index += 1;
            continue;
        }
        if (arg === "--scale") {
            if (index + 1 >= argv.length) {
                throw new Error("--scale requires a positive integer");
            }
            explicitScale = Number.parseInt(argv[index + 1], 10);
            if (!Number.isFinite(explicitScale) || explicitScale < 1) {
                throw new Error("--scale requires a positive integer");
            }
            index += 1;
            continue;
        }
        throw new Error(`unknown argument: ${arg}`);
    }

    if (explicitScale !== null) {
        return explicitScale;
    }
    if (!Object.prototype.hasOwnProperty.call(TIER_SCALES, tier)) {
        throw new Error(`unsupported tier: ${tier}`);
    }
    return TIER_SCALES[tier];
}

function numericLoops(scale) {
    const outerLimit = 24 * scale;
    const innerLimit = 3000 * scale;
    let value = 17;
    let checksum = 0;

    for (let outer = 0; outer < outerLimit; outer += 1) {
        for (let inner = 0; inner < innerLimit; inner += 1) {
            value = (value * 1103 + 97 + outer + inner) % 65521;
            if (value % 7 === 0) {
                checksum += Math.floor(value / 7);
            } else if (value % 5 === 0) {
                checksum += value % 97;
            } else {
                checksum += value % 31;
            }
            checksum %= MOD;
        }
        checksum = (checksum + outer * 17 + (value % 13)) % MOD;
    }

    return checksum;
}

class MultiplyWorker {
    constructor(seed) {
        this.state = seed;
    }

    step(delta) {
        this.state = (this.state * 13 + delta + 7) % 10007;
        return this.state;
    }

    read() {
        return this.state;
    }
}

class ScaleWorker {
    constructor(seed) {
        this.state = seed;
    }

    step(delta) {
        this.state = (this.state * 17 + delta * 3 + 11) % 10009;
        return this.state;
    }

    read() {
        return this.state;
    }
}

class XorWorker {
    constructor(seed) {
        this.state = seed;
    }

    step(delta) {
        this.state = ((this.state ^ (delta + 31)) + delta * 5 + 19) % 10037;
        return this.state;
    }

    read() {
        return this.state;
    }
}

class DriftWorker {
    constructor(seed) {
        this.state = seed;
    }

    step(delta) {
        this.state = (this.state + delta * delta + 23) % 10039;
        return this.state;
    }

    read() {
        return this.state;
    }
}

function dispatch(worker, delta) {
    return worker.step(delta);
}

function dispatchLoops(scale) {
    const workers = [
        new MultiplyWorker(17),
        new ScaleWorker(29),
        new XorWorker(43),
        new DriftWorker(61),
    ];
    const outerLimit = 120 * scale;
    const innerLimit = 320 * scale;
    let checksum = 0;

    for (let outer = 0; outer < outerLimit; outer += 1) {
        for (let inner = 0; inner < innerLimit; inner += 1) {
            const index = (outer + inner) & 3;
            const delta = outer * 7 + inner * 11 + index;
            const value = dispatch(workers[index], delta);
            checksum = (checksum + value * (index + 1) + (delta % 29)) % MOD;
        }

        const worker = workers[outer & 3];
        checksum = (checksum + worker.read() * (outer + 1)) % MOD;
    }

    return checksum;
}

function containerLabel(value) {
    if (value % 2 === 0) {
        return "even";
    }
    if (value > 128) {
        return "odd_hi";
    }
    return "odd_lo";
}

function containerPipeline(scale) {
    const total = 1024 * scale;
    const queue = [];
    const seen = new Set();
    const buckets = new Map();
    let seed = 41;

    for (let index = 0; index < total; index += 1) {
        seed = (seed * 29 + 17 + index) % 257;
        queue.push([containerLabel(seed), seed * scale + (index % 13)]);
    }

    for (let index = 0; index < queue.length; index += 1) {
        const [label, value] = queue[index];
        seen.add(`${label}:${value}`);
    }

    for (const item of seen) {
        const separator = item.indexOf(":");
        const label = item.slice(0, separator);
        const value = Number(item.slice(separator + 1));
        if (!buckets.has(label)) {
            buckets.set(label, []);
        }
        buckets.get(label).push(value);
    }

    const oddLoSum = (buckets.get("odd_lo") || []).reduce((sum, value) => sum + value, 0);
    const oddHiSum = (buckets.get("odd_hi") || []).reduce((sum, value) => sum + value, 0);
    const evenSum = (buckets.get("even") || []).reduce((sum, value) => sum + value, 0);
    const checksum = evenSum * 100000 + oddHiSum * 100 + oddLoSum + seen.size;
    return checksum % MOD;
}

function insertionSort(values) {
    for (let index = 1; index < values.length; index += 1) {
        const key = values[index];
        let cursor = index - 1;
        while (cursor >= 0 && values[cursor] > key) {
            values[cursor + 1] = values[cursor];
            cursor -= 1;
        }
        values[cursor + 1] = key;
    }
}

function buildSortPattern(pattern, length) {
    const values = [];
    let seed = 97n;

    if (pattern === 0) {
        for (let index = 0; index < length; index += 1) {
            seed = (seed * 1103515245n + 12345n + BigInt(index)) % 2147483647n;
            values.push(Number(seed % 100000n));
        }
        return values;
    }

    if (pattern === 1) {
        for (let index = 0; index < length; index += 1) {
            values.push(length - index);
        }
        return values;
    }

    if (pattern === 2) {
        for (let index = 0; index < length; index += 1) {
            values.push((index * 17 + 3) % (Math.floor(length / 8) + 5));
        }
        return values;
    }

    for (let index = 0; index < length; index += 1) {
        values.push(index * 3 + (index % 7));
    }
    for (let index = 0; index < length; index += 7) {
        const swapIndex = (index * 13 + 5) % length;
        const temporary = values[index];
        values[index] = values[swapIndex];
        values[swapIndex] = temporary;
    }
    return values;
}

function sortArray(scale) {
    const length = 16 * scale;
    let step = Math.floor(length / 7);
    let checksum = 0;
    if (step < 1) {
        step = 1;
    }

    for (let pattern = 0; pattern < 4; pattern += 1) {
        const values = buildSortPattern(pattern, length);
        insertionSort(values);
        let cursor = 0;
        let subtotal = 0;
        while (cursor < length) {
            subtotal = (subtotal + values[cursor] * (cursor + 1)) % MOD;
            cursor += step;
        }
        subtotal = (subtotal
            + values[0] * 3
            + values[Math.floor(length / 2)] * 5
            + values[length - 1] * 7
            + pattern * 11) % MOD;
        checksum = (checksum * 131 + subtotal) % MOD;
    }

    return checksum;
}

function primeTrialDivision(scale) {
    const limit = 5000 * scale;
    let checksum = 0;
    let count = 0;

    for (let candidate = 2; candidate <= limit; candidate += 1) {
        let isPrime = true;
        for (let divisor = 2; divisor * divisor <= candidate; divisor += 1) {
            if (candidate % divisor === 0) {
                isPrime = false;
                break;
            }
        }
        if (isPrime) {
            count += 1;
            checksum = (checksum + candidate * ((count % 97) + 1)) % MOD;
        }
    }

    return checksum;
}

function matrixAdd2d(scale) {
    const rows = 24 * scale;
    const cols = 32 * scale;
    const cells = rows * cols;
    const lhs = new Array(cells).fill(0);
    const rhs = new Array(cells).fill(0);
    const dst = new Array(cells).fill(0);
    const scratch = new Array(cells).fill(0);
    let checksum = 0;

    for (let index = 0; index < cells; index += 1) {
        lhs[index] = (index * 13 + 7) % 997;
        rhs[index] = (index * 17 + 11) % 991;
    }

    for (let row = 0; row < rows; row += 1) {
        let rowSum = 0;
        for (let col = 0; col < cols; col += 1) {
            const index = row * cols + col;
            dst[index] = lhs[index] + rhs[index] + ((row + col) % 7);
            scratch[index] = dst[index] - Math.floor(lhs[index] / 3) + (rhs[index] % 11);
            rowSum += scratch[index] * (col + 1);
        }
        checksum = (checksum + rowSum * (row + 1)) % MOD;
    }

    for (let index = 0; index < cells; index += 1) {
        checksum = (checksum + scratch[index] * ((index % 17) + 1)) % MOD;
    }

    return checksum;
}

function stringBuild(scale) {
    const fragments = ["al", "be", "cy", "do", "ex", "fu"];
    const counts = new Map();
    const keys = [];
    let assembled = "";
    let assembledScore = 0;
    let checksum = 0;
    let seed = 17;
    const iterations = 180 * scale;

    for (let index = 0; index < iterations; index += 1) {
        seed = (seed * 73 + 19 + index) % 997;
        const tokenId = (seed + index) % 23;
        const token = fragments[seed % fragments.length]
            + "-"
            + fragments[tokenId % fragments.length]
            + fragments[(tokenId + 2) % fragments.length];
        const tokenScore = (seed % 211) + tokenId * 17 + index;
        assembled += token;
        assembledScore = (assembledScore * 41 + tokenScore) % MOD;
        if (index % 4 === 0) {
            assembled += "|";
            assembledScore = (assembledScore + 3) % MOD;
        } else {
            assembled += ":";
            assembledScore = (assembledScore + 7) % MOD;
        }

        if (index % 9 === 8) {
            if (!counts.has(assembled)) {
                counts.set(assembled, 0);
                keys.push(assembled);
            }
            counts.set(assembled, (counts.get(assembled) + assembledScore + index + 1) % MOD);
            checksum = (checksum + counts.get(assembled) + (seed % 97)) % MOD;
            assembled = token;
            assembledScore = tokenScore % MOD;
        }
    }

    if (assembled.length > 0) {
        if (!counts.has(assembled)) {
            counts.set(assembled, 0);
            keys.push(assembled);
        }
        counts.set(assembled, (counts.get(assembled) + assembledScore + iterations) % MOD);
    }

    for (let index = 0; index < keys.length; index += 1) {
        checksum = (checksum + counts.get(keys[index]) * (index + 1)) % MOD;
    }

    return checksum;
}

function mapObjectAccess(scale) {
    const labels = ["aa", "bb", "cc", "dd"];
    const buckets = new Map();
    let checksum = 0;
    let left = 3;
    let right = 7;
    let hits = 0;
    const outerLimit = 64 * scale;

    for (let outer = 0; outer < outerLimit; outer += 1) {
        for (let inner = 0; inner < 32; inner += 1) {
            left = (left * 31 + outer + inner + hits) % 10007;
            right = (right + left + inner * 3 + 5) % 10009;
            hits += 1;
            const label = labels[(outer + inner) % labels.length];
            const key = `${label}_slot`;
            buckets.set(key, ((buckets.get(key) || 0) + left + right + hits) % MOD);
            checksum = (checksum + buckets.get(key) + left + hits) % MOD;
        }
    }

    return (checksum
        + buckets.get("aa_slot")
        + buckets.get("bb_slot")
        + buckets.get("cc_slot")
        + buckets.get("dd_slot")) % MOD;
}

function fibRecursiveValue(n) {
    if (n <= 1) {
        return n;
    }
    return fibRecursiveValue(n - 1) + fibRecursiveValue(n - 2);
}

function fibRecursive(scale) {
    const rounds = 18 * scale;
    let checksum = 0;

    for (let index = 0; index < rounds; index += 1) {
        const n = 13 + (index % 6);
        const value = fibRecursiveValue(n);
        checksum = (checksum * 131 + value * (index + 3) + n) % MOD;
    }

    return checksum;
}

class PolyAdder {
    constructor(base) {
        this.base = base;
    }

    call(value, delta) {
        return (value + delta + this.base) % 100003;
    }
}

class PolyMultiply {
    constructor(factor) {
        this.factor = factor;
    }

    call(value, delta) {
        return (value * (this.factor + 3) + delta + 7) % 100003;
    }
}

class PolyXor {
    constructor(mask) {
        this.mask = mask;
    }

    call(value, delta) {
        return ((value ^ (delta + this.mask)) + this.mask * 5 + delta) % 100003;
    }
}

function callLeaf(value, salt) {
    return (value * 17 + salt * 13 + 19) % 100003;
}

function callChainA(value, salt) {
    return callLeaf(value + 3, salt + 1);
}

function callChainB(value, salt) {
    return callLeaf(callChainA(value + (salt % 5), salt + 7), salt + 11);
}

function callChainC(value, salt) {
    return callLeaf(callChainB(value ^ salt, salt + 13), salt + 17);
}

function tailAccumulate(steps, acc) {
    if (steps === 0) {
        return acc;
    }
    return tailAccumulate(steps - 1, (acc * 3 + steps + 5) % 100003);
}

function dispatchCallable(callableObj, value, delta) {
    return callableObj.call(value, delta);
}

function callChainPolymorphic(scale) {
    const rounds = 320 * scale;
    const adder = new PolyAdder(17);
    const multiply = new PolyMultiply(23);
    const xorCall = new PolyXor(31);
    let state = 17;
    let checksum = 0;

    for (let outer = 0; outer < rounds; outer += 1) {
        const delta = outer * 7 + (state % 13);
        const selector = outer % 3;
        if (selector === 0) {
            state = dispatchCallable(adder, callChainA(state, delta), delta);
        } else if (selector === 1) {
            state = dispatchCallable(multiply, callChainB(state, delta), delta);
        } else {
            state = dispatchCallable(xorCall, callChainC(state, delta), delta);
        }

        checksum = (checksum + state * (selector + 1) + tailAccumulate((outer % 5) + 1, state)) % MOD;
    }

    return checksum;
}

class HotRecord {
    constructor(seed) {
        this.a = seed;
        this.b = seed + 3;
        this.c = seed + 7;
        this.d = seed + 11;
    }
}

function objectFieldHot(scale) {
    const rounds = 12000 * scale;
    const record = new HotRecord(5);
    let checksum = 0;

    for (let index = 0; index < rounds; index += 1) {
        record.a = (record.a + record.b + index) % 10007;
        record.b = (record.b + record.c + record.a + 3) % 10009;
        record.c = (record.c + record.d + record.b + (index % 7)) % 10037;
        record.d = (record.d + record.a + record.c + 5) % 10039;
        const snapshot = record.a * 3 + record.b * 5 + record.c * 7 + record.d * 11;
        if (snapshot % 2 === 0) {
            checksum = (checksum + snapshot + record.b) % MOD;
        } else {
            checksum = (checksum + snapshot + record.c) % MOD;
        }
    }

    return checksum;
}

function arrayIndexDense(scale) {
    const length = 128 * scale;
    const rounds = 48 * scale;
    const values = new Array(length).fill(0);
    let checksum = 0;

    for (let index = 0; index < length; index += 1) {
        values[index] = (index * 13 + 7) % 997;
    }

    for (let roundIndex = 0; roundIndex < rounds; roundIndex += 1) {
        for (let cursor = 1; cursor < length - 1; cursor += 1) {
            const left = values[cursor - 1];
            const mid = values[cursor];
            const right = values[cursor + 1];
            const updated = (left + mid * 3 + right * 5 + roundIndex + cursor) % 1000003;
            values[cursor] = updated;
            checksum = (checksum + updated * (cursor + 1)) % MOD;
        }

        checksum = (checksum + values[0] + values[length - 1] + roundIndex) % MOD;
    }

    return checksum;
}

function branchJumpDense(scale) {
    const outerLimit = 180 * scale;
    const innerLimit = 180;
    let state = 23;
    let checksum = 0;

    for (let outer = 0; outer < outerLimit; outer += 1) {
        for (let inner = 0; inner < innerLimit; inner += 1) {
            state = (state * 97 + outer * 13 + inner * 17 + 19) % 65521;
            if (state % 11 === 0) {
                checksum += Math.floor(state / 11) + outer;
            } else if (state % 7 === 0) {
                checksum += (state % 97) + inner * 3;
            } else if (state % 5 === 0) {
                checksum += Math.floor(state / 5) % 89 + outer * 5;
            } else if (state % 3 === 0) {
                checksum += (state ^ (outer + inner)) + 17;
            } else {
                checksum += (state % 31) + outer + inner;
            }

            checksum %= MOD;
            if (checksum % 2 === 0) {
                checksum = (checksum + state % 19) % MOD;
            } else {
                checksum = (checksum + state % 23) % MOD;
            }
        }
    }

    return checksum;
}

class Service {
    constructor(weight, bias) {
        this.weight = weight;
        this.bias = bias;
    }

    handle(value, ticket) {
        this.bias = (this.bias + ticket + this.weight) % 10007;
        if ((ticket + this.weight) % 2 === 0) {
            return (value * this.weight + this.bias + ticket) % 1000003;
        }
        return (value + this.weight * 7 + this.bias + ticket * 3) % 1000003;
    }
}

function routeService(service, value, ticket) {
    return service.handle(value, ticket);
}

function mixedServiceLoop(scale) {
    const length = 24 * scale;
    const rounds = 320 * scale;
    const counters = new Array(length).fill(0);
    const service0 = new Service(3, 11);
    const service1 = new Service(5, 17);
    const service2 = new Service(7, 23);
    let checksum = 0;
    let state = 31;

    for (let index = 0; index < length; index += 1) {
        counters[index] = (index * 19 + 5) % 257;
    }

    for (let outer = 0; outer < rounds; outer += 1) {
        for (let inner = 0; inner < 32; inner += 1) {
            const slot = (outer + inner + state) % length;
            const current = counters[slot];
            const selector = slot % 3;
            const ticket = outer * 11 + inner * 7 + selector;

            if (selector === 0) {
                state = routeService(service0, current + state, ticket);
            } else if (selector === 1) {
                state = routeService(service1, current + state, ticket);
            } else {
                state = routeService(service2, current + state, ticket);
            }

            counters[slot] = (current + state + selector + inner) % 1000003;
            if (counters[slot] % 4 === 0) {
                checksum = (checksum + counters[slot] + state + current) % MOD;
            } else {
                checksum = (checksum + counters[slot] * (selector + 1) + state) % MOD;
            }
        }
    }

    return (
        checksum
        + service0.bias
        + service1.bias
        + service2.bias
        + counters[0]
        + counters[Math.floor(length / 2)]
        + counters[length - 1]
    ) % MOD;
}

const GC_FRAGMENTS = ["amber", "birch", "cedar", "dune", "ember", "frost", "grove", "harbor"];

function gcFragmentFor(slot) {
    return GC_FRAGMENTS[slot % GC_FRAGMENTS.length];
}

function gcPayloadFor(seed, cycle, slot) {
    const head = gcFragmentFor(seed + cycle);
    const middle = gcFragmentFor(Math.floor(seed / 3) + slot * 5);
    const tail = gcFragmentFor(seed + cycle * 7 + slot * 11);
    return `${head}-${middle}-${tail}:${seed}:${cycle}:${slot}`;
}

function gcFragmentStress(scale) {
    const survivors = [];
    const scratch = [];
    const anchors = new Map();
    const oldArchive = [];
    const oldLookup = new Map();
    let seed = 29;
    let checksum = 0;
    let anchorCount = 0;

    for (let cycle = 0; cycle < 36 * scale; cycle += 1) {
        scratch.length = 0;
        let probeKey = "";
        let probeFallback = 0;

        for (let slot = 0; slot < 320; slot += 1) {
            seed = (seed * 73 + 19 + cycle + slot) % 10007;
            const payload = gcPayloadFor(seed, cycle, slot);
            scratch.push(payload);
            scratch.push(`${payload}|${gcFragmentFor(seed + slot + 3)}`);

            if (slot % 5 === 0) {
                survivors.push(`${payload}#hold#${cycle}`);
            }
            if (slot % 7 === 0) {
                const anchorKey = `${payload}#anchor#${slot}`;
                if (!anchors.has(anchorKey)) {
                    anchorCount += 1;
                }
                anchors.set(anchorKey, seed + cycle + slot);
            }
            if (slot % 11 === 0) {
                const shadowKey = `${payload}#shadow#${cycle}`;
                if (!anchors.has(shadowKey)) {
                    anchorCount += 1;
                }
                anchors.set(shadowKey, seed * 2 + slot);
            }
            if (slot % 17 === 0) {
                const archiveValue = `${payload}#old#${gcFragmentFor(seed + cycle + slot)}`;
                oldArchive.push(archiveValue);
                oldLookup.set(`slot#${slot % 32}`, archiveValue);
            }
            if (slot % 13 === 0 && survivors.length > 24) {
                survivors.shift();
            }
            if (slot % 19 === 0 && oldArchive.length > 96) {
                oldArchive.shift();
            }

            checksum = (checksum * 131 + seed + cycle * 17 + slot * 29 + survivors.length) % MOD;
            if (slot === 0) {
                probeKey = `${payload}#anchor#0`;
                probeFallback = seed + cycle;
            }
        }

        scratch.length = 0;
        if (cycle % 4 === 3) {
            anchors.clear();
            anchorCount = 0;
        }
        if (anchors.has(probeKey)) {
            checksum = (checksum * 137 + anchors.get(probeKey) + survivors.length + cycle) % MOD;
        } else {
            checksum = (checksum * 137 + probeFallback + survivors.length + cycle) % MOD;
        }
        if (oldLookup.has(`slot#${cycle % 32}`)) {
            checksum = (checksum * 149 + oldArchive.length * 7 + anchorCount + cycle + 31) % MOD;
        } else {
            checksum = (checksum * 149 + oldArchive.length * 7 + anchorCount + cycle) % MOD;
        }
        if (cycle % 9 === 8) {
            survivors.length = 0;
        }
    }

    return (checksum + survivors.length * 17 + anchorCount * 19 + oldArchive.length * 23 + seed) % MOD;
}

const CASE_HANDLERS = {
    numeric_loops: ["BENCH_NUMERIC_LOOPS_PASS", numericLoops],
    dispatch_loops: ["BENCH_DISPATCH_LOOPS_PASS", dispatchLoops],
    container_pipeline: ["BENCH_CONTAINER_PIPELINE_PASS", containerPipeline],
    sort_array: ["BENCH_SORT_ARRAY_PASS", sortArray],
    prime_trial_division: ["BENCH_PRIME_TRIAL_DIVISION_PASS", primeTrialDivision],
    matrix_add_2d: ["BENCH_MATRIX_ADD_2D_PASS", matrixAdd2d],
    string_build: ["BENCH_STRING_BUILD_PASS", stringBuild],
    map_object_access: ["BENCH_MAP_OBJECT_ACCESS_PASS", mapObjectAccess],
    fib_recursive: ["BENCH_FIB_RECURSIVE_PASS", fibRecursive],
    call_chain_polymorphic: ["BENCH_CALL_CHAIN_POLYMORPHIC_PASS", callChainPolymorphic],
    object_field_hot: ["BENCH_OBJECT_FIELD_HOT_PASS", objectFieldHot],
    array_index_dense: ["BENCH_ARRAY_INDEX_DENSE_PASS", arrayIndexDense],
    branch_jump_dense: ["BENCH_BRANCH_JUMP_DENSE_PASS", branchJumpDense],
    mixed_service_loop: ["BENCH_MIXED_SERVICE_LOOP_PASS", mixedServiceLoop],
    gc_fragment_baseline: ["BENCH_GC_FRAGMENT_BASELINE_PASS", gcFragmentStress],
    gc_fragment_stress: ["BENCH_GC_FRAGMENT_STRESS_PASS", gcFragmentStress],
};

function runMain(caseName) {
    if (!Object.prototype.hasOwnProperty.call(CASE_HANDLERS, caseName)) {
        throw new Error(`unknown benchmark case: ${caseName}`);
    }

    const [banner, handler] = CASE_HANDLERS[caseName];
    const scale = parseScale(process.argv.slice(2));
    const checksum = handler(scale);
    process.stdout.write(`${banner}\n${checksum}\n`);
}

module.exports = {
    runMain,
};
