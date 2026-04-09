#include "benchmark_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ZrBenchInt zr_bench_mod(ZrBenchInt value) {
    value %= ZR_BENCH_MOD;
    if (value < 0) {
        value += ZR_BENCH_MOD;
    }
    return value;
}

int zr_bench_scale_from_tier(const char *tier) {
    if (tier == NULL) {
        return 0;
    }
    if (strcmp(tier, "smoke") == 0) {
        return 1;
    }
    if (strcmp(tier, "core") == 0) {
        return 4;
    }
    if (strcmp(tier, "stress") == 0) {
        return 16;
    }
    if (strcmp(tier, "profile") == 0) {
        return 1;
    }
    return 0;
}

ZrBenchInt zr_bench_run_numeric_loops(int scale) {
    const int outerLimit = 24 * scale;
    const int innerLimit = 3000 * scale;
    ZrBenchInt value = 17;
    ZrBenchInt checksum = 0;
    int outer;

    for (outer = 0; outer < outerLimit; outer++) {
        int inner;
        for (inner = 0; inner < innerLimit; inner++) {
            value = (value * 1103 + 97 + outer + inner) % 65521;
            if ((value % 7) == 0) {
                checksum += value / 7;
            } else if ((value % 5) == 0) {
                checksum += value % 97;
            } else {
                checksum += value % 31;
            }
            checksum = zr_bench_mod(checksum);
        }
        checksum = zr_bench_mod(checksum + outer * 17 + (value % 13));
    }

    return checksum;
}

typedef struct ZrBenchWorker {
    ZrBenchInt state;
    int kind;
} ZrBenchWorker;

static ZrBenchInt zr_bench_worker_step(ZrBenchWorker *worker, ZrBenchInt delta) {
    if (worker->kind == 0) {
        worker->state = (worker->state * 13 + delta + 7) % 10007;
    } else if (worker->kind == 1) {
        worker->state = (worker->state * 17 + delta * 3 + 11) % 10009;
    } else if (worker->kind == 2) {
        worker->state = ((worker->state ^ (delta + 31)) + delta * 5 + 19) % 10037;
    } else {
        worker->state = (worker->state + delta * delta + 23) % 10039;
    }
    return worker->state;
}

ZrBenchInt zr_bench_run_dispatch_loops(int scale) {
    const int outerLimit = 120 * scale;
    const int innerLimit = 320 * scale;
    ZrBenchWorker workers[4] = {{17, 0}, {29, 1}, {43, 2}, {61, 3}};
    ZrBenchInt checksum = 0;
    int outer;

    for (outer = 0; outer < outerLimit; outer++) {
        int inner;
        for (inner = 0; inner < innerLimit; inner++) {
            const int index = (outer + inner) & 3;
            const ZrBenchInt delta = outer * 7 + inner * 11 + index;
            const ZrBenchInt value = zr_bench_worker_step(&workers[index], delta);
            checksum = zr_bench_mod(checksum + value * (index + 1) + (delta % 29));
        }

        checksum = zr_bench_mod(checksum + workers[outer & 3].state * (outer + 1));
    }

    return checksum;
}

static int zr_bench_container_label_index(int value) {
    if ((value % 2) == 0) {
        return 0;
    }
    if (value > 128) {
        return 1;
    }
    return 2;
}

