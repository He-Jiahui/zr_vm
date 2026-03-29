#ifndef ZR_VM_TESTS_RUNTIME_SUPPORT_H
#define ZR_VM_TESTS_RUNTIME_SUPPORT_H

#include <stdio.h>
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

#define ZR_TEST_START(summary)                                                                                         \
    do {                                                                                                               \
        printf("Unit Test - %s\n", summary);                                                                           \
        fflush(stdout);                                                                                                \
    } while (0)

#define ZR_TEST_INFO(summary, details)                                                                                 \
    do {                                                                                                               \
        printf("Testing %s:\n %s\n", summary, details);                                                                \
        fflush(stdout);                                                                                                \
    } while (0)

#define ZR_TEST_PASS(timer, summary)                                                                                   \
    do {                                                                                                               \
        double zrTestElapsed = ((double) ((timer).endTime - (timer).startTime) / CLOCKS_PER_SEC) * 1000.0;           \
        printf("Pass - Cost Time:%.3fms - %s\n", zrTestElapsed, summary);                                             \
        fflush(stdout);                                                                                                \
    } while (0)

#define ZR_TEST_FAIL(timer, summary, reason)                                                                           \
    do {                                                                                                               \
        double zrTestElapsed = ((double) ((timer).endTime - (timer).startTime) / CLOCKS_PER_SEC) * 1000.0;           \
        printf("Fail - Cost Time:%.3fms - %s:\n %s\n", zrTestElapsed, summary, reason);                               \
        fflush(stdout);                                                                                                \
    } while (0)

#define ZR_TEST_DIVIDER()                                                                                              \
    do {                                                                                                               \
        printf("----------\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

#define ZR_TEST_MODULE_DIVIDER()                                                                                       \
    do {                                                                                                               \
        printf("==========\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

TZrPtr ZrTests_Runtime_Allocator_Default(TZrPtr userData,
                                         TZrPtr pointer,
                                         TZrSize originalSize,
                                         TZrSize newSize,
                                         TZrInt64 flag);

SZrState *ZrTests_Runtime_State_Create(FZrPanicHandlingFunction panicHandler);

void ZrTests_Runtime_State_Destroy(SZrState *state);

TZrBool ZrTests_Runtime_Function_Execute(SZrState *state, SZrFunction *function, SZrTypeValue *result);

TZrBool ZrTests_Runtime_Function_ExecuteExpectInt64(SZrState *state, SZrFunction *function, TZrInt64 *result);

#endif
