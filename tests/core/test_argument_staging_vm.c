#include <stdio.h>
#include <string.h>
#include "runtime_support.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/stack.h"

#define REQUIRE(condition) do { if (!(condition)) { \
    fprintf(stderr,"FAIL actual VM consumer:%d: %s\n",__LINE__,#condition); \
    ZrCore_Profile_SetCurrentState(ZR_NULL); return 1; } } while(0)

static int layout(SZrState *state, SZrFunction *function, unsigned n, unsigned parameters) {
    SZrFunctionFrameSlotLayout *slots = ZrCore_Memory_RawMallocWithType(state->global,
            n*sizeof(*slots),ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if(slots == ZR_NULL) return 0;
    memset(slots,0,n*sizeof(*slots));
    function->stackSize=n;function->parameterCount=parameters;
    function->frameByteSize=2u*n*sizeof(SZrTypeValueOnStack);
    function->frameByteAlign=_Alignof(SZrTypeValueOnStack);
    function->frameSlotLayouts=slots;function->frameSlotLayoutLength=n;
    for(unsigned i=0;i<n;++i) {
        slots[i].stackSlot=i;
        slots[i].byteOffset=(n+i)*sizeof(SZrTypeValueOnStack);
        slots[i].byteSize=sizeof(SZrTypeValue);slots[i].byteAlign=_Alignof(SZrTypeValue);
        slots[i].slotKind=ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
        slots[i].isParameter=(TZrBool)(i<parameters);
    }
    ZrCore_Function_FinalizeDirectFrameValueSlots(function);
    return 1;
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    SZrState *state=ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *caller,*callee;
    SZrCallInfo *previous,*call;
    SZrProfileRuntime profile={0};
    SZrProfileRuntime *oldProfile;
    TZrStackValuePointer callerFunctionBase,callerFrameBase,callBase;
    SZrStackFramePlace sourcePlace,destinationPlace;
    REQUIRE(state != ZR_NULL);
    caller=ZrCore_Function_New(state);callee=ZrCore_Function_New(state);
    REQUIRE(caller != ZR_NULL && callee != ZR_NULL);
    REQUIRE(layout(state,caller,6,0) && layout(state,callee,2,1));
    callerFunctionBase=ZrCore_Function_CheckStackAndGc(state,40,state->stackTop.valuePointer);
    REQUIRE(callerFunctionBase != ZR_NULL);
    callerFrameBase=callerFunctionBase+1;
    callBase=callerFrameBase+2;
    for(unsigned i=0;i<20;++i) ZrCore_Value_ResetAsNull(&callerFunctionBase[i].value);
    ZrCore_Value_InitAsRawObject(state,&callerFunctionBase[0].value,ZR_CAST_RAW_OBJECT_AS_SUPER(caller));
    ZrCore_Value_InitAsRawObject(state,&callBase[0].value,ZR_CAST_RAW_OBJECT_AS_SUPER(callee));
    REQUIRE(ZrCore_Function_MakeFrameSlotPlace(state,caller,callerFrameBase,3,&sourcePlace));
    REQUIRE(ZrCore_Function_MakeFrameSlotPlace(state,callee,callBase+1,0,&destinationPlace));
    REQUIRE(sourcePlace.address != destinationPlace.address);
    ZrCore_Value_InitAsInt(state,sourcePlace.address,137);
    ZrCore_Value_ResetAsNull(&callerFrameBase[3].value);
    previous=&state->baseCallInfo;
    previous->functionBase.valuePointer=callerFunctionBase;
    previous->functionTop.valuePointer=callerFrameBase+12;
    previous->metadataFunction=caller;
    previous->previous=ZR_NULL;
    previous->callStatus=ZR_CALL_STATUS_NONE;
    state->callInfoList=previous;
    state->stackTop.valuePointer=callBase+2;
    state->debugHookSignal=0;
    oldProfile=state->global->profileRuntime;
    state->global->profileRuntime=&profile;
    profile.recordHelpers=ZR_TRUE;
    ZrCore_Profile_SetCurrentState(state);
    call=ZrCore_Function_PreCallKnownVmValueWithArgumentSource(state,callBase,
            &callBase[0].value,1,ZR_NULL,callerFrameBase,3);
    REQUIRE(call != ZR_NULL && call->previous == previous);
    REQUIRE(call->hasArgumentSourceFrame && call->argumentSourceStartSlot == 3);
    REQUIRE(profile.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_DIRECT] == 1);
    REQUIRE(profile.helperCounts[ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_CHECKED] == 0);
    REQUIRE(((SZrTypeValue *)destinationPlace.address)->type == ZR_VALUE_TYPE_INT64);
    REQUIRE(((SZrTypeValue *)destinationPlace.address)->value.nativeObject.nativeInt64 == 137);
    REQUIRE(call->functionBase.valuePointer[1].value.type == ZR_VALUE_TYPE_INT64);
    REQUIRE(call->functionBase.valuePointer[1].value.value.nativeObject.nativeInt64 == 137);
    state->global->profileRuntime=oldProfile;
    ZrCore_Profile_SetCurrentState(state);
    ZrTests_Runtime_State_Destroy(state);
    puts("PASS: actual allVALUE VM PreCall consumed packed-only scalar through FromFrame");
    return 0;
}
