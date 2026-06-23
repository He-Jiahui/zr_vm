#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_I64_THUNK_SHAPES_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_I64_THUNK_SHAPES_H

#include "backend_aot_internal.h"

TZrBool backend_aot_c_try_get_i64_constant_return(const SZrFunction *function, TZrInt64 *outValue);
TZrBool backend_aot_c_try_get_i64_identity_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_negate_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_bitwise_not_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_bitwise_and_constant_return(const SZrFunction *function,
                                                                   TZrInt64 *outValue);
TZrBool backend_aot_c_try_get_i64_arg0_bitwise_or_constant_return(const SZrFunction *function,
                                                                  TZrInt64 *outValue);
TZrBool backend_aot_c_try_get_i64_arg0_bitwise_xor_constant_return(const SZrFunction *function,
                                                                   TZrInt64 *outValue);
TZrBool backend_aot_c_try_get_i64_arg0_add_constant_return(const SZrFunction *function, TZrInt64 *outValue);
TZrBool backend_aot_c_try_get_i64_arg0_subtract_constant_return(const SZrFunction *function,
                                                               TZrInt64 *outValue);
TZrBool backend_aot_c_try_get_i64_arg0_multiply_constant_return(const SZrFunction *function,
                                                               TZrInt64 *outValue);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_add_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_add_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_subtract_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_multiply_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_divide_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_modulo_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_bitwise_and_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_bitwise_or_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_bitwise_xor_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_subtract_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_multiply_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_divide_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_modulo_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_and_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_or_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_i64_arg0_arg1_arg2_bitwise_xor_return(const SZrFunction *function);

#endif
