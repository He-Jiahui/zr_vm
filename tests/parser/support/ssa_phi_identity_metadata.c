#include <stdlib.h>
#include <string.h>
#include "ssa_phi_identity_support.h"
#include "ssa_exec_ir_owned_module_shim.h"

int Pix_Metadata(SPixFixture *f,SZrExecIrFunction *fn,unsigned profile) {
    unsigned root=f->base.p,ret=fn->instructionCount,i;
    memset(f->plan+fn->id-1u,0,sizeof(*f->plan));
    for(i=0u;i<fn->blockCount;++i)f->plan[fn->id-1u].dom[i]=1u;
    if(profile==0u){
        fn->instructions[ret-1u].flags|=ZR_EXEC_IR_FLAG_DEBUG_POLL;
        return ZrParser_ExecIr_BuildStateMaps(fn,&f->base.diagnostic);
    }
    if(profile==1u||profile==2u){
        fn->gcRoots=calloc(1u,sizeof(*fn->gcRoots));if(!fn->gcRoots)return 0;
        fn->gcRootCount=fn->gcRootCapacity=1u;fn->gcRoots[0]=root;
    }
    if(profile==2u||profile==7u){
        fn->gcMap=calloc(1u,sizeof(*fn->gcMap));if(!fn->gcMap)return 0;
        fn->gcMapCount=fn->gcMapCapacity=1u;
        if(profile==7u)return 1;
        fn->gcMap->entries=calloc(1u,sizeof(*fn->gcMap->entries));if(!fn->gcMap->entries)return 0;
        fn->gcMap->entryCount=fn->gcMap->entryCapacity=1u;
        fn->gcMap->entries[0].site=ret;fn->gcMap->safepointId=ret;
        fn->gcMap->sourceId=fn->instructions[ret-1u].sourceId;
        fn->gcMap->rootRange=(SZrExecIrRange){.start=0u,.count=1u};
        fn->instructions[ret-1u].flags|=ZR_EXEC_IR_FLAG_DEBUG_POLL;
    }
    if(profile==3u||profile==4u||profile==6u){
        fn->deoptStates=calloc(1u,sizeof(*fn->deoptStates));if(!fn->deoptStates)return 0;
        fn->deoptStateCount=fn->deoptStateCapacity=1u;fn->deoptStates[0].id=1u;
        fn->deoptStates[0].sourceId=fn->instructions[ret-1u].sourceId;
        fn->deoptStates[0].resumeId=1u;fn->instructions[ret-1u].deoptId=1u;
        if(profile==3u){
            fn->deoptValues=calloc(1u,sizeof(*fn->deoptValues));if(!fn->deoptValues)return 0;
            fn->deoptValueCount=fn->deoptValueCapacity=1u;fn->deoptValues[0]=root;
            fn->deoptStates[0].valueRange.count=1u;
        }else if(profile==4u){
            fn->deoptAggregates=calloc(1u,sizeof(*fn->deoptAggregates));
            fn->deoptAggregateFields=calloc(1u,sizeof(*fn->deoptAggregateFields));
            if(!fn->deoptAggregates||!fn->deoptAggregateFields)return 0;
            fn->deoptAggregateCount=fn->deoptAggregateCapacity=1u;
            fn->deoptAggregateFieldCount=fn->deoptAggregateFieldCapacity=1u;
            fn->deoptStates[0].aggregates.count=1u;fn->deoptAggregates[0].identityId=1u;
            fn->deoptAggregates[0].typeToken=101u;fn->deoptAggregates[0].layoutId=201u;
            fn->deoptAggregates[0].fields.count=1u;
            fn->deoptAggregateFields[0].kind=ZR_EXEC_IR_DEOPT_FIELD_VALUE;
            fn->deoptAggregateFields[0].valueId=root;
        }
    }
    if(profile==5u){
        fn->frameLayout=calloc(1u,sizeof(*fn->frameLayout));if(!fn->frameLayout)return 0;
        fn->frameLayout->slots=calloc(1u,sizeof(*fn->frameLayout->slots));if(!fn->frameLayout->slots)return 0;
        fn->frameLayout->slotCount=fn->frameLayout->slotCapacity=1u;
        fn->frameLayout->storageSlotCount=fn->frameLayout->logicalSlotCount=1u;
        fn->frameLayout->localCount=1u;
        fn->frameLayout->frameByteSize=8u;fn->frameLayout->frameByteAlign=8u;
        fn->frameLayout->layoutHash=UINT64_C(0x50484946);
        fn->frameLayout->slots[0]=(SZrExecIrFrameSlot){.slotId=1u,.byteOffset=0u,.byteSize=8u,.byteAlign=8u,
            .typeToken=ZR_VALUE_TYPE_INT64,.kind=ZR_EXEC_IR_FRAME_SLOT_VALUE};
    }
    if(profile==8u){ /* Isolate the aggregate-field gate, with no state or aggregate. */
        fn->deoptAggregateFields=calloc(1u,sizeof(*fn->deoptAggregateFields));if(!fn->deoptAggregateFields)return 0;
        fn->deoptAggregateFieldCount=fn->deoptAggregateFieldCapacity=1u;
        fn->deoptAggregateFields[0].kind=ZR_EXEC_IR_DEOPT_FIELD_VALUE;fn->deoptAggregateFields[0].valueId=root;
    }
    if(profile==9u){ /* Isolate aggregate presence instead of relying on deoptStateCount. */
        fn->deoptAggregates=calloc(1u,sizeof(*fn->deoptAggregates));if(!fn->deoptAggregates)return 0;
        fn->deoptAggregateCount=fn->deoptAggregateCapacity=1u;
        fn->deoptAggregates[0].identityId=1u;fn->deoptAggregates[0].typeToken=101u;fn->deoptAggregates[0].layoutId=201u;
    }
    if(profile==10u){
        fn->deoptValues=calloc(1u,sizeof(*fn->deoptValues));if(!fn->deoptValues)return 0;
        fn->deoptValueCount=fn->deoptValueCapacity=1u;fn->deoptValues[0]=root;
    }
    return 1;
}
