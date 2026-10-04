#include <stdio.h>
#include "runtime_support.h"
#include "zr_vm_core/ownership.h"
#include <string.h>
#include <zr_vm_core/function.h>
#include <zr_vm_core/state.h>
#include <zr_vm_core/global.h>
#include <zr_vm_core/profile.h>
#include <zr_vm_core/stack.h>
#include <zr_vm_core/value.h>
#if defined(ZR_ARGUMENT_STAGING_TEST_CANDIDATE)
#include "function_argument_staging.h"
#endif

typedef struct Fixture {
    SZrTypeValueOnStack backing[40];
    SZrState state;
    SZrGlobalState global;
    SZrProfileRuntime profile;
    SZrFunction source, destination;
    SZrFunctionFrameSlotLayout sources[10], destinations[10];
    TZrStackValuePointer base;
    unsigned length;
} Fixture;

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, #condition); \
    ZrCore_Profile_SetCurrentState(ZR_NULL); return 1; } } while (0)

static void setup(Fixture *f, unsigned n, unsigned rotation) {
    memset(f, 0, sizeof(*f));
    f->base = f->backing + 1;
    f->length = n;
    f->state.global = &f->global;
    f->state.stackBase.valuePointer = f->backing;
    f->state.stackTail.valuePointer = f->backing + 40;
    for (unsigned i = 0; i < 40; ++i) ZrCore_Value_ResetAsNull(&f->backing[i].value);
    f->source.stackSize = f->destination.stackSize = n;
    f->source.parameterCount = f->destination.parameterCount = n;
    f->source.frameByteSize = f->destination.frameByteSize = 2u*n*sizeof(SZrTypeValueOnStack);
    f->source.frameByteAlign = f->destination.frameByteAlign = _Alignof(SZrTypeValueOnStack);
    f->source.frameSlotLayouts = f->sources;
    f->destination.frameSlotLayouts = f->destinations;
    f->source.frameSlotLayoutLength = f->destination.frameSlotLayoutLength = n;
    for (unsigned i = 0; i < n; ++i) {
        f->sources[i].stackSlot = i;
        f->sources[i].byteOffset = (n+i)*sizeof(SZrTypeValueOnStack);
        f->sources[i].byteSize = sizeof(SZrTypeValue);
        f->sources[i].byteAlign = _Alignof(SZrTypeValue);
        f->sources[i].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
        f->sources[i].isParameter = ZR_TRUE;
        f->destinations[i] = f->sources[i];
        f->destinations[i].byteOffset = (n+(i+rotation)%n)*sizeof(SZrTypeValueOnStack);
        ZrCore_Value_InitAsInt(&f->state, &f->base[n+i].value, 10*(i+1));
    }
    ZrCore_Function_FinalizeDirectFrameValueSlots(&f->source);
    ZrCore_Function_FinalizeDirectFrameValueSlots(&f->destination);
}

static int permutation(unsigned n, unsigned rotation) {
    Fixture f;
    setup(&f, n, rotation);
    for (unsigned i=0; i<n; ++i) {
        SZrStackFramePlace place;
        CHECK(ZrCore_Function_MakeFrameSlotPlace(&f.state,&f.destination,f.base,i,&place));
        CHECK(place.address == &f.base[n+(i+rotation)%n].value);
    }
    CHECK(ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
            f.base,&f.source,f.base,0,n));
    for (unsigned i=0; i<n; ++i) {
        CHECK(f.base[n+(i+rotation)%n].value.type == ZR_VALUE_TYPE_INT64);
        CHECK(f.base[n+(i+rotation)%n].value.value.nativeObject.nativeInt64 == 10*(i+1));
        CHECK(f.base[i].value.value.nativeObject.nativeInt64 == 10*(i+1));
    }
    return 0;
}

static int fallback_sources(void) {
    Fixture f;
    setup(&f,1,0);
    ZrCore_Value_ResetAsNull(&f.base[1].value);
    ZrCore_Value_InitAsInt(&f.state,&f.base[0].value,73);
    CHECK(ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
            f.base,&f.source,f.base,0,1));
    CHECK(f.base[1].value.value.nativeObject.nativeInt64 == 73);
    setup(&f,1,0);
    f.source.frameSlotLayouts=ZR_NULL;
    f.source.frameSlotLayoutLength=0;
    f.source.frameByteSize=0; /* actual dense-only function storage contract */
    ZrCore_Value_InitAsInt(&f.state,&f.base[0].value,91);
    CHECK(ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
            f.base,&f.source,f.base,0,1));
    CHECK(f.base[1].value.value.nativeObject.nativeInt64 == 91);
    return 0;
}

