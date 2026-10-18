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
    char loopSource[256];
    char *fileSource = ZR_NULL;
    const char *source = loopSource;
    TZrBool fromFile = (TZrBool)(argc >= 3 && strcmp(argv[1], "--source") == 0);
    unsigned long trips = !fromFile && argc == 2 ? strtoul(argv[1], ZR_NULL, 10) : 10000000u;
    TZrInt64 expected = (TZrInt64)(trips / 8u) * 28;
    TZrBool hasExpected = !fromFile;
    if ((!fromFile && argc > 2) || (fromFile && argc != 3 && argc != 5)) return 2;
    if (fromFile) {
        FILE *file = fopen(argv[2], "rb");
        if (file == ZR_NULL) return 2;
        fileSource = malloc(1024u * 1024u + 1u);
        if (fileSource == ZR_NULL) { fclose(file); return 2; }
        size_t length = fread(fileSource, 1u, 1024u * 1024u, file);
        int extra = fgetc(file);
        TZrBool valid = (TZrBool)(!ferror(file) && extra == EOF && length != 0u);
        fclose(file);
        if (!valid) { free(fileSource); return 2; }
        fileSource[length] = '\0';
        source = fileSource;
        if (argc == 5) {
            char *end;
            if (strcmp(argv[3], "--expected") != 0) { free(fileSource); return 2; }
            expected = strtoll(argv[4], &end, 10);
            if (end == argv[4] || *end != '\0') { free(fileSource); return 2; }
            hasExpected = ZR_TRUE;
        }
    }
    double samples[2][5];
    TZrUInt64 instructions = 0;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrExecutionCancelToken *cancel = ZrCore_ExecutionCancelToken_New();
    if (state == ZR_NULL || cancel == ZR_NULL || trips == 0 || trips > 100000000u) return 2;
    if (!fromFile) {
        for (unsigned i = 0; i < trips % 8u; ++i) expected += i;
        snprintf(loopSource, sizeof(loopSource), "var i = 0; var sum = 0; while (i < %lu) { "
            "sum = sum + (i & 7); i = i + 1; } return sum;", trips);
    }
    SZrFunction *function = ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "budgeted_loop.zr"));
    free(fileSource);
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
            if (!success || !ZR_VALUE_IS_TYPE_SIGNED_INT(result.type)) return 1;
            if (!hasExpected) {
                expected = result.value.nativeObject.nativeInt64;
                hasExpected = ZR_TRUE;
            }
            if (result.value.nativeObject.nativeInt64 != expected) return 1;
            if (mode) instructions = budget.executedInstructions;
        }
    }
    double plain = median(samples[0]);
    double budgeted = median(samples[1]);
    printf("{\"checksum\":%lld,\"unbudgeted_median_ms\":%.3f,\"budgeted_median_ms\":%.3f,"
           "\"overhead_percent\":%.2f,\"executed_instructions\":%llu,\"budget_ns_per_instruction\":%.3f}\n",
            (long long)expected, plain, budgeted, plain ? (budgeted / plain - 1.) * 100. : 0.,
            (unsigned long long)instructions,
            instructions ? (budgeted - plain) * 1e6 / (double)instructions : 0.);
    ZrCore_ExecutionCancelToken_Free(cancel);
    ZrTests_Runtime_State_Destroy(state);
    return instructions > 0 ? 0 : 1;
}