ZrBenchInt zr_bench_run_container_pipeline(int scale) {
    const int total = 1024 * scale;
    const int maxValue = 256 * scale + 12;
    const int seenStride = maxValue + 1;
    unsigned char *seen = NULL;
    int *labels = NULL;
    int *values = NULL;
    int seed = 41;
    int index;
    ZrBenchInt evenSum = 0;
    ZrBenchInt oddHiSum = 0;
    ZrBenchInt oddLoSum = 0;
    ZrBenchInt uniqueCount = 0;

    labels = (int *)malloc((size_t)total * sizeof(*labels));
    values = (int *)malloc((size_t)total * sizeof(*values));
    seen = (unsigned char *)calloc((size_t)3 * (size_t)seenStride, sizeof(*seen));
    if (labels == NULL || values == NULL || seen == NULL) {
        free(labels);
        free(values);
        free(seen);
        return 0;
    }

    for (index = 0; index < total; index++) {
        seed = (seed * 29 + 17 + index) % 257;
        labels[index] = zr_bench_container_label_index(seed);
        values[index] = seed * scale + (index % 13);
    }

    for (index = 0; index < total; index++) {
        seen[labels[index] * seenStride + values[index]] = 1;
    }

    for (index = 0; index <= maxValue; index++) {
        if (seen[index] != 0) {
            evenSum += index;
            uniqueCount++;
        }
        if (seen[seenStride + index] != 0) {
            oddHiSum += index;
            uniqueCount++;
        }
        if (seen[seenStride * 2 + index] != 0) {
            oddLoSum += index;
            uniqueCount++;
        }
    }

    free(labels);
    free(values);
    free(seen);
    return zr_bench_mod(evenSum * 100000 + oddHiSum * 100 + oddLoSum + uniqueCount);
}

static void zr_bench_insertion_sort(ZrBenchInt *values, int length) {
    int index;

    for (index = 1; index < length; index++) {
        const ZrBenchInt key = values[index];
        int cursor = index - 1;
        while (cursor >= 0 && values[cursor] > key) {
            values[cursor + 1] = values[cursor];
            cursor--;
        }
        values[cursor + 1] = key;
    }
}

static void zr_bench_build_sort_pattern(int pattern, int length, ZrBenchInt *values) {
    ZrBenchInt seed = 97;
    int index;

    if (pattern == 0) {
        for (index = 0; index < length; index++) {
            seed = (seed * 1103515245 + 12345 + index) % 2147483647;
            values[index] = seed % 100000;
        }
        return;
    }

    if (pattern == 1) {
        for (index = 0; index < length; index++) {
            values[index] = length - index;
        }
        return;
    }

    if (pattern == 2) {
        for (index = 0; index < length; index++) {
            values[index] = (index * 17 + 3) % (length / 8 + 5);
        }
        return;
    }

    for (index = 0; index < length; index++) {
        values[index] = index * 3 + (index % 7);
    }
    for (index = 0; index < length; index += 7) {
        const int swapIndex = (index * 13 + 5) % length;
        const ZrBenchInt temporary = values[index];
        values[index] = values[swapIndex];
        values[swapIndex] = temporary;
    }
}

ZrBenchInt zr_bench_run_sort_array(int scale) {
    const int length = 16 * scale;
    int step = length / 7;
    ZrBenchInt checksum = 0;
    int pattern;

    if (step < 1) {
        step = 1;
    }

    for (pattern = 0; pattern < 4; pattern++) {
        ZrBenchInt *values = (ZrBenchInt *)malloc((size_t)length * sizeof(*values));
        ZrBenchInt subtotal = 0;
        int cursor = 0;

        if (values == NULL) {
            return 0;
        }

        zr_bench_build_sort_pattern(pattern, length, values);
        zr_bench_insertion_sort(values, length);

        while (cursor < length) {
            subtotal = zr_bench_mod(subtotal + values[cursor] * (cursor + 1));
            cursor += step;
        }
        subtotal = zr_bench_mod(subtotal
                                + values[0] * 3
                                + values[length / 2] * 5
                                + values[length - 1] * 7
                                + pattern * 11);
        checksum = zr_bench_mod(checksum * 131 + subtotal);
        free(values);
    }

    return checksum;
}

ZrBenchInt zr_bench_run_prime_trial_division(int scale) {
    const int limit = 5000 * scale;
    ZrBenchInt checksum = 0;
    ZrBenchInt count = 0;
    int candidate;

    for (candidate = 2; candidate <= limit; candidate++) {
        int divisor;
        int isPrime = 1;
        for (divisor = 2; divisor * divisor <= candidate; divisor++) {
            if ((candidate % divisor) == 0) {
                isPrime = 0;
                break;
            }
        }
        if (isPrime) {
            count++;
            checksum = zr_bench_mod(checksum + candidate * ((count % 97) + 1));
        }
    }

    return checksum;
}

