//
// Runtime profiling support for benchmark instrumentation.
//

#include "zr_vm_core/profile.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"

ZR_PROFILE_THREAD_LOCAL SZrProfileRuntime *g_zr_profile_current = ZR_NULL;

static const TZrChar *const CZrProfileHelperNames[ZR_PROFILE_HELPER_ENUM_MAX] = {
        "value_copy",
        "value_reset_null",
        "stack_get_value",
        "precall",
        "get_member",
        "set_member",
        "get_by_index",
        "set_by_index"
};

static const TZrChar *const CZrProfileSlowPathNames[ZR_PROFILE_SLOWPATH_ENUM_MAX] = {
        "meta_fallback",
        "callsite_cache_lookup",
        "callsite_cache_miss",
        "protect_e",
        "protect_eh",
        "protect_esh",
        "meta_call_prepare"
};

static const TZrChar *const CZrProfileQuickeningProbeNames[ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX] = {
        "get_stack_typed_arithmetic",
        "get_constant_typed_arithmetic"
};

#define ZR_PROFILE_INSTRUCTION_NAME_DECLARE(OPCODE) #OPCODE,
static const TZrChar *const CZrProfileInstructionNames[ZR_INSTRUCTION_ENUM(ENUM_MAX)] = {
        ZR_INSTRUCTION_DECLARE(ZR_PROFILE_INSTRUCTION_NAME_DECLARE)
};
#undef ZR_PROFILE_INSTRUCTION_NAME_DECLARE

static TZrBool profile_env_enabled(const TZrChar *value) {
    if (value == ZR_NULL || value[0] == '\0') {
        return ZR_FALSE;
    }

    return !(strcmp(value, "0") == 0 || strcmp(value, "false") == 0 || strcmp(value, "FALSE") == 0);
}

static TZrChar *profile_dup_env(const TZrChar *name) {
    const TZrChar *value = getenv(name);
    TZrSize length;
    TZrChar *copy;

    if (value == ZR_NULL || value[0] == '\0') {
        return ZR_NULL;
    }

    length = strlen(value);
    copy = (TZrChar *)malloc(length + 1u);
    if (copy == ZR_NULL) {
        return ZR_NULL;
    }
    memcpy(copy, value, length + 1u);
    return copy;
}

static void profile_write_counts(FILE *file,
                                 const TZrChar *sectionName,
                                 TZrUInt64 count,
                                 const TZrChar *(*nameGetter)(TZrUInt32),
                                 const TZrUInt64 *values) {
    TZrUInt32 index;
    TZrBool emitted = ZR_FALSE;

    fprintf(file, "  \"%s\": [\n", sectionName);
    for (index = 0; index < count; index++) {
        if (values[index] == 0u) {
            continue;
        }
        if (emitted) {
            fputs(",\n", file);
        }
        fprintf(file,
                "    {\"name\": \"%s\", \"count\": %" PRIu64 "}",
                nameGetter(index),
                values[index]);
        emitted = ZR_TRUE;
    }
    if (emitted) {
        fputc('\n', file);
    }
    fputs("  ]", file);
}

static const TZrChar *profile_helper_name_getter(TZrUInt32 index) {
    return ZrCore_Profile_HelperKindName((EZrProfileHelperKind)index);
}

static const TZrChar *profile_slow_path_name_getter(TZrUInt32 index) {
    return ZrCore_Profile_SlowPathKindName((EZrProfileSlowPathKind)index);
}

static const TZrChar *profile_quickening_probe_name_getter(TZrUInt32 index) {
    return ZrCore_Profile_QuickeningProbeKindName((EZrProfileQuickeningProbeKind)index);
}

static const TZrChar *profile_instruction_name_getter(TZrUInt32 index) {
    return ZrCore_Profile_InstructionName((EZrInstructionCode)index);
}

static void profile_write_report(const SZrProfileRuntime *runtime) {
    FILE *file;

    if (runtime == ZR_NULL || !runtime->hasOutputPath || runtime->outputPath == ZR_NULL) {
        return;
    }

    file = fopen(runtime->outputPath, "wb");
    if (file == ZR_NULL) {
        return;
    }

    fputs("{\n", file);
    fprintf(file,
            "  \"case\": \"%s\",\n",
            runtime->hasCaseName && runtime->caseName != ZR_NULL ? runtime->caseName : "");
    fprintf(file,
            "  \"mode\": \"%s\",\n",
            runtime->hasModeName && runtime->modeName != ZR_NULL ? runtime->modeName : "");
    fprintf(file, "  \"record_instructions\": %s,\n", runtime->recordInstructions ? "true" : "false");
    fprintf(file, "  \"record_slowpaths\": %s,\n", runtime->recordSlowPaths ? "true" : "false");
    fprintf(file, "  \"record_helpers\": %s,\n", runtime->recordHelpers ? "true" : "false");
    profile_write_counts(file,
                         "instructions",
                         ZR_INSTRUCTION_ENUM(ENUM_MAX),
                         profile_instruction_name_getter,
                         runtime->instructionCounts);
    fputs(",\n", file);
    profile_write_counts(file,
                         "helpers",
                         ZR_PROFILE_HELPER_ENUM_MAX,
                         profile_helper_name_getter,
                         runtime->helperCounts);
    fputs(",\n", file);
    profile_write_counts(file,
                         "slowpaths",
                         ZR_PROFILE_SLOWPATH_ENUM_MAX,
                         profile_slow_path_name_getter,
                         runtime->slowPathCounts);
    fputs(",\n", file);
    profile_write_counts(file,
                         "quickening_probes",
                         ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX,
                         profile_quickening_probe_name_getter,
                         runtime->quickeningProbeCounts);
    fputs("\n}\n", file);
    fclose(file);
}