#if defined(ZR_ARGUMENT_STAGING_TEST_CANDIDATE)
static int workspace_and_failure(void) {
    Fixture f;
    SZrFunctionArgumentStage workspace[10];
    SZrTypeValueOnStack before[40];
    SZrExecutionArgumentStagingDiagnostic diagnostic;
    TZrSize size;
    setup(&f,3,1);
    CHECK(ZrCore_Function_ValueArgumentWorkspaceSize(&f.destination,&size));
    CHECK(size == 3*sizeof(workspace[0]));
    memcpy(before,f.backing,sizeof(before));
    CHECK(ZrCore_Function_StageValueFrameParametersWithWorkspace(&f.state,&f.destination,
            f.base,&f.source,f.base,0,3,workspace,size-1,&diagnostic) == ZR_EXECUTION_TRANSFER_SCRATCH_TOO_SMALL);
    CHECK(memcmp(before,f.backing,sizeof(before)) == 0);
    CHECK(diagnostic.status == ZR_EXECUTION_TRANSFER_SCRATCH_TOO_SMALL);
    CHECK(ZrCore_Function_StageValueFrameParametersWithWorkspace(&f.state,&f.destination,
            f.base,&f.source,f.base,0,3,workspace,size,&diagnostic) == ZR_EXECUTION_TRANSFER_OK);
    setup(&f,3,1);
    f.destinations[2].byteOffset += 1;
    ZrCore_Function_FinalizeDirectFrameValueSlots(&f.destination);
    memcpy(before,f.backing,sizeof(before));
    CHECK(!ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
            f.base,&f.source,f.base,0,3));
    CHECK(memcmp(before,f.backing,sizeof(before)) == 0);
    CHECK(!ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
            f.base,&f.source,f.base,0,3));
    CHECK(memcmp(before,f.backing,sizeof(before)) == 0);
    return 0;
}

static int conflicting_and_repeated_sources(void) {
    Fixture f;
    SZrTypeValueOnStack before[40];
    SZrFunctionArgumentStage workspace[10];
    SZrExecutionArgumentStagingDiagnostic diagnostic;
    setup(&f,2,0);
    f.destinations[1].byteOffset=f.destinations[0].byteOffset;
    ZrCore_Function_FinalizeDirectFrameValueSlots(&f.destination);
    memcpy(before,f.backing,sizeof(before));
    CHECK(ZrCore_Function_StageValueFrameParametersWithWorkspace(&f.state,&f.destination,
            f.base,&f.source,f.base,0,2,workspace,sizeof(workspace),&diagnostic) == ZR_EXECUTION_TRANSFER_ALIAS_CONFLICT);
    CHECK(diagnostic.parameterIndex == 1 && diagnostic.sourceStackSlot == 1 && diagnostic.relatedParameterIndex == 0);
    CHECK(memcmp(before,f.backing,sizeof(before)) == 0);
    /* Two distinct logical source keys have the same physical occupant. */
    f.sources[1].byteOffset=f.sources[0].byteOffset;
    ZrCore_Function_FinalizeDirectFrameValueSlots(&f.source);
    CHECK(ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
            f.base,&f.source,f.base,0,2));
    CHECK(f.base[0].value.value.nativeObject.nativeInt64 == 10);
    CHECK(f.base[1].value.value.nativeObject.nativeInt64 == 10);
    setup(&f,2,0);
    f.sources[1].stackSlot=0;
    ZrCore_Function_FinalizeDirectFrameValueSlots(&f.source);
    memcpy(before,f.backing,sizeof(before));
    CHECK(!ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
            f.base,&f.source,f.base,0,2));
    CHECK(memcmp(before,f.backing,sizeof(before)) == 0);
    return 0;
}