ZrBenchInt zr_bench_run_matrix_add_2d(int scale) {
    const int rows = 24 * scale;
    const int cols = 32 * scale;
    const int cells = rows * cols;
    ZrBenchInt *lhs = NULL;
    ZrBenchInt *rhs = NULL;
    ZrBenchInt *dst = NULL;
    ZrBenchInt *scratch = NULL;
    ZrBenchInt checksum = 0;
    int index;
    int row;

    lhs = (ZrBenchInt *)malloc((size_t)cells * sizeof(*lhs));
    rhs = (ZrBenchInt *)malloc((size_t)cells * sizeof(*rhs));
    dst = (ZrBenchInt *)malloc((size_t)cells * sizeof(*dst));
    scratch = (ZrBenchInt *)malloc((size_t)cells * sizeof(*scratch));
    if (lhs == NULL || rhs == NULL || dst == NULL || scratch == NULL) {
        free(lhs);
        free(rhs);
        free(dst);
        free(scratch);
        return 0;
    }

    for (index = 0; index < cells; index++) {
        lhs[index] = (index * 13 + 7) % 997;
        rhs[index] = (index * 17 + 11) % 991;
    }

    for (row = 0; row < rows; row++) {
        ZrBenchInt rowSum = 0;
        int col;
        for (col = 0; col < cols; col++) {
            index = row * cols + col;
            dst[index] = lhs[index] + rhs[index] + ((row + col) % 7);
            scratch[index] = dst[index] - lhs[index] / 3 + (rhs[index] % 11);
            rowSum += scratch[index] * (col + 1);
        }
        checksum = zr_bench_mod(checksum + rowSum * (row + 1));
    }

    for (index = 0; index < cells; index++) {
        checksum = zr_bench_mod(checksum + scratch[index] * ((index % 17) + 1));
    }

    free(lhs);
    free(rhs);
    free(dst);
    free(scratch);
    return checksum;
}

#define ZR_BENCH_MAX_STRING_KEYS 512
#define ZR_BENCH_MAX_STRING_LENGTH 512

static int zr_bench_find_string_key(char keys[ZR_BENCH_MAX_STRING_KEYS][ZR_BENCH_MAX_STRING_LENGTH],
                                    int keyCount,
                                    const char *key) {
    int index;

    for (index = 0; index < keyCount; index++) {
        if (strcmp(keys[index], key) == 0) {
            return index;
        }
    }
    return -1;
}

ZrBenchInt zr_bench_run_string_build(int scale) {
    static const char *const fragments[] = {"al", "be", "cy", "do", "ex", "fu"};
    char keys[ZR_BENCH_MAX_STRING_KEYS][ZR_BENCH_MAX_STRING_LENGTH];
    ZrBenchInt counts[ZR_BENCH_MAX_STRING_KEYS];
    char assembled[ZR_BENCH_MAX_STRING_LENGTH];
    ZrBenchInt assembledScore = 0;
    ZrBenchInt checksum = 0;
    ZrBenchInt seed = 17;
    const int iterations = 180 * scale;
    int keyCount = 0;
    int index;

    memset(keys, 0, sizeof(keys));
    memset(counts, 0, sizeof(counts));
    assembled[0] = '\0';

    for (index = 0; index < iterations; index++) {
        char token[64];
        int tokenId;
        ZrBenchInt tokenScore;
        int keyIndex;

        seed = (seed * 73 + 19 + index) % 997;
        tokenId = (int)((seed + index) % 23);
        tokenScore = (seed % 211) + tokenId * 17 + index;
        snprintf(token,
                 sizeof(token),
                 "%s-%s%s",
                 fragments[seed % 6],
                 fragments[tokenId % 6],
                 fragments[(tokenId + 2) % 6]);
        strncat(assembled, token, sizeof(assembled) - strlen(assembled) - 1);
        assembledScore = zr_bench_mod(assembledScore * 41 + tokenScore);
        if ((index % 4) == 0) {
            strncat(assembled, "|", sizeof(assembled) - strlen(assembled) - 1);
            assembledScore = zr_bench_mod(assembledScore + 3);
        } else {
            strncat(assembled, ":", sizeof(assembled) - strlen(assembled) - 1);
            assembledScore = zr_bench_mod(assembledScore + 7);
        }

        if ((index % 9) == 8) {
            keyIndex = zr_bench_find_string_key(keys, keyCount, assembled);
            if (keyIndex < 0) {
                keyIndex = keyCount++;
                strncpy(keys[keyIndex], assembled, sizeof(keys[keyIndex]) - 1);
                keys[keyIndex][sizeof(keys[keyIndex]) - 1] = '\0';
            }

            counts[keyIndex] = zr_bench_mod(counts[keyIndex] + assembledScore + index + 1);
            checksum = zr_bench_mod(checksum + counts[keyIndex] + (seed % 97));
            strncpy(assembled, token, sizeof(assembled) - 1);
            assembled[sizeof(assembled) - 1] = '\0';
            assembledScore = zr_bench_mod(tokenScore);
        }
    }

    if (assembled[0] != '\0') {
        const int keyIndex = zr_bench_find_string_key(keys, keyCount, assembled);
        const int targetIndex = keyIndex >= 0 ? keyIndex : keyCount++;
        if (keyIndex < 0) {
            strncpy(keys[targetIndex], assembled, sizeof(keys[targetIndex]) - 1);
            keys[targetIndex][sizeof(keys[targetIndex]) - 1] = '\0';
        }
        counts[targetIndex] = zr_bench_mod(counts[targetIndex] + assembledScore + iterations);
    }

    for (index = 0; index < keyCount; index++) {
        checksum = zr_bench_mod(checksum + counts[index] * (index + 1));
    }

    return checksum;
}

