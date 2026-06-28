#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_LSP_NUMERIC_RANGE_QUERY_TEST_SUPPORT_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_LSP_NUMERIC_RANGE_QUERY_TEST_SUPPORT_H

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/state.h"

TZrPtr ZrVmTest_LspNumericRangeQueryAllocator(TZrPtr userData,
                                              TZrPtr pointer,
                                              TZrSize originalSize,
                                              TZrSize newSize,
                                              TZrInt64 flag);

TZrBool ZrVmTest_LspRunAssignmentRangeCaseAt(SZrState *state,
                                             const TZrChar *label,
                                             const TZrChar *uriText,
                                             const TZrChar *content,
                                             const TZrChar *needle,
                                             TZrSize offset,
                                             TZrInt64 expectedMin,
                                             TZrInt64 expectedMax);

#endif
