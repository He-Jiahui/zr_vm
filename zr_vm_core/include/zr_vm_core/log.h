//
// Created by HeJiahui on 2025/6/20.
//

#ifndef ZR_VM_CORE_LOG_H
#define ZR_VM_CORE_LOG_H
#include "zr_vm_core/conf.h"

struct SZrState;

typedef enum EZrOutputChannel {
    ZR_OUTPUT_CHANNEL_STDOUT = 0,
    ZR_OUTPUT_CHANNEL_STDERR = 1
} EZrOutputChannel;

typedef enum EZrOutputKind {
    ZR_OUTPUT_KIND_RESULT = 0,
    ZR_OUTPUT_KIND_HELP = 1,
    ZR_OUTPUT_KIND_META = 2,
    ZR_OUTPUT_KIND_DIAGNOSTIC = 3
} EZrOutputKind;

typedef void (*FZrLog)(struct SZrState *state,
                       EZrLogLevel level,
                       EZrOutputChannel channel,
                       EZrOutputKind kind,
                       TZrNativeString message);

ZR_CORE_API void ZrCore_Log_Write(struct SZrState *state,
                                  EZrLogLevel level,
                                  EZrOutputChannel channel,
                                  EZrOutputKind kind,
                                  TZrNativeString message);

ZR_CORE_API void ZrCore_Log_Printf(struct SZrState *state,
                                   EZrLogLevel level,
                                   EZrOutputChannel channel,
                                   EZrOutputKind kind,
                                   TZrNativeString format,
                                   ...);

ZR_CORE_API void ZrCore_Log_Resultf(struct SZrState *state, TZrNativeString format, ...);

ZR_CORE_API void ZrCore_Log_Helpf(struct SZrState *state, TZrNativeString format, ...);

ZR_CORE_API void ZrCore_Log_Metaf(struct SZrState *state, TZrNativeString format, ...);

ZR_CORE_API void ZrCore_Log_Diagnosticf(struct SZrState *state,
                                        EZrLogLevel level,
                                        EZrOutputChannel channel,
                                        TZrNativeString format,
                                        ...);

ZR_CORE_API void ZrCore_Log_Error(struct SZrState *state, TZrNativeString format, ...);

ZR_CORE_API void ZrCore_Log_Exception(struct SZrState *state, TZrNativeString format, ...);

ZR_CORE_API void ZrCore_Log_Fatal(struct SZrState *state, TZrNativeString format, ...);

ZR_CORE_API void ZrCore_Log_FlushDefaultSinks(void);

#endif //ZR_VM_CORE_LOG_H