ZrBenchInt zr_bench_run_map_object_access(int scale) {
    ZrBenchInt bucketCounts[4];
    ZrBenchInt checksum = 0;
    ZrBenchInt left = 3;
    ZrBenchInt right = 7;
    ZrBenchInt hits = 0;
    int outer;

    memset(bucketCounts, 0, sizeof(bucketCounts));

    for (outer = 0; outer < 64 * scale; outer++) {
        int inner;
        for (inner = 0; inner < 32; inner++) {
            const int labelIndex = (outer + inner) % 4;
            left = (left * 31 + outer + inner + hits) % 10007;
            right = (right + left + inner * 3 + 5) % 10009;
            hits += 1;
            bucketCounts[labelIndex] = zr_bench_mod(bucketCounts[labelIndex] + left + right + hits);
            checksum = zr_bench_mod(checksum + bucketCounts[labelIndex] + left + hits);
        }
    }

    return zr_bench_mod(checksum + bucketCounts[0] + bucketCounts[1] + bucketCounts[2] + bucketCounts[3]);
}

static ZrBenchInt zr_bench_fib_recursive_value(int n) {
    if (n <= 1) {
        return n;
    }
    return zr_bench_fib_recursive_value(n - 1) + zr_bench_fib_recursive_value(n - 2);
}

ZrBenchInt zr_bench_run_fib_recursive(int scale) {
    const int rounds = 18 * scale;
    ZrBenchInt checksum = 0;
    int index;

    for (index = 0; index < rounds; index++) {
        const int n = 13 + (index % 6);
        const ZrBenchInt value = zr_bench_fib_recursive_value(n);
        checksum = zr_bench_mod(checksum * 131 + value * (index + 3) + n);
    }

    return checksum;
}

typedef struct ZrBenchPolyCallable {
    ZrBenchInt state;
    int kind;
    ZrBenchInt (*invoke)(struct ZrBenchPolyCallable *callable, ZrBenchInt value, ZrBenchInt delta);
} ZrBenchPolyCallable;

static ZrBenchInt zr_bench_poly_adder_call(ZrBenchPolyCallable *callable, ZrBenchInt value, ZrBenchInt delta) {
    return (value + delta + callable->state) % 100003;
}

static ZrBenchInt zr_bench_poly_multiply_call(ZrBenchPolyCallable *callable, ZrBenchInt value, ZrBenchInt delta) {
    return (value * (callable->state + 3) + delta + 7) % 100003;
}

static ZrBenchInt zr_bench_poly_xor_call(ZrBenchPolyCallable *callable, ZrBenchInt value, ZrBenchInt delta) {
    return ((value ^ (delta + callable->state)) + callable->state * 5 + delta) % 100003;
}

