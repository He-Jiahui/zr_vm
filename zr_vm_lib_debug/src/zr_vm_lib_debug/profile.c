#include "zr_vm_lib_debug/profile.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "zr_vm_core/function.h"

static ZrDebugProfile *g_active_profiles = ZR_NULL;

typedef struct ZrDebugProfileLocation {
    const SZrFunction *function;
    const TZrChar *name;
    const TZrChar *source;
    TZrSize line;
} ZrDebugProfileLocation;

static TZrUInt64 zr_debug_profile_now_ns(void) {
    clock_t ticks = clock();
    if (ticks == (clock_t)-1) {
        return 0u;
    }
    return ((TZrUInt64)ticks * 1000000000ull) / (TZrUInt64)CLOCKS_PER_SEC;
}

static void zr_debug_profile_copy_text(TZrChar *destination, TZrSize destinationSize, const TZrChar *source) {
    TZrSize length;

    if (destination == ZR_NULL || destinationSize == 0u) {
        return;
    }

    if (source == ZR_NULL) {
        source = "";
    }

    length = strlen(source);
    if (length >= destinationSize) {
        length = destinationSize - 1u;
    }
    if (length > 0u) {
        memcpy(destination, source, length);
    }
    destination[length] = '\0';
}

static TZrUInt32 zr_debug_profile_event_mask(EZrDebugHookEvent event) {
    if (event >= ZR_DEBUG_HOOK_EVENT_MAX) {
        return 0u;
    }
    return 1u << event;
}

static ZrDebugProfile *zr_debug_profile_find_active(SZrState *state) {
    ZrDebugProfile *profile;

    for (profile = g_active_profiles; profile != ZR_NULL; profile = profile->next_active) {
        if (profile->state == state) {
            return profile;
        }
    }

    return ZR_NULL;
}

static TZrBool zr_debug_profile_register_active(ZrDebugProfile *profile) {
    if (profile == ZR_NULL || profile->state == ZR_NULL) {
        return ZR_FALSE;
    }
    if (zr_debug_profile_find_active(profile->state) != ZR_NULL) {
        return ZR_FALSE;
    }

    profile->next_active = g_active_profiles;
    g_active_profiles = profile;
    return ZR_TRUE;
}

static void zr_debug_profile_unregister_active(ZrDebugProfile *profile) {
    ZrDebugProfile **cursor = &g_active_profiles;

    while (*cursor != ZR_NULL) {
        if (*cursor == profile) {
            *cursor = profile->next_active;
            profile->next_active = ZR_NULL;
            return;
        }
        cursor = &(*cursor)->next_active;
    }
}

static TZrBool zr_debug_profile_reserve_entries(ZrDebugProfile *profile, TZrSize minimumCapacity) {
    ZrDebugProfileEntry *entries;
    TZrSize newCapacity;

    if (profile == ZR_NULL) {
        return ZR_FALSE;
    }
    if (profile->entry_capacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = profile->entry_capacity == 0u ? 8u : profile->entry_capacity * 2u;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2u;
    }

    entries = (ZrDebugProfileEntry *)realloc(profile->entries, sizeof(*entries) * newCapacity);
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }

    profile->entries = entries;
    profile->entry_capacity = newCapacity;
    return ZR_TRUE;
}

static TZrBool zr_debug_profile_reserve_frames(ZrDebugProfile *profile, TZrSize minimumCapacity) {
    ZrDebugProfileFrame *frames;
    TZrSize newCapacity;

    if (profile == ZR_NULL) {
        return ZR_FALSE;
    }
    if (profile->frame_capacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = profile->frame_capacity == 0u ? 16u : profile->frame_capacity * 2u;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2u;
    }

    frames = (ZrDebugProfileFrame *)realloc(profile->frames, sizeof(*frames) * newCapacity);
    if (frames == ZR_NULL) {
        return ZR_FALSE;
    }

    profile->frames = frames;
    profile->frame_capacity = newCapacity;
    return ZR_TRUE;
}

static TZrBool zr_debug_profile_reserve_samples(ZrDebugProfile *profile, TZrSize minimumCapacity) {
    ZrDebugProfileSample *samples;
    TZrSize newCapacity;

    if (profile == ZR_NULL) {
        return ZR_FALSE;
    }
    if (profile->sample_capacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = profile->sample_capacity == 0u ? 16u : profile->sample_capacity * 2u;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2u;
    }

    samples = (ZrDebugProfileSample *)realloc(profile->samples, sizeof(*samples) * newCapacity);
    if (samples == ZR_NULL) {
        return ZR_FALSE;
    }

    profile->samples = samples;
    profile->sample_capacity = newCapacity;
    return ZR_TRUE;
}

