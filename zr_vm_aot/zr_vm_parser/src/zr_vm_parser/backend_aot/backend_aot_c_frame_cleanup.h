#ifndef ZR_VM_PARSER_BACKEND_AOT_C_FRAME_CLEANUP_H
#define ZR_VM_PARSER_BACKEND_AOT_C_FRAME_CLEANUP_H

#include <stdio.h>

#include "backend_aot_internal.h"

TZrBool backend_aot_c_frame_cleanup_would_emit(const SZrAotExecIrFrameLayout *frameLayout);
void backend_aot_write_c_frame_cleanup(FILE *file, const SZrAotExecIrFrameLayout *frameLayout);

#endif
