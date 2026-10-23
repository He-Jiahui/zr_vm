/* Runtime ABI footprint probe; also guards the 64-bit r2 value/slot target. */
#include <stdio.h>
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/object.h"
int main(void) {
    printf("{\"value_bytes\":%zu,\"value_alignment\":%zu,\"stack_slot_bytes\":%zu,"
           "\"call_info_bytes\":%zu,\"raw_object_bytes\":%zu,\"string_bytes\":%zu,"
           "\"object_bytes\":%zu,\"function_bytes\":%zu}\n",
           sizeof(SZrTypeValue), _Alignof(SZrTypeValue), sizeof(SZrTypeValueOnStack),
           sizeof(SZrCallInfo), sizeof(SZrRawObject), sizeof(SZrString),
           sizeof(SZrObject), sizeof(SZrFunction));
    return sizeof(void *) == 8u &&
           (sizeof(SZrTypeValue) != 40u || sizeof(SZrTypeValueOnStack) != 48u);
}