static ZrBenchInt zr_bench_call_leaf(ZrBenchInt value, ZrBenchInt salt) {
    return (value * 17 + salt * 13 + 19) % 100003;
}

static ZrBenchInt zr_bench_call_chain_a(ZrBenchInt value, ZrBenchInt salt) {
    return zr_bench_call_leaf(value + 3, salt + 1);
}

static ZrBenchInt zr_bench_call_chain_b(ZrBenchInt value, ZrBenchInt salt) {
    return zr_bench_call_leaf(zr_bench_call_chain_a(value + (salt % 5), salt + 7), salt + 11);
}

static ZrBenchInt zr_bench_call_chain_c(ZrBenchInt value, ZrBenchInt salt) {
    return zr_bench_call_leaf(zr_bench_call_chain_b(value ^ salt, salt + 13), salt + 17);
}

static ZrBenchInt zr_bench_tail_accumulate(int steps, ZrBenchInt acc) {
    if (steps == 0) {
        return acc;
    }
    return zr_bench_tail_accumulate(steps - 1, (acc * 3 + steps + 5) % 100003);
}

ZrBenchInt zr_bench_run_call_chain_polymorphic(int scale) {
    const int rounds = 320 * scale;
    ZrBenchPolyCallable adder = {17, 0, zr_bench_poly_adder_call};
    ZrBenchPolyCallable multiply = {23, 1, zr_bench_poly_multiply_call};
    ZrBenchPolyCallable xorCall = {31, 2, zr_bench_poly_xor_call};
    ZrBenchInt state = 17;
    ZrBenchInt checksum = 0;
    int outer;

    for (outer = 0; outer < rounds; outer++) {
        const ZrBenchInt delta = outer * 7 + (state % 13);
        const int selector = outer % 3;
        if (selector == 0) {
            state = adder.invoke(&adder, zr_bench_call_chain_a(state, delta), delta);
        } else if (selector == 1) {
            state = multiply.invoke(&multiply, zr_bench_call_chain_b(state, delta), delta);
        } else {
            state = xorCall.invoke(&xorCall, zr_bench_call_chain_c(state, delta), delta);
        }

        checksum = zr_bench_mod(checksum + state * (selector + 1) + zr_bench_tail_accumulate((outer % 5) + 1, state));
    }

    return checksum;
}

typedef struct ZrBenchHotRecord {
    ZrBenchInt a;
    ZrBenchInt b;
    ZrBenchInt c;
    ZrBenchInt d;
} ZrBenchHotRecord;

ZrBenchInt zr_bench_run_object_field_hot(int scale) {
    const int rounds = 12000 * scale;
    ZrBenchHotRecord record = {5, 8, 12, 16};
    ZrBenchInt checksum = 0;
    int index;

    for (index = 0; index < rounds; index++) {
        ZrBenchInt snapshot;

        record.a = (record.a + record.b + index) % 10007;
        record.b = (record.b + record.c + record.a + 3) % 10009;
        record.c = (record.c + record.d + record.b + (index % 7)) % 10037;
        record.d = (record.d + record.a + record.c + 5) % 10039;
        snapshot = record.a * 3 + record.b * 5 + record.c * 7 + record.d * 11;
        if ((snapshot % 2) == 0) {
            checksum = zr_bench_mod(checksum + snapshot + record.b);
        } else {
            checksum = zr_bench_mod(checksum + snapshot + record.c);
        }
    }

    return checksum;
}

ZrBenchInt zr_bench_run_array_index_dense(int scale) {
    const int length = 128 * scale;
    const int rounds = 48 * scale;
    ZrBenchInt *values = NULL;
    ZrBenchInt checksum = 0;
    int index;
    int roundIndex;

    values = (ZrBenchInt *)malloc((size_t)length * sizeof(*values));
    if (values == NULL) {
        return 0;
    }

    for (index = 0; index < length; index++) {
        values[index] = (index * 13 + 7) % 997;
    }

    for (roundIndex = 0; roundIndex < rounds; roundIndex++) {
        int cursor;
        for (cursor = 1; cursor < length - 1; cursor++) {
            const ZrBenchInt left = values[cursor - 1];
            const ZrBenchInt mid = values[cursor];
            const ZrBenchInt right = values[cursor + 1];
            const ZrBenchInt updated = (left + mid * 3 + right * 5 + roundIndex + cursor) % 1000003;
            values[cursor] = updated;
            checksum = zr_bench_mod(checksum + updated * (cursor + 1));
        }

        checksum = zr_bench_mod(checksum + values[0] + values[length - 1] + roundIndex);
    }

    free(values);
    return checksum;
}

