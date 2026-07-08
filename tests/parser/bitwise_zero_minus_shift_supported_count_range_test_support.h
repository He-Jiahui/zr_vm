#ifndef ZR_VM_TESTS_PARSER_BITWISE_ZERO_MINUS_SHIFT_SUPPORTED_COUNT_RANGE_TEST_SUPPORT_H
#define ZR_VM_TESTS_PARSER_BITWISE_ZERO_MINUS_SHIFT_SUPPORTED_COUNT_RANGE_TEST_SUPPORT_H

#include "zr_vm_common/zr_type_conf.h"

void assert_bitwise_zero_minus_shift_supported_count_range(const char *sourceNameText,
                                                           const char *source,
                                                           TZrInt64 expectedMin,
                                                           TZrInt64 expectedMax);

#endif