static TZrSize zr_debug_profile_find_or_add_entry(ZrDebugProfile *profile,
                                                  const SZrFunction *function,
                                                  const TZrChar *name,
                                                  const TZrChar *source) {
    TZrSize index;
    ZrDebugProfileEntry *entry;

    if (profile == ZR_NULL || function == ZR_NULL) {
        return (TZrSize)-1;
    }

    for (index = 0u; index < profile->entry_count; index++) {
        if (profile->entries[index].function == function) {
            return index;
        }
    }

    if (!zr_debug_profile_reserve_entries(profile, profile->entry_count + 1u)) {
        return (TZrSize)-1;
    }

    index = profile->entry_count++;
    entry = &profile->entries[index];
    memset(entry, 0, sizeof(*entry));
    entry->function = function;
    zr_debug_profile_copy_text(entry->name, sizeof(entry->name), name != ZR_NULL && name[0] != '\0' ? name : "<anonymous>");
    zr_debug_profile_copy_text(entry->source, sizeof(entry->source), source);
    return index;
}

static TZrSize zr_debug_profile_find_or_add_sample(ZrDebugProfile *profile,
                                                   const SZrFunction *function,
                                                   TZrSize line,
                                                   const TZrChar *name,
                                                   const TZrChar *source) {
    TZrSize index;
    ZrDebugProfileSample *sample;

    if (profile == ZR_NULL || function == ZR_NULL) {
        return (TZrSize)-1;
    }

    for (index = 0u; index < profile->sample_count; index++) {
        if (profile->samples[index].function == function && profile->samples[index].line == line) {
            return index;
        }
    }

    if (!zr_debug_profile_reserve_samples(profile, profile->sample_count + 1u)) {
        return (TZrSize)-1;
    }

    index = profile->sample_count++;
    sample = &profile->samples[index];
    memset(sample, 0, sizeof(*sample));
    sample->function = function;
    sample->line = line;
    zr_debug_profile_copy_text(sample->name,
                               sizeof(sample->name),
                               name != ZR_NULL && name[0] != '\0' ? name : "<anonymous>");
    zr_debug_profile_copy_text(sample->source, sizeof(sample->source), source);
    return index;
}

static TZrBool zr_debug_profile_capture_location(SZrState *state, ZrDebugProfileLocation *outLocation) {
    SZrDebugActivation activation;
    SZrDebugInfo info;

    if (state == ZR_NULL || outLocation == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outLocation, 0, sizeof(*outLocation));
    memset(&activation, 0, sizeof(activation));
    if (!ZrCore_Debug_GetStack(state, 0u, &activation) || activation.function == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&info, 0, sizeof(info));
    if (ZrCore_Debug_GetInfo(state,
                             &activation,
                             (EZrDebugInfoType)(ZR_DEBUG_INFO_FUNCTION_NAME |
                                                ZR_DEBUG_INFO_SOURCE_FILE |
                                                ZR_DEBUG_INFO_LINE_NUMBER),
                             &info)) {
        outLocation->name = info.name;
        outLocation->source = info.source;
        outLocation->line = info.currentLine;
    }
    outLocation->function = activation.function;
    return ZR_TRUE;
}

static TZrBool zr_debug_profile_capture_frame(ZrDebugProfile *profile,
                                              SZrState *state,
                                              TZrSize *outEntryIndex) {
    ZrDebugProfileLocation location;
    TZrSize entryIndex;

    if (profile == ZR_NULL || outEntryIndex == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!zr_debug_profile_capture_location(state, &location)) {
        return ZR_FALSE;
    }

    entryIndex = zr_debug_profile_find_or_add_entry(profile, location.function, location.name, location.source);
    if (entryIndex == (TZrSize)-1) {
        return ZR_FALSE;
    }

    *outEntryIndex = entryIndex;
    return ZR_TRUE;
}

static void zr_debug_profile_record_call(ZrDebugProfile *profile, SZrState *state) {
    TZrSize entryIndex;
    ZrDebugProfileFrame *frame;

    if (!zr_debug_profile_capture_frame(profile, state, &entryIndex)) {
        return;
    }

    profile->entries[entryIndex].call_count++;
    if (!zr_debug_profile_reserve_frames(profile, profile->frame_count + 1u)) {
        return;
    }

    frame = &profile->frames[profile->frame_count++];
    memset(frame, 0, sizeof(*frame));
    frame->entry_index = entryIndex;
    frame->start_time_ns = zr_debug_profile_now_ns();
}

