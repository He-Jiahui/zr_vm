//
// Runtime profiling for benchmark/report generation.
//

#ifndef ZR_VM_CORE_PROFILE_H
#define ZR_VM_CORE_PROFILE_H

#include "zr_vm_core/conf.h"
#include "zr_vm_common/zr_instruction_conf.h"

struct SZrGlobalState;
struct SZrState;

#if defined(_MSC_VER)
    #define ZR_PROFILE_THREAD_LOCAL __declspec(thread)
#else
    #define ZR_PROFILE_THREAD_LOCAL _Thread_local
#endif

typedef enum EZrProfileHelperKind {
    ZR_PROFILE_HELPER_VALUE_COPY = 0,
    ZR_PROFILE_HELPER_VALUE_RESET_NULL,
    ZR_PROFILE_HELPER_STACK_GET_VALUE,
    ZR_PROFILE_HELPER_PRECALL,
    ZR_PROFILE_HELPER_GET_MEMBER,
    ZR_PROFILE_HELPER_SET_MEMBER,
    ZR_PROFILE_HELPER_GET_BY_INDEX,
    ZR_PROFILE_HELPER_SET_BY_INDEX,
    ZR_PROFILE_HELPER_ENUM_MAX
} EZrProfileHelperKind;

typedef enum EZrProfileSlowPathKind {
    ZR_PROFILE_SLOWPATH_META_FALLBACK = 0,
    ZR_PROFILE_SLOWPATH_CALLSITE_CACHE_LOOKUP,
    ZR_PROFILE_SLOWPATH_CALLSITE_CACHE_MISS,
    ZR_PROFILE_SLOWPATH_PROTECT_E,
    ZR_PROFILE_SLOWPATH_PROTECT_EH,
    ZR_PROFILE_SLOWPATH_PROTECT_ESH,
    ZR_PROFILE_SLOWPATH_META_CALL_PREPARE,
    ZR_PROFILE_SLOWPATH_ENUM_MAX
} EZrProfileSlowPathKind;

typedef struct SZrProfileRuntime {
    TZrBool recordInstructions;
    TZrBool recordSlowPaths;
    TZrBool recordHelpers;
    TZrBool hasOutputPath;
    TZrBool hasCaseName;
    TZrBool hasModeName;
    TZrUInt64 instructionCounts[ZR_INSTRUCTION_ENUM(ENUM_MAX)];
    TZrUInt64 helperCounts[ZR_PROFILE_HELPER_ENUM_MAX];
    TZrUInt64 slowPathCounts[ZR_PROFILE_SLOWPATH_ENUM_MAX];
    TZrChar *outputPath;
    TZrChar *caseName;
    TZrChar *modeName;
} SZrProfileRuntime;

#if defined(_MSC_VER)
extern ZR_PROFILE_THREAD_LOCAL SZrProfileRuntime *g_zr_profile_current;
#else
ZR_CORE_API ZR_PROFILE_THREAD_LOCAL SZrProfileRuntime *g_zr_profile_current;
#endif

ZR_CORE_API void ZrCore_Profile_GlobalInit(struct SZrGlobalState *global);
ZR_CORE_API void ZrCore_Profile_GlobalShutdown(struct SZrGlobalState *global);
ZR_CORE_API void ZrCore_Profile_SetCurrentState(struct SZrState *state);
ZR_CORE_API SZrProfileRuntime *ZrCore_Profile_Current(void);
ZR_CORE_API SZrProfileRuntime *ZrCore_Profile_FromState(struct SZrState *state);
ZR_CORE_API const TZrChar *ZrCore_Profile_HelperKindName(EZrProfileHelperKind kind);
ZR_CORE_API const TZrChar *ZrCore_Profile_SlowPathKindName(EZrProfileSlowPathKind kind);
ZR_CORE_API const TZrChar *ZrCore_Profile_InstructionName(EZrInstructionCode opcode);

static ZR_FORCE_INLINE void ZrCore_Profile_RecordHelperCurrent(EZrProfileHelperKind kind) {
#if defined(_MSC_VER)
    SZrProfileRuntime *runtime = ZrCore_Profile_Current();
#else
    SZrProfileRuntime *runtime = g_zr_profile_current;
#endif
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordHelpers)) {
        runtime->helperCounts[kind]++;
    }
}

static ZR_FORCE_INLINE void ZrCore_Profile_RecordHelperFromState(struct SZrState *state, EZrProfileHelperKind kind) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordHelpers)) {
        runtime->helperCounts[kind]++;
    }
}

static ZR_FORCE_INLINE void ZrCore_Profile_RecordSlowPathCurrent(EZrProfileSlowPathKind kind) {
#if defined(_MSC_VER)
    SZrProfileRuntime *runtime = ZrCore_Profile_Current();
#else
    SZrProfileRuntime *runtime = g_zr_profile_current;
#endif
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordSlowPaths)) {
        runtime->slowPathCounts[kind]++;
    }
}

static ZR_FORCE_INLINE void ZrCore_Profile_RecordInstructionFromState(struct SZrState *state, EZrInstructionCode opcode) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordInstructions)) {
        runtime->instructionCounts[opcode]++;
    }
}

static ZR_FORCE_INLINE void ZrCore_Profile_RecordSlowPathFromState(struct SZrState *state,
                                                                   EZrProfileSlowPathKind kind) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordSlowPaths)) {
        runtime->slowPathCounts[kind]++;
    }
}

#endif
