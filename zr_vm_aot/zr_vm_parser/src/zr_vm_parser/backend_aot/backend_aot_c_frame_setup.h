#ifndef ZR_VM_PARSER_BACKEND_AOT_C_FRAME_SETUP_H
#define ZR_VM_PARSER_BACKEND_AOT_C_FRAME_SETUP_H

#include <stdio.h>

#include "zr_vm_common/zr_common_conf.h"
#include "backend_aot_exec_ir.h"

void backend_aot_write_c_frame_setup(FILE *file,
                                     const SZrAotExecIrFrameLayout *frameLayout,
                                     TZrUInt32 functionIndex);

#endif