static void zr_debug_profile_record_return(ZrDebugProfile *profile, SZrState *state) {
    TZrSize entryIndex;
    TZrSize frameIndex;
    TZrUInt64 now;
    TZrUInt64 elapsed;
    TZrUInt64 selfTime;
    ZrDebugProfileFrame frame;

    if (!zr_debug_profile_capture_frame(profile, state, &entryIndex)) {
        return;
    }

    profile->entries[entryIndex].return_count++;
    if (profile->frame_count == 0u) {
        return;
    }

    frameIndex = profile->frame_count;
    while (frameIndex > 0u) {
        frameIndex--;
        if (profile->frames[frameIndex].entry_index == entryIndex) {
            break;
        }
    }
    if (profile->frames[frameIndex].entry_index != entryIndex) {
        return;
    }

    frame = profile->frames[frameIndex];
    profile->frame_count = frameIndex;
    now = zr_debug_profile_now_ns();
    elapsed = now >= frame.start_time_ns ? now - frame.start_time_ns : 0u;
    selfTime = elapsed >= frame.child_time_ns ? elapsed - frame.child_time_ns : 0u;
    profile->entries[entryIndex].total_time_ns += elapsed;
    profile->entries[entryIndex].self_time_ns += selfTime;
    if (profile->frame_count > 0u) {
        profile->frames[profile->frame_count - 1u].child_time_ns += elapsed;
    }
}

static void zr_debug_profile_record_sample(ZrDebugProfile *profile, SZrState *state) {
    ZrDebugProfileLocation location;
    TZrSize sampleIndex;

    if (profile == ZR_NULL || profile->sample_period == 0u) {
        return;
    }
    if (!zr_debug_profile_capture_location(state, &location)) {
        return;
    }

    sampleIndex = zr_debug_profile_find_or_add_sample(profile,
                                                      location.function,
                                                      location.line,
                                                      location.name,
                                                      location.source);
    if (sampleIndex == (TZrSize)-1) {
        return;
    }

    profile->samples[sampleIndex].sample_count++;
    profile->total_sample_count++;
}

static void zr_debug_profile_hook(SZrState *state, SZrDebugInfo *debugInfo) {
    ZrDebugProfile *profile = zr_debug_profile_find_active(state);

    if (profile != ZR_NULL && debugInfo != ZR_NULL) {
        if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_CALL) {
            zr_debug_profile_record_call(profile, state);
        } else if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_RETURN) {
            zr_debug_profile_record_return(profile, state);
        } else if (debugInfo->event == ZR_DEBUG_HOOK_EVENT_COUNT) {
            zr_debug_profile_record_sample(profile, state);
        }
    }

    if (profile != ZR_NULL &&
        profile->previous_hook != ZR_NULL &&
        debugInfo != ZR_NULL &&
        (profile->previous_mask & zr_debug_profile_event_mask(debugInfo->event)) != 0u) {
        profile->previous_hook(state, debugInfo);
    }
}

ZR_DEBUG_API void ZrDebug_Profile_Init(ZrDebugProfile *profile) {
    if (profile == ZR_NULL) {
        return;
    }
    memset(profile, 0, sizeof(*profile));
}

ZR_DEBUG_API void ZrDebug_Profile_Reset(ZrDebugProfile *profile) {
    if (profile == ZR_NULL) {
        return;
    }
    profile->entry_count = 0u;
    profile->frame_count = 0u;
    profile->sample_count = 0u;
    profile->total_sample_count = 0u;
}

