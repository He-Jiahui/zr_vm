#ifndef ZR_VM_PARSER_WRITER_INTERMEDIATE_GENERATED_SOURCE_MAP_H
#define ZR_VM_PARSER_WRITER_INTERMEDIATE_GENERATED_SOURCE_MAP_H

#include <stdio.h>

#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"

TZrBool writer_intermediate_validate_function_prototype_data(
        const SZrFunction *function);

void writer_intermediate_write_generated_source_maps(
        FILE *file,
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 indentLevel);

#endif // ZR_VM_PARSER_WRITER_INTERMEDIATE_GENERATED_SOURCE_MAP_H
