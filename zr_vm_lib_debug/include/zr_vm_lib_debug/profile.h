#ifndef ZR_VM_DEBUG_PROFILE_H
#define ZR_VM_DEBUG_PROFILE_H

#include "zr_vm_lib_debug/conf.h"
#include "zr_vm_core/debug.h"

#define ZR_DEBUG_PROFILE_NAME_CAPACITY ZR_DEBUG_NAME_CAPACITY
#define ZR_DEBUG_PROFILE_SOURCE_CAPACITY ZR_DEBUG_TEXT_CAPACITY

typedef struct ZrDebugProfileEntry {
    const struct SZrFunction *function;
    TZrChar name[ZR_DEBUG_PROFILE_NAME_CAPACITY];
    TZrChar source[ZR_DEBUG_PROFILE_SOURCE_CAPACITY];
    TZrUInt64 call_count;
    TZrUInt64 return_count;
    TZrUInt64 total_time_ns;
    TZrUInt64 self_time_ns;
} ZrDebugProfileEntry;

typedef struct ZrDebugProfileSample {
    const struct SZrFunction *function;
    TZrChar name[ZR_DEBUG_PROFILE_NAME_CAPACITY];
    TZrChar source[ZR_DEBUG_PROFILE_SOURCE_CAPACITY];
    TZrSize line;
    TZrUInt64 sample_count;
} ZrDebugProfileSample;

typedef struct ZrDebugProfileFrame {
    TZrSize entry_index;
    TZrUInt64 start_time_ns;
    TZrUInt64 child_time_ns;
} ZrDebugProfileFrame;

typedef struct ZrDebugProfile {
    struct SZrState *state;
    FZrDebugHook previous_hook;
    TZrUInt32 previous_mask;
    TZrUInt32 previous_count;
    TZrBool active;
    ZrDebugProfileEntry *entries;
    TZrSize entry_count;
    TZrSize entry_capacity;
    ZrDebugProfileFrame *frames;
    TZrSize frame_count;
    TZrSize frame_capacity;
    ZrDebugProfileSample *samples;
    TZrSize sample_count;
    TZrSize sample_capacity;
    TZrUInt64 total_sample_count;
    TZrUInt32 sample_period;
    struct ZrDebugProfile *next_active;
} ZrDebugProfile;

ZR_DEBUG_API void ZrDebug_Profile_Init(ZrDebugProfile *profile);
ZR_DEBUG_API void ZrDebug_Profile_Reset(ZrDebugProfile *profile);
ZR_DEBUG_API TZrBool ZrDebug_Profile_Start(ZrDebugProfile *profile, struct SZrState *state);
ZR_DEBUG_API TZrBool ZrDebug_Profile_StartWithSampling(ZrDebugProfile *profile,
                                                       struct SZrState *state,
                                                       TZrUInt32 samplePeriod);
ZR_DEBUG_API void ZrDebug_Profile_Stop(ZrDebugProfile *profile);
ZR_DEBUG_API void ZrDebug_Profile_Destroy(ZrDebugProfile *profile);
ZR_DEBUG_API TZrSize ZrDebug_Profile_GetEntryCount(const ZrDebugProfile *profile);
ZR_DEBUG_API const ZrDebugProfileEntry *ZrDebug_Profile_GetEntry(const ZrDebugProfile *profile, TZrSize index);
ZR_DEBUG_API const ZrDebugProfileEntry *ZrDebug_Profile_FindByName(const ZrDebugProfile *profile,
                                                                   const TZrChar *name);
ZR_DEBUG_API TZrSize ZrDebug_Profile_GetSampleCount(const ZrDebugProfile *profile);
ZR_DEBUG_API TZrUInt64 ZrDebug_Profile_GetTotalSampleCount(const ZrDebugProfile *profile);
ZR_DEBUG_API const ZrDebugProfileSample *ZrDebug_Profile_GetSample(const ZrDebugProfile *profile, TZrSize index);

#endif