void ZrCore_Profile_GlobalInit(SZrGlobalState *global) {
    SZrProfileRuntime *runtime;
    const TZrChar *recordInstructions;
    const TZrChar *recordSlowPaths;
    const TZrChar *recordHelpers;

    if (global == ZR_NULL) {
        return;
    }

    global->profileRuntime = ZR_NULL;
    recordInstructions = getenv("ZR_VM_PROFILE_INSTRUCTIONS");
    recordSlowPaths = getenv("ZR_VM_PROFILE_SLOWPATHS");
    recordHelpers = getenv("ZR_VM_PROFILE_HELPERS");
    if (!profile_env_enabled(recordInstructions) &&
        !profile_env_enabled(recordSlowPaths) &&
        !profile_env_enabled(recordHelpers)) {
        return;
    }

    runtime = (SZrProfileRuntime *)calloc(1u, sizeof(*runtime));
    if (runtime == ZR_NULL) {
        return;
    }

    runtime->recordInstructions = profile_env_enabled(recordInstructions);
    runtime->recordSlowPaths = profile_env_enabled(recordSlowPaths);
    runtime->recordHelpers = profile_env_enabled(recordHelpers);
    runtime->outputPath = profile_dup_env("ZR_VM_PROFILE_OUT");
    runtime->caseName = profile_dup_env("ZR_VM_PROFILE_CASE");
    runtime->modeName = profile_dup_env("ZR_VM_PROFILE_MODE");
    runtime->hasOutputPath = (TZrBool)(runtime->outputPath != ZR_NULL);
    runtime->hasCaseName = (TZrBool)(runtime->caseName != ZR_NULL);
    runtime->hasModeName = (TZrBool)(runtime->modeName != ZR_NULL);
    global->profileRuntime = runtime;
}

void ZrCore_Profile_GlobalShutdown(SZrGlobalState *global) {
    SZrProfileRuntime *runtime;

    if (global == ZR_NULL || global->profileRuntime == ZR_NULL) {
        return;
    }

    runtime = global->profileRuntime;
    if (g_zr_profile_current == runtime) {
        g_zr_profile_current = ZR_NULL;
    }

    profile_write_report(runtime);
    free(runtime->outputPath);
    free(runtime->caseName);
    free(runtime->modeName);
    free(runtime);
    global->profileRuntime = ZR_NULL;
}

void ZrCore_Profile_SetCurrentState(SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        g_zr_profile_current = ZR_NULL;
        return;
    }

    g_zr_profile_current = state->global->profileRuntime;
}

SZrProfileRuntime *ZrCore_Profile_Current(void) { return g_zr_profile_current; }

SZrProfileRuntime *ZrCore_Profile_FromState(SZrState *state) {
    return (state != ZR_NULL && state->global != ZR_NULL) ? state->global->profileRuntime : ZR_NULL;
}

const TZrChar *ZrCore_Profile_HelperKindName(EZrProfileHelperKind kind) {
    return (kind >= 0 && kind < ZR_PROFILE_HELPER_ENUM_MAX) ? CZrProfileHelperNames[kind] : "unknown";
}

const TZrChar *ZrCore_Profile_SlowPathKindName(EZrProfileSlowPathKind kind) {
    return (kind >= 0 && kind < ZR_PROFILE_SLOWPATH_ENUM_MAX) ? CZrProfileSlowPathNames[kind] : "unknown";
}

const TZrChar *ZrCore_Profile_QuickeningProbeKindName(EZrProfileQuickeningProbeKind kind) {
    return (kind >= 0 && kind < ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX) ? CZrProfileQuickeningProbeNames[kind]
                                                                      : "unknown";
}

const TZrChar *ZrCore_Profile_InstructionName(EZrInstructionCode opcode) {
    return (opcode >= 0 && opcode < ZR_INSTRUCTION_ENUM(ENUM_MAX)) ? CZrProfileInstructionNames[opcode] : "unknown";
}
