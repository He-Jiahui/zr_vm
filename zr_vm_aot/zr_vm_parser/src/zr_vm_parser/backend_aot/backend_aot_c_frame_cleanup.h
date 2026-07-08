#ifndef ZR_VM_PARSER_BACKEND_AOT_C_FRAME_CLEANUP_H
#define ZR_VM_PARSER_BACKEND_AOT_C_FRAME_CLEANUP_H

#include <stdio.h>

#include "backend_aot_internal.h"

TZrBool backend_aot_c_frame_cleanup_would_emit_for_function(SZrState *state,
                                                            const SZrAotExecIrFunction *functionIr);
void backend_aot_write_c_frame_root_cleanup(FILE *file);
void backend_aot_write_c_frame_cleanup(FILE *file,
                                       SZrState *state,
                                       const SZrAotExecIrFunction *functionIr);

#endif