static int profiles(void) {
    Fixture f;
    setup(&f,2,1);
    f.global.profileRuntime=&f.profile;
    f.profile.recordHelpers=ZR_TRUE;
    f.profile.recordMemory=ZR_TRUE;
    ZrCore_Profile_SetCurrentState(&f.state);
    CHECK(ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
            f.base,&f.source,f.base,0,2));
    CHECK(f.profile.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_LAYOUT_VISIT] == 2);
    CHECK(f.profile.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_CHECKED] == 2);
    CHECK(f.profile.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_DIRECT] == 0);
    CHECK(f.profile.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY] == 4);
    CHECK(f.profile.helperCounts[ZR_PROFILE_HELPER_VALUE_RESET_NULL] == 4);
    CHECK(f.profile.helperCounts[ZR_PROFILE_HELPER_VALUE_CONSTRUCT] == 4);
    CHECK(f.profile.helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE] == 4);
    CHECK(f.profile.memoryMetricCounts[ZR_PROFILE_MEMORY_VALUE_COPY_BYTES] == 4*sizeof(SZrTypeValue));
    CHECK(ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
            f.base,&f.source,f.base,UINT32_MAX,0));
    CHECK(f.profile.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_EMPTY] == 1);
    ZrCore_Profile_SetCurrentState(ZR_NULL);
    setup(&f,1,0);
    f.global.profileRuntime=&f.profile;
    ZrCore_Profile_SetCurrentState(&f.state);
    CHECK(ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
            f.base,&f.source,f.base,0,1));
    for(unsigned i=0;i<sizeof(f.profile.helperCounts)/sizeof(f.profile.helperCounts[0]);++i) CHECK(f.profile.helperCounts[i] == 0);
    ZrCore_Profile_SetCurrentState(ZR_NULL);
    return 0;
}

static int output_aliases_and_flags(void) {
    Fixture f;
    SZrFunctionArgumentStage workspace[10];
    SZrTypeValueOnStack before[40];
    SZrProfileRuntime unrelated={0};
    SZrGlobalState otherGlobal={0};
    SZrState otherState={0};
    const TZrUInt16 flags[]={ZR_FUNCTION_FRAME_SLOT_FLAG_INLINE_RECEIVER_ARGUMENT,
        ZR_FUNCTION_FRAME_SLOT_FLAG_CONSTRUCTOR_INITIALIZATION_BITMAP,0x8000u};
    setup(&f,1,0);
    memcpy(before,f.backing,sizeof(before));
    CHECK(ZrCore_Function_StageValueFrameParametersWithWorkspace(&f.state,&f.destination,
            f.base,&f.source,f.base,0,1,workspace,sizeof(workspace),
            (SZrExecutionArgumentStagingDiagnostic *)&f.base[1].value) == ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT);
    CHECK(memcmp(before,f.backing,sizeof(before)) == 0);
    otherGlobal.profileRuntime=&unrelated;otherState.global=&otherGlobal;
    unrelated.recordHelpers=ZR_TRUE;
    ZrCore_Profile_SetCurrentState(&otherState);
    CHECK(ZrCore_Function_StageValueFrameParametersWithWorkspace(&f.state,&f.destination,
            f.base,&f.source,f.base,0,1,&unrelated,sizeof(unrelated),ZR_NULL) == ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT);
    CHECK(memcmp(before,f.backing,sizeof(before)) == 0);
    for(unsigned i=0;i<sizeof(unrelated.helperCounts)/sizeof(unrelated.helperCounts[0]);++i) CHECK(unrelated.helperCounts[i] == 0);
    ZrCore_Profile_SetCurrentState(ZR_NULL);
    for(unsigned i=0;i<sizeof(flags)/sizeof(flags[0]);++i) {
        setup(&f,1,0);f.destinations[0].reserved0|=flags[i];
        memcpy(before,f.backing,sizeof(before));
        CHECK(ZrCore_Function_StageValueFrameParametersWithWorkspace(&f.state,&f.destination,
                f.base,&f.source,f.base,0,1,workspace,sizeof(workspace),ZR_NULL) == ZR_EXECUTION_TRANSFER_UNSUPPORTED);
        CHECK(memcmp(before,f.backing,sizeof(before)) == 0);
    }
    setup(&f,1,0);
    f.base[1].value.isNative=ZR_FALSE;
    memcpy(before,f.backing,sizeof(before));
    CHECK(ZrCore_Function_StageValueFrameParametersWithWorkspace(&f.state,&f.destination,
            f.base,&f.source,f.base,0,1,workspace,sizeof(workspace),ZR_NULL) == ZR_EXECUTION_TRANSFER_UNSUPPORTED);
    CHECK(memcmp(before,f.backing,sizeof(before)) == 0);
    return 0;
}

