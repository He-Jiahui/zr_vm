#ifndef ZR_VM_PARSER_BACKEND_AOT_C_FRAME_CLEANUP_H
#define ZR_VM_PARSER_BACKEND_AOT_C_FRAME_CLEANUP_H

#include <stdio.h>

#include "backend_aot_internal.h"

void backend_aot_write_c_frame_cleanup(FILE *file, const SZrAotExecIrFrameLayout *frameLayout);

#endif
