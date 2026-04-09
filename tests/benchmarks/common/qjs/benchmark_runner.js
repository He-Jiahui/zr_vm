const MOD = 1000000007;
const TIER_SCALES = {
    smoke: 1,
    core: 4,
    stress: 16,
    profile: 1,
};

function parseScale(args) {
    let tier = "core";
    let explicitScale = null;

    for (let index = 0; index < args.length; index += 1) {
        const arg = args[index];
        if (arg === "--tier") {
            if (index + 1 >= args.length) {
                throw new Error("--tier requires smoke, core, stress, or profile");
            }
            tier = args[index + 1];
            index += 1;
            continue;
        }
        if (arg === "--scale") {
            if (index + 1 >= args.length) {
                throw new Error("--scale requires a positive integer");
            }
            explicitScale = Number.parseInt(args[index + 1], 10);
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

function normalizeArgs(args) {
    if (!args || args.length === 0) {
        return [];
    }

    if (typeof args[0] === "string" && !args[0].startsWith("--") && args[0].endsWith(".js")) {
        return args.slice(1);
    }

    return args;
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
            const value = workers[index].step(delta);
            checksum = (checksum + value * (index + 1) + (delta % 29)) % MOD;
        }

        checksum = (checksum + workers[outer & 3].read() * (outer + 1)) % MOD;
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
    return (evenSum * 100000 + oddHiSum * 100 + oddLoSum + seen.size) % MOD;
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

const CASE_HANDLERS = {
    numeric_loops: ["BENCH_NUMERIC_LOOPS_PASS", numericLoops],
    dispatch_loops: ["BENCH_DISPATCH_LOOPS_PASS", dispatchLoops],
    container_pipeline: ["BENCH_CONTAINER_PIPELINE_PASS", containerPipeline],
    sort_array: ["BENCH_SORT_ARRAY_PASS", sortArray],
    prime_trial_division: ["BENCH_PRIME_TRIAL_DIVISION_PASS", primeTrialDivision],
    matrix_add_2d: ["BENCH_MATRIX_ADD_2D_PASS", matrixAdd2d],
    string_build: ["BENCH_STRING_BUILD_PASS", stringBuild],
    map_object_access: ["BENCH_MAP_OBJECT_ACCESS_PASS", mapObjectAccess],
};

export function runMain(caseName, args) {
    if (!Object.prototype.hasOwnProperty.call(CASE_HANDLERS, caseName)) {
        throw new Error(`unknown benchmark case: ${caseName}`);
    }

    const [banner, handler] = CASE_HANDLERS[caseName];
    const scale = parseScale(normalizeArgs(args || []));
    const checksum = handler(scale);
    print(banner);
    print(String(checksum));
}