static int empty_selection(void) {
    Fixture f;
    SZrTypeValueOnStack before[40];
    setup(&f,2,0);
    for(unsigned i=0;i<2;++i) f.destinations[i].isParameter=ZR_FALSE;
    ZrCore_Function_FinalizeDirectFrameValueSlots(&f.destination);
    f.state.stackBase.valuePointer=ZR_NULL;f.state.stackTail.valuePointer=ZR_NULL;
    memcpy(before,f.backing,sizeof(before));
    CHECK(ZrCore_Function_StageValueFrameParametersWithWorkspace(&f.state,&f.destination,
            f.base,&f.source,f.base,UINT32_MAX,7,ZR_NULL,0,ZR_NULL) == ZR_EXECUTION_TRANSFER_OK);
    CHECK(ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
            f.base,&f.source,f.base,UINT32_MAX,7));
    CHECK(memcmp(before,f.backing,sizeof(before)) == 0);
    setup(&f,2,0);
    f.destinations[0].slotKind=ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    f.destinations[1].byteSize=1;
    ZrCore_Function_FinalizeDirectFrameValueSlots(&f.destination);
    memcpy(before,f.backing,sizeof(before));
    CHECK(ZrCore_Function_StageValueFrameParametersWithWorkspace(&f.state,&f.destination,
            f.base,&f.source,f.base,0,2,ZR_NULL,0,ZR_NULL) == ZR_EXECUTION_TRANSFER_OK);
    CHECK(memcmp(before,f.backing,sizeof(before)) == 0);
    return 0;
}

static int scalar_bits(void) {
    Fixture f;
    SZrTypeValue expected;
    for(unsigned i=0;i<5;++i) {
        setup(&f,1,0);
        switch(i) {
            case 0: ZrCore_Value_ResetAsNull(&f.base[1].value);break;
            case 1: ZrCore_Value_InitAsBool(&f.state,&f.base[1].value,ZR_TRUE);break;
            case 2: ZrCore_Value_InitAsInt(&f.state,&f.base[1].value,INT64_MIN);break;
            case 3: ZrCore_Value_InitAsUInt(&f.state,&f.base[1].value,UINT64_MAX);break;
            case 4: ZrCore_Value_InitAsFloat(&f.state,&f.base[1].value,-0.0);break;
        }
        memcpy(&expected,&f.base[1].value,sizeof(expected));
        CHECK(ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
                f.base,&f.source,f.base,0,1));
        CHECK(memcmp(&expected,&f.base[1].value,sizeof(expected)) == 0);
        CHECK(memcmp(&expected,&f.base[0].value,sizeof(expected)) == 0);
    }
    return 0;
}
#endif

#if defined(ZR_ARGUMENT_STAGING_TEST_CANDIDATE)
static int ignored_ordinals(void) {
    Fixture f;
    SZrTypeValue before;
    setup(&f,2,0);
    f.destinations[1].byteOffset=UINT32_MAX;
    ZrCore_Function_FinalizeDirectFrameValueSlots(&f.destination);
    before=f.base[3].value;
    CHECK(ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
            f.base,&f.source,f.base,0,1));
    CHECK(f.base[2].value.value.nativeObject.nativeInt64 == 10);
    CHECK(memcmp(&before,&f.base[3].value,sizeof(before)) == 0);
    setup(&f,2,0);
    f.destinations[0].slotKind=ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    f.destinations[1].byteOffset=UINT32_MAX;
    ZrCore_Function_FinalizeDirectFrameValueSlots(&f.destination);
    {
        SZrTypeValueOnStack snapshot[40];
        memcpy(snapshot,f.backing,sizeof(snapshot));
        f.state.stackBase.valuePointer=ZR_NULL;f.state.stackTail.valuePointer=ZR_NULL;
        CHECK(ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
                f.base,&f.source,f.base,UINT32_MAX,2));
        CHECK(memcmp(snapshot,f.backing,sizeof(snapshot)) == 0);
    }
    return 0;
}

static int partial_mirror(void) {
    Fixture f;
    SZrTypeValueOnStack expected[40];
    SZrTypeValue value;
    SZrStackFramePlace place;
    setup(&f,1,0);
    f.destinations[0].byteOffset=_Alignof(SZrTypeValue);
    ZrCore_Function_FinalizeDirectFrameValueSlots(&f.destination);
    CHECK(ZrCore_Function_MakeFrameSlotPlace(&f.state,&f.destination,f.base,0,&place));
    CHECK(place.address != &f.base[0].value);
    CHECK(ZrCore_Value_SlotsOverlapNoProfile(place.address,&f.base[0].value));
    ZrCore_Value_ResetAsNull(place.address);
    ZrCore_Value_InitAsInt(&f.state,&f.base[11].value,811);
    value=f.base[11].value;
    memcpy(expected,f.backing,sizeof(expected));
    memcpy((unsigned char *)expected+((unsigned char *)place.address-(unsigned char *)f.backing),&value,sizeof(value));
    f.global.profileRuntime=&f.profile;f.profile.recordHelpers=ZR_TRUE;
    ZrCore_Profile_SetCurrentState(&f.state);
    CHECK(ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
            f.base,&f.source,f.base+10,0,1));
    CHECK(memcmp(expected,f.backing,sizeof(expected)) == 0);
    CHECK(f.profile.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY] == 1);
    CHECK(f.profile.helperCounts[ZR_PROFILE_HELPER_VALUE_RESET_NULL] == 1);
    ZrCore_Profile_SetCurrentState(ZR_NULL);
    return 0;
}