ZrBenchInt zr_bench_run_branch_jump_dense(int scale) {
    const int outerLimit = 180 * scale;
    const int innerLimit = 180;
    ZrBenchInt state = 23;
    ZrBenchInt checksum = 0;
    int outer;

    for (outer = 0; outer < outerLimit; outer++) {
        int inner;
        for (inner = 0; inner < innerLimit; inner++) {
            state = (state * 97 + outer * 13 + inner * 17 + 19) % 65521;
            if ((state % 11) == 0) {
                checksum += state / 11 + outer;
            } else if ((state % 7) == 0) {
                checksum += (state % 97) + inner * 3;
            } else if ((state % 5) == 0) {
                checksum += (state / 5) % 89 + outer * 5;
            } else if ((state % 3) == 0) {
                checksum += (state ^ (outer + inner)) + 17;
            } else {
                checksum += (state % 31) + outer + inner;
            }

            checksum = zr_bench_mod(checksum);
            if ((checksum % 2) == 0) {
                checksum = zr_bench_mod(checksum + state % 19);
            } else {
                checksum = zr_bench_mod(checksum + state % 23);
            }
        }
    }

    return checksum;
}

typedef struct ZrBenchService {
    ZrBenchInt weight;
    ZrBenchInt bias;
} ZrBenchService;

static ZrBenchInt zr_bench_service_handle(ZrBenchService *service, ZrBenchInt value, ZrBenchInt ticket) {
    service->bias = (service->bias + ticket + service->weight) % 10007;
    if (((ticket + service->weight) % 2) == 0) {
        return (value * service->weight + service->bias + ticket) % 1000003;
    }
    return (value + service->weight * 7 + service->bias + ticket * 3) % 1000003;
}

ZrBenchInt zr_bench_run_mixed_service_loop(int scale) {
    const int length = 24 * scale;
    const int rounds = 320 * scale;
    ZrBenchInt *counters = NULL;
    ZrBenchService service0 = {3, 11};
    ZrBenchService service1 = {5, 17};
    ZrBenchService service2 = {7, 23};
    ZrBenchInt checksum = 0;
    ZrBenchInt state = 31;
    int index;
    int outer;

    counters = (ZrBenchInt *)malloc((size_t)length * sizeof(*counters));
    if (counters == NULL) {
        return 0;
    }

    for (index = 0; index < length; index++) {
        counters[index] = (index * 19 + 5) % 257;
    }

    for (outer = 0; outer < rounds; outer++) {
        int inner;
        for (inner = 0; inner < 32; inner++) {
            const int slot = (int)((outer + inner + state) % length);
            const ZrBenchInt current = counters[slot];
            const int selector = slot % 3;
            const ZrBenchInt ticket = outer * 11 + inner * 7 + selector;

            if (selector == 0) {
                state = zr_bench_service_handle(&service0, current + state, ticket);
            } else if (selector == 1) {
                state = zr_bench_service_handle(&service1, current + state, ticket);
            } else {
                state = zr_bench_service_handle(&service2, current + state, ticket);
            }

            counters[slot] = (current + state + selector + inner) % 1000003;
            if ((counters[slot] % 4) == 0) {
                checksum = zr_bench_mod(checksum + counters[slot] + state + current);
            } else {
                checksum = zr_bench_mod(checksum + counters[slot] * (selector + 1) + state);
            }
        }
    }

    checksum = zr_bench_mod(checksum
                            + service0.bias
                            + service1.bias
                            + service2.bias
                            + counters[0]
                            + counters[length / 2]
                            + counters[length - 1]);
    free(counters);
    return checksum;
}
