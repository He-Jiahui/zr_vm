#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SYMBOLIC_MATH_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SYMBOLIC_MATH_H

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_type_conf.h"

TZrBool loop_assignment_sequence_symbolic_int64_add(TZrInt64 left,
                                                    TZrInt64 right,
                                                    TZrInt64 *outValue);

TZrBool loop_assignment_sequence_symbolic_int64_mul(TZrInt64 left,
                                                    TZrInt64 right,
                                                    TZrInt64 *outValue);

TZrBool loop_assignment_sequence_symbolic_int64_div(TZrInt64 left,
                                                    TZrInt64 right,
                                                    TZrInt64 *outValue);

TZrBool loop_assignment_sequence_symbolic_int64_mod(TZrInt64 left,
                                                    TZrInt64 right,
                                                    TZrInt64 *outValue);

TZrBool loop_assignment_sequence_symbolic_int64_shift_left(TZrInt64 left,
                                                           TZrInt64 right,
                                                           TZrInt64 *outValue);

TZrBool loop_assignment_sequence_symbolic_int64_shift_right(TZrInt64 left,
                                                            TZrInt64 right,
                                                            TZrInt64 *outValue);

TZrBool loop_assignment_sequence_symbolic_int64_bitwise_and(TZrInt64 left,
                                                            TZrInt64 right,
                                                            TZrInt64 *outValue);

TZrBool loop_assignment_sequence_symbolic_int64_bitwise_or(TZrInt64 left,
                                                           TZrInt64 right,
                                                           TZrInt64 *outValue);

TZrBool loop_assignment_sequence_symbolic_int64_bitwise_xor(TZrInt64 left,
                                                            TZrInt64 right,
                                                            TZrInt64 *outValue);

TZrBool loop_assignment_sequence_symbolic_int64_range_add(TZrInt64 leftMin,
                                                          TZrInt64 leftMax,
                                                          TZrInt64 rightMin,
                                                          TZrInt64 rightMax,
                                                          TZrInt64 *outMin,
                                                          TZrInt64 *outMax);

TZrBool loop_assignment_sequence_symbolic_int64_range_mul(TZrInt64 leftMin,
                                                          TZrInt64 leftMax,
                                                          TZrInt64 rightMin,
                                                          TZrInt64 rightMax,
                                                          TZrInt64 *outMin,
                                                          TZrInt64 *outMax);

TZrBool loop_assignment_sequence_symbolic_add_signed_range(TZrInt64 currentMin,
                                                           TZrInt64 currentMax,
                                                           TZrInt64 termMin,
                                                           TZrInt64 termMax,
                                                           TZrInt32 sign,
                                                           TZrInt64 *outMin,
                                                           TZrInt64 *outMax);

#endif