static int distinct_float(void) {
    Fixture f;
    SZrTypeValue before;
    setup(&f,1,0);
    ZrCore_Value_InitAsFloat(&f.state,&f.base[1].value,-0.0);
    f.base[1].value.type=ZR_VALUE_TYPE_FLOAT;
    before=f.base[1].value;
    CHECK(ZrCore_Function_CopyValueFrameParametersFromFrame(&f.state,&f.destination,
            f.base,&f.source,f.base,0,1));
    CHECK(f.base[1].value.type == ZR_VALUE_TYPE_FLOAT);
    CHECK(memcmp(&before,&f.base[1].value,sizeof(before)) == 0);
    CHECK(memcmp(&before,&f.base[0].value,sizeof(before)) == 0);
    return 0;
}

static int actual_owning_legacy(void) {
    Fixture f;
    SZrState *state=ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *object;
    TZrStackValuePointer frame;
    SZrProfileRuntime profile={0};
    SZrProfileRuntime *old;
    CHECK(state != ZR_NULL);
    object=ZrCore_Function_New(state);
    CHECK(object != ZR_NULL);
    frame=ZrCore_Function_CheckStackAndGc(state,16,state->stackTop.valuePointer);
    CHECK(frame != ZR_NULL);
    for(unsigned i=0;i<8;++i) ZrCore_Value_ResetAsNull(&frame[i].value);
    setup(&f,1,0);
    ZrCore_Value_InitAsRawObject(state,&frame[0].value,ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    CHECK(ZrCore_Ownership_SharePlainValue(state,&frame[1].value,&frame[0].value));
    CHECK(frame[1].value.ownershipKind == ZR_OWNERSHIP_VALUE_KIND_SHARED);
    old=state->global->profileRuntime;state->global->profileRuntime=&profile;
    profile.recordHelpers=ZR_TRUE;profile.recordMemory=ZR_TRUE;
    ZrCore_Profile_SetCurrentState(state);
    CHECK(ZrCore_Function_CopyValueFrameParametersFromFrame(state,&f.destination,
            frame+3,&f.source,frame,0,1));
    CHECK(frame[3].value.ownershipKind == ZR_OWNERSHIP_VALUE_KIND_SHARED);
    CHECK(frame[4].value.ownershipKind == ZR_OWNERSHIP_VALUE_KIND_SHARED);
    CHECK(frame[3].value.ownershipControl == frame[1].value.ownershipControl);
    CHECK(frame[4].value.ownershipControl == frame[1].value.ownershipControl);
    CHECK(ZrCore_Ownership_GetStrongRefCount(ZR_CAST_RAW_OBJECT_AS_SUPER(object)) == 3);
    CHECK(profile.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_LAYOUT_VISIT] == 1);
    CHECK(profile.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_DIRECT] == 1);
    CHECK(profile.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_CHECKED] == 0);
    CHECK(profile.helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE] == 2);
    CHECK(profile.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY] == 2);
    state->global->profileRuntime=old;ZrCore_Profile_SetCurrentState(state);
    ZrCore_Ownership_ReleaseValue(state,&frame[3].value);
    ZrCore_Ownership_ReleaseValue(state,&frame[4].value);
    ZrCore_Ownership_ReleaseValue(state,&frame[1].value);
    ZrCore_Value_ResetAsNull(&frame[0].value);
    ZrTests_Runtime_State_Destroy(state);
    ZrCore_Profile_SetCurrentState(ZR_NULL);
    return 0;
}

#endif

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    int failures=0;
    failures+=permutation(2,1);
    failures+=permutation(3,1);
    failures+=permutation(1,0);
    failures+=fallback_sources();
#if defined(ZR_ARGUMENT_STAGING_TEST_CANDIDATE)
    failures+=workspace_and_failure();
    failures+=conflicting_and_repeated_sources();
    failures+=profiles();
    failures+=output_aliases_and_flags();
    failures+=empty_selection();
    failures+=scalar_bits();
    failures+=ignored_ordinals();
    failures+=partial_mirror();
    failures+=distinct_float();
    failures+=actual_owning_legacy();
    failures+=permutation(10,1); /* uses the CRT workspace branch */
#endif
    printf("argument staging matrix failures=%d\n",failures);
    return failures ? 1 : 0;
}
