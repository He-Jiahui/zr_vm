/* Native integer power edge cases; no VM or allocator setup is required. */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/math.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static int uint_zero(void) {
    CHECK(ZrCore_Math_UIntPower(0, 0) == 1);
    CHECK(ZrCore_Math_UIntPower(0, 1) == 0);
    CHECK(ZrCore_Math_UIntPower(0, 2) == 0);
    return 0;
}

static int uint_one_and_precision(void) {
    CHECK(ZrCore_Math_UIntPower(UINT64_C(7), 0) == 1);
    CHECK(ZrCore_Math_UIntPower(UINT64_C(7), 1) == 7);
    CHECK(ZrCore_Math_UIntPower(UINT64_C(1) << 32, 1) == (UINT64_C(1) << 32));
    CHECK(ZrCore_Math_UIntPower(UINT64_MAX, 1) == UINT64_MAX);
    return 0;
}

static int uint_exact_large(void) {
    CHECK(ZrCore_Math_UIntPower(2, 63) == (UINT64_C(1) << 63));
    return 0;
}

static int uint_accumulated_overflow(void) {
    /* The repeated multiply must detect the first unrepresentable result. */
    CHECK(ZrCore_Math_UIntPower(2, 64) == 0);
    CHECK(ZrCore_Math_UIntPower(UINT64_MAX, 2) == 0);
    CHECK(ZrCore_Math_UIntPower(3, 40) == UINT64_C(12157665459056928801));
    CHECK(ZrCore_Math_UIntPower(3, 41) == 0);
    return 0;
}

static TZrInt64 int64_min(void) {
    return -INT64_C(9223372036854775807) - 1;
}

static int signed_edges(void) {
    CHECK(ZrCore_Math_IntPower(0, 0) == 1);
    CHECK(ZrCore_Math_IntPower(0, 1) == 0);
    CHECK(ZrCore_Math_IntPower(1, INT64_MAX) == 1);
    CHECK(ZrCore_Math_IntPower(-1, INT64_MAX) == -1);
    CHECK(ZrCore_Math_IntPower(-1, INT64_MAX - 1) == 1);
    CHECK(ZrCore_Math_IntPower(-2, 3) == -8);
    CHECK(ZrCore_Math_IntPower(-2, 4) == 16);
    CHECK(ZrCore_Math_IntPower(INT64_MAX, 1) == INT64_MAX);
    CHECK(ZrCore_Math_IntPower(2, -1) == 0);
    return 0;
}

static int signed_min_exact(void) {
    const TZrInt64 min = -INT64_C(9223372036854775807) - 1;
    CHECK(ZrCore_Math_IntPower(min, 0) == 1);
    CHECK(ZrCore_Math_IntPower(min, 1) == min);
    CHECK(ZrCore_Math_IntPower(-2, 63) == min);
    CHECK(ZrCore_Math_IntPower(2, 63) == 0);
    return 0;
}

static int signed_min_overflow(void) {
    CHECK(ZrCore_Math_IntPower(int64_min(), 2) == 0);
    CHECK(ZrCore_Math_IntPower(int64_min(), 3) == 0);
    CHECK(ZrCore_Math_IntPower(INT64_MAX, 2) == 0);
    return 0;
}

int main(int argc, char **argv) {
    static const struct { const char *name; int (*run)(void); } cases[] = {
        {"uint_zero", uint_zero},
        {"uint_one_and_precision", uint_one_and_precision},
        {"uint_exact_large", uint_exact_large},
        {"uint_accumulated_overflow", uint_accumulated_overflow},
        {"signed_edges", signed_edges},
        {"signed_min_exact", signed_min_exact},
        {"signed_min_overflow", signed_min_overflow}
    };
    size_t i;
    if (argc != 2) return 2;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (strcmp(argv[1], cases[i].name) == 0) {
            int result = cases[i].run();
            if (result == 0) printf("PASS %s\n", argv[1]);
            return result;
        }
    }
    return 2;
}
