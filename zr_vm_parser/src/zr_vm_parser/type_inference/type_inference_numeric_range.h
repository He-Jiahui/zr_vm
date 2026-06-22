#ifndef ZR_VM_PARSER_TYPE_INFERENCE_NUMERIC_RANGE_H
#define ZR_VM_PARSER_TYPE_INFERENCE_NUMERIC_RANGE_H

#include "zr_vm_parser/type_inference.h"

TZrBool type_inference_numeric_range_checked_int64_binary_result(const TZrChar *op,
                                                                 TZrInt64 left,
                                                                 TZrInt64 right,
                                                                 TZrInt64 *outValue);

#endif
