/* Persistent interpreter timing: compilation and process startup are untimed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"

static double median(double *values) {
    for (unsigned i = 1; i < 5; ++i) {
        double value = values[i];
        unsigned j = i;
        while (j > 0 && values[j - 1] > value) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = value;
    }
    return values[2];
}

int main(int argc, char **argv) {
    char source[256];
    unsigned long trips = argc == 2 ? strtoul(argv[1], ZR_NULL, 10) : 10000000u;
    TZrInt64 expected = (TZrInt64)(trips / 8u) * 28;
    double samples[2][5];
    TZrUInt64 instructions = 0;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrExecutionCancelToken *cancel = ZrCore_ExecutionCancelToken_New();
    if (state == ZR_NULL || cancel == ZR_NULL || trips == 0 || trips > 100000000u) return 2;
    for (unsigned i = 0; i < trips % 8u; ++i) expected += i;
    snprintf(source, sizeof(source), "var i = 0; var sum = 0; while (i < %lu) { "
            "sum = sum + (i & 7); i = i + 1; } return sum;", trips);
    SZrFunction *function = ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "budgeted_loop.zr"));
    if (function == ZR_NULL || !ZrCore_Function_AcquireHostRoot(state, function)) return 1;
    for (unsigned sample = 0; sample < 5; ++sample) {
        for (unsigned order = 0; order < 2; ++order) {
            unsigned mode = (sample + order) % 2;
            SZrExecutionBudget budget = {0};
            SZrTypeValue result;
            budget.hasDeadline = ZR_TRUE;
            budget.deadlineMicros = ZrCore_ExecutionBudget_NowMicros() + 60000000u;
            budget.cancelToken = cancel;
            state->executionBudget = mode ? &budget : ZR_NULL;
            TZrUInt64 started = ZrCore_ExecutionBudget_NowMicros();
            TZrBool success = ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result);
            samples[mode][sample] = (double)(ZrCore_ExecutionBudget_NowMicros() - started) / 1000.;
            state->executionBudget = ZR_NULL;
            if (!success || !ZR_VALUE_IS_TYPE_SIGNED_INT(result.type) ||
                result.value.nativeObject.nativeInt64 != expected) return 1;
            if (mode) instructions = budget.executedInstructions;
        }
    }
    double plain = median(samples[0]);
    double budgeted = median(samples[1]);
    printf("{\"checksum\":%lld,\"unbudgeted_median_ms\":%.3f,\"budgeted_median_ms\":%.3f,"
           "\"overhead_percent\":%.2f,\"executed_instructions\":%llu}\n",
            (long long)expected, plain, budgeted, plain ? (budgeted / plain - 1.) * 100. : 0.,
            (unsigned long long)instructions);
    ZrCore_ExecutionCancelToken_Free(cancel);
    ZrTests_Runtime_State_Destroy(state);
    return instructions > 0 ? 0 : 1;
}