static TZrBool zr_debug_profile_start_internal(ZrDebugProfile *profile, SZrState *state, TZrUInt32 samplePeriod) {
    TZrUInt32 mask;
    TZrUInt32 count;

    if (profile == ZR_NULL || state == ZR_NULL || profile->active) {
        return ZR_FALSE;
    }

    profile->state = state;
    profile->previous_hook = ZrCore_Debug_GetHook(state);
    profile->previous_mask = ZrCore_Debug_GetHookMask(state);
    profile->previous_count = ZrCore_Debug_GetHookCount(state);
    if (profile->previous_hook == ZR_NULL && profile->previous_mask != 0u) {
        profile->state = ZR_NULL;
        profile->previous_mask = 0u;
        profile->previous_count = 0u;
        return ZR_FALSE;
    }
    if (samplePeriod != 0u &&
        (profile->previous_mask & ZR_DEBUG_HOOK_MASK_COUNT) != 0u &&
        profile->previous_count != samplePeriod) {
        profile->state = ZR_NULL;
        profile->previous_hook = ZR_NULL;
        profile->previous_mask = 0u;
        profile->previous_count = 0u;
        return ZR_FALSE;
    }
    profile->frame_count = 0u;
    if (!zr_debug_profile_register_active(profile)) {
        profile->state = ZR_NULL;
        profile->previous_hook = ZR_NULL;
        profile->previous_mask = 0u;
        profile->previous_count = 0u;
        return ZR_FALSE;
    }

    mask = profile->previous_mask | ZR_DEBUG_HOOK_MASK_CALL | ZR_DEBUG_HOOK_MASK_RETURN;
    count = profile->previous_count;
    if (samplePeriod != 0u) {
        mask |= ZR_DEBUG_HOOK_MASK_COUNT;
        count = samplePeriod;
    }
    profile->sample_period = samplePeriod;
    profile->active = ZR_TRUE;
    ZrCore_Debug_SetHook(state, zr_debug_profile_hook, mask, count);
    return ZR_TRUE;
}

ZR_DEBUG_API TZrBool ZrDebug_Profile_Start(ZrDebugProfile *profile, SZrState *state) {
    return zr_debug_profile_start_internal(profile, state, 0u);
}

ZR_DEBUG_API TZrBool ZrDebug_Profile_StartWithSampling(ZrDebugProfile *profile,
                                                       SZrState *state,
                                                       TZrUInt32 samplePeriod) {
    if (samplePeriod == 0u) {
        return ZR_FALSE;
    }
    return zr_debug_profile_start_internal(profile, state, samplePeriod);
}

ZR_DEBUG_API void ZrDebug_Profile_Stop(ZrDebugProfile *profile) {
    SZrState *state;

    if (profile == ZR_NULL || !profile->active) {
        return;
    }

    state = profile->state;
    if (state != ZR_NULL) {
        ZrCore_Debug_SetHook(state, profile->previous_hook, profile->previous_mask, profile->previous_count);
    }
    zr_debug_profile_unregister_active(profile);
    profile->state = ZR_NULL;
    profile->previous_hook = ZR_NULL;
    profile->previous_mask = 0u;
    profile->previous_count = 0u;
    profile->active = ZR_FALSE;
    profile->frame_count = 0u;
    profile->sample_period = 0u;
}

ZR_DEBUG_API void ZrDebug_Profile_Destroy(ZrDebugProfile *profile) {
    if (profile == ZR_NULL) {
        return;
    }
    if (profile->active) {
        ZrDebug_Profile_Stop(profile);
    }
    free(profile->entries);
    free(profile->frames);
    free(profile->samples);
    ZrDebug_Profile_Init(profile);
}

ZR_DEBUG_API TZrSize ZrDebug_Profile_GetEntryCount(const ZrDebugProfile *profile) {
    return profile != ZR_NULL ? profile->entry_count : 0u;
}

ZR_DEBUG_API const ZrDebugProfileEntry *ZrDebug_Profile_GetEntry(const ZrDebugProfile *profile, TZrSize index) {
    if (profile == ZR_NULL || index >= profile->entry_count) {
        return ZR_NULL;
    }
    return &profile->entries[index];
}

ZR_DEBUG_API const ZrDebugProfileEntry *ZrDebug_Profile_FindByName(const ZrDebugProfile *profile,
                                                                   const TZrChar *name) {
    TZrSize index;

    if (profile == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0u; index < profile->entry_count; index++) {
        if (strcmp(profile->entries[index].name, name) == 0) {
            return &profile->entries[index];
        }
    }

    return ZR_NULL;
}

ZR_DEBUG_API TZrSize ZrDebug_Profile_GetSampleCount(const ZrDebugProfile *profile) {
    return profile != ZR_NULL ? profile->sample_count : 0u;
}

ZR_DEBUG_API TZrUInt64 ZrDebug_Profile_GetTotalSampleCount(const ZrDebugProfile *profile) {
    return profile != ZR_NULL ? profile->total_sample_count : 0u;
}

ZR_DEBUG_API const ZrDebugProfileSample *ZrDebug_Profile_GetSample(const ZrDebugProfile *profile, TZrSize index) {
    if (profile == ZR_NULL || index >= profile->sample_count) {
        return ZR_NULL;
    }
    return &profile->samples[index];
}
