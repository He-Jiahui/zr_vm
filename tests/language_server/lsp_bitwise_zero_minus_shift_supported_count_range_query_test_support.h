#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_LSP_BITWISE_ZERO_MINUS_SHIFT_SUPPORTED_COUNT_RANGE_QUERY_TEST_SUPPORT_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_LSP_BITWISE_ZERO_MINUS_SHIFT_SUPPORTED_COUNT_RANGE_QUERY_TEST_SUPPORT_H

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/state.h"

TZrBool ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
        SZrState *state,
        const TZrChar *caseName,
        const TZrChar *expression,
        TZrInt64 expectedMin,
        TZrInt64 expectedMax);

#endif
