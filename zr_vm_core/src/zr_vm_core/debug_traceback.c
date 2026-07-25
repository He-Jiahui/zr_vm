#include "zr_vm_core/debug.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_common/zr_contract_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/task_frame_runtime.h"

#define ZR_DEBUG_TRACEBACK_DEFAULT_MAX_FRAMES 21u
#define ZR_DEBUG_TRACEBACK_FOLD_MARKER_FRAME_COST 1u

typedef struct SZrDebugTracebackWriter {
    TZrChar *buffer;
    TZrSize capacity;
    TZrSize length;
} SZrDebugTracebackWriter;

static void debug_traceback_writer_init(SZrDebugTracebackWriter *writer, TZrChar *buffer, TZrSize bufferSize) {
    writer->buffer = buffer;
    writer->capacity = bufferSize;
    writer->length = 0u;
    if (buffer != ZR_NULL && bufferSize > 0u) {
        buffer[0] = '\0';
    }
}

static void debug_traceback_append_span(SZrDebugTracebackWriter *writer, const TZrChar *text, TZrSize textLength) {
    TZrSize writable;

    if (writer == ZR_NULL || writer->buffer == ZR_NULL || writer->capacity == 0u || text == ZR_NULL ||
        textLength == 0u) {
        return;
    }

    if (writer->length >= writer->capacity - 1u) {
        writer->buffer[writer->capacity - 1u] = '\0';
        return;
    }

    writable = writer->capacity - 1u - writer->length;
    if (textLength < writable) {
        writable = textLength;
    }

    memcpy(writer->buffer + writer->length, text, writable);
    writer->length += writable;
    writer->buffer[writer->length] = '\0';
}

static void debug_traceback_append_cstr(SZrDebugTracebackWriter *writer, const TZrChar *text) {
    if (text != ZR_NULL) {
        debug_traceback_append_span(writer, text, strlen(text));
    }
}

static void debug_traceback_append_u32(SZrDebugTracebackWriter *writer, TZrUInt32 value) {
    TZrChar numberBuffer[32];
    int written = snprintf(numberBuffer, sizeof(numberBuffer), "%u", (unsigned int)value);

    if (written > 0) {
        debug_traceback_append_span(writer, numberBuffer, (TZrSize)written);
    }
}

static TZrUInt32 debug_traceback_count_frames(struct SZrState *state, TZrUInt32 level) {
    TZrUInt32 count = 0u;
    SZrDebugActivation activation;

    while (ZrCore_Debug_GetStack(state, level + count, &activation)) {
        count++;
    }

    return count;
}

static void debug_traceback_append_prefix(SZrDebugTracebackWriter *writer, TZrNativeString prefixMessage) {
    TZrSize prefixLength;

    if (prefixMessage == ZR_NULL || prefixMessage[0] == '\0') {
        return;
    }

    prefixLength = strlen(prefixMessage);
    debug_traceback_append_span(writer, prefixMessage, prefixLength);
    if (prefixMessage[prefixLength - 1u] != '\n') {
        debug_traceback_append_cstr(writer, "\n");
    }
}

static void debug_traceback_append_frame(struct SZrState *state, SZrDebugTracebackWriter *writer, TZrUInt32 level) {
    SZrDebugActivation activation;
    SZrDebugInfo info;
    TZrNativeString functionName;
    TZrNativeString sourceName;

    memset(&activation, 0, sizeof(activation));
    memset(&info, 0, sizeof(info));
    if (!ZrCore_Debug_GetStack(state, level, &activation) ||
        !ZrCore_Debug_GetInfo(state,
                              &activation,
                              (EZrDebugInfoType)(ZR_DEBUG_INFO_FUNCTION_NAME | ZR_DEBUG_INFO_SOURCE_FILE |
                                                 ZR_DEBUG_INFO_LINE_NUMBER | ZR_DEBUG_INFO_TAIL_CALL),
                              &info)) {
        return;
    }

    functionName = info.name != ZR_NULL && info.name[0] != '\0' ? info.name : "<anonymous>";
    sourceName = info.source != ZR_NULL && info.source[0] != '\0' ? info.source : "<unknown>";

    debug_traceback_append_cstr(writer, "  at ");
    debug_traceback_append_cstr(writer, functionName);
    if (info.isNative) {
        debug_traceback_append_cstr(writer, " [native]");
    } else {
        debug_traceback_append_cstr(writer, " (");
        debug_traceback_append_cstr(writer, sourceName);
        if (info.currentLine > 0u) {
            debug_traceback_append_cstr(writer, ":");
            debug_traceback_append_u32(writer, (TZrUInt32)info.currentLine);
        }
        debug_traceback_append_cstr(writer, ")");
    }
    if (info.isTailCall) {
        debug_traceback_append_cstr(writer, " (...tail calls...)");
    }
    debug_traceback_append_cstr(writer, "\n");
}

static void debug_traceback_append_skip(SZrDebugTracebackWriter *writer, TZrUInt32 skippedFrames) {
    if (skippedFrames == 0u) {
        return;
    }

    debug_traceback_append_cstr(writer, "  ... (skipping ");
    debug_traceback_append_u32(writer, skippedFrames);
    debug_traceback_append_cstr(writer, " levels)\n");
}

static void debug_traceback_append_frames(struct SZrState *state,
                                          SZrDebugTracebackWriter *writer,
                                          TZrUInt32 level,
                                          TZrUInt32 totalFrames,
                                          TZrUInt32 maxFrames) {
    TZrUInt32 frameIndex;

    if (totalFrames == 0u) {
        return;
    }

    if (maxFrames == 0u) {
        maxFrames = ZR_DEBUG_TRACEBACK_DEFAULT_MAX_FRAMES;
    }

    if (totalFrames <= maxFrames || maxFrames <= ZR_DEBUG_TRACEBACK_FOLD_MARKER_FRAME_COST + 1u) {
        TZrUInt32 framesToRender = totalFrames < maxFrames ? totalFrames : maxFrames;
        for (frameIndex = 0u; frameIndex < framesToRender; frameIndex++) {
            debug_traceback_append_frame(state, writer, level + frameIndex);
        }
        if (totalFrames > framesToRender) {
            debug_traceback_append_skip(writer, totalFrames - framesToRender);
        }
        return;
    }

    {
        TZrUInt32 visibleFrames = maxFrames - ZR_DEBUG_TRACEBACK_FOLD_MARKER_FRAME_COST;
        TZrUInt32 headFrames = visibleFrames / 2u;
        TZrUInt32 tailFrames = visibleFrames - headFrames;
        TZrUInt32 skippedFrames = totalFrames - headFrames - tailFrames;
        TZrUInt32 tailStart = totalFrames - tailFrames;

        for (frameIndex = 0u; frameIndex < headFrames; frameIndex++) {
            debug_traceback_append_frame(state, writer, level + frameIndex);
        }
        debug_traceback_append_skip(writer, skippedFrames);
        for (frameIndex = tailStart; frameIndex < totalFrames; frameIndex++) {
            debug_traceback_append_frame(state, writer, level + frameIndex);
        }
    }
}

TZrSize ZrCore_Debug_Traceback(struct SZrState *state,
                               TZrNativeString prefixMessage,
                               TZrUInt32 level,
                               TZrUInt32 maxFrames,
                               TZrChar *buffer,
                               TZrSize bufferSize) {
    SZrDebugTracebackWriter writer;
    TZrUInt32 totalFrames;

    if (state == ZR_NULL || buffer == ZR_NULL || bufferSize == 0u) {
        return 0u;
    }

    debug_traceback_writer_init(&writer, buffer, bufferSize);
    debug_traceback_append_prefix(&writer, prefixMessage);
    totalFrames = debug_traceback_count_frames(state, level);
    debug_traceback_append_frames(state, &writer, level, totalFrames, maxFrames);
    return writer.length;
}

static TZrBool debug_async_contract_has_identity(const SZrDebugAsyncSchedulerContract *contract) {
    TZrUInt32 schedulerTable;
    TZrUInt32 taskTable;
    TZrUInt32 jobTable;

    if (contract == ZR_NULL) {
        return ZR_FALSE;
    }
    schedulerTable = ZR_METADATA_TOKEN_TABLE(contract->schedulerTypeToken);
    taskTable = ZR_METADATA_TOKEN_TABLE(contract->taskTypeToken);
    jobTable = ZR_METADATA_TOKEN_TABLE(contract->jobTypeToken);
    return contract != ZR_NULL &&
           contract->schedulerTypeToken != 0u &&
           contract->taskTypeToken != 0u &&
           contract->jobTypeToken != 0u &&
           (schedulerTable == ZR_METADATA_TABLE_TYPE_DEF || schedulerTable == ZR_METADATA_TABLE_TYPE_REF) &&
           (taskTable == ZR_METADATA_TABLE_TYPE_DEF || taskTable == ZR_METADATA_TABLE_TYPE_REF) &&
           (jobTable == ZR_METADATA_TABLE_TYPE_DEF || jobTable == ZR_METADATA_TABLE_TYPE_REF) &&
           contract->schedulerAbiVersion != 0u &&
           contract->schedulerPolicyMask != 0u &&
           contract->transportContractHash != 0u &&
           contract->schedulerContractHash != 0u;
}

TZrBool ZrCore_Debug_ProjectSchedulerSourceContract(
        const SZrFunction *function,
        TZrUInt32 schedulerTypeId,
        SZrDebugAsyncSchedulerContract *outContract) {
    const SZrFunctionSchedulerSourceFact *fact;

    if (outContract != ZR_NULL) {
        memset(outContract, 0, sizeof(*outContract));
    }
    if (function == ZR_NULL || outContract == ZR_NULL) {
        return ZR_FALSE;
    }
    fact = ZrCore_Function_FindSchedulerSourceFact(function, schedulerTypeId);
    if (fact == ZR_NULL ||
        fact->contractRole != ZR_MEMBER_CONTRACT_ROLE_TASK_SCHEDULER_SCHEDULE ||
        fact->scheduleMemberToken == 0u ||
        fact->scheduleSignatureToken == 0u ||
        fact->scheduleSignatureHash == 0u ||
        (ZR_METADATA_TOKEN_TABLE(fact->scheduleMemberToken) != ZR_METADATA_TABLE_MEMBER_DEF &&
         ZR_METADATA_TOKEN_TABLE(fact->scheduleMemberToken) != ZR_METADATA_TABLE_MEMBER_REF) ||
        ZR_METADATA_TOKEN_TABLE(fact->scheduleSignatureToken) != ZR_METADATA_TABLE_SIGNATURE ||
        fact->schedulerProvider.metadataToken == 0u ||
        fact->taskProvider.metadataToken == 0u ||
        fact->jobProvider.metadataToken == 0u) {
        return ZR_FALSE;
    }

    outContract->origin = ZR_DEBUG_ASYNC_CONTRACT_ORIGIN_SOURCE_FACT;
    outContract->schedulerTypeToken = fact->schedulerProvider.metadataToken;
    outContract->taskTypeToken = fact->taskProvider.metadataToken;
    outContract->jobTypeToken = fact->jobProvider.metadataToken;
    outContract->scheduleMemberToken = fact->scheduleMemberToken;
    outContract->scheduleSignatureToken = fact->scheduleSignatureToken;
    outContract->scheduleSignatureHash = fact->scheduleSignatureHash;
    outContract->schedulerAbiVersion = fact->schedulerAbiVersion;
    outContract->schedulerPolicyMask = fact->schedulerPolicyMask;
    outContract->attachedRequirementFlags = fact->attachedRequirementFlags;
    outContract->isolatedRequirementFlags = fact->isolatedRequirementFlags;
    outContract->transportContractHash = fact->transportContractHash;
    outContract->schedulerContractHash = fact->schedulerContractHash;
    if (!debug_async_contract_has_identity(outContract)) {
        memset(outContract, 0, sizeof(*outContract));
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Debug_ProjectSchedulerArtifactContract(
        const SZrArtifactSchedulerContractRow *row,
        SZrDebugAsyncSchedulerContract *outContract) {
    if (outContract != ZR_NULL) {
        memset(outContract, 0, sizeof(*outContract));
    }
    if (row == ZR_NULL || outContract == ZR_NULL) {
        return ZR_FALSE;
    }

    outContract->origin = ZR_DEBUG_ASYNC_CONTRACT_ORIGIN_ARTIFACT_ROW;
    outContract->schedulerTypeToken = row->schedulerTypeToken;
    outContract->taskTypeToken = row->taskTypeToken;
    outContract->jobTypeToken = row->jobTypeToken;
    outContract->schedulerAbiVersion = row->abiVersion;
    outContract->schedulerPolicyMask = row->policyMask;
    outContract->attachedRequirementFlags = row->attachedRequirementFlags;
    outContract->isolatedRequirementFlags = row->isolatedRequirementFlags;
    outContract->transportContractHash = row->transportContractHash;
    outContract->schedulerContractHash = row->schedulerContractHash;
    if (!debug_async_contract_has_identity(outContract)) {
        memset(outContract, 0, sizeof(*outContract));
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Debug_AsyncSchedulerContractsEqual(
        const SZrDebugAsyncSchedulerContract *left,
        const SZrDebugAsyncSchedulerContract *right) {
    if (!debug_async_contract_has_identity(left) || !debug_async_contract_has_identity(right)) {
        return ZR_FALSE;
    }
    if (left->schedulerTypeToken != right->schedulerTypeToken ||
        left->taskTypeToken != right->taskTypeToken ||
        left->jobTypeToken != right->jobTypeToken ||
        left->schedulerAbiVersion != right->schedulerAbiVersion ||
        left->schedulerPolicyMask != right->schedulerPolicyMask ||
        left->attachedRequirementFlags != right->attachedRequirementFlags ||
        left->isolatedRequirementFlags != right->isolatedRequirementFlags ||
        left->transportContractHash != right->transportContractHash ||
        left->schedulerContractHash != right->schedulerContractHash) {
        return ZR_FALSE;
    }
    if (left->origin == ZR_DEBUG_ASYNC_CONTRACT_ORIGIN_SOURCE_FACT &&
        right->origin == ZR_DEBUG_ASYNC_CONTRACT_ORIGIN_SOURCE_FACT) {
        return left->scheduleMemberToken == right->scheduleMemberToken &&
               left->scheduleSignatureToken == right->scheduleSignatureToken &&
               left->scheduleSignatureHash == right->scheduleSignatureHash;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Debug_ProjectTaskFrameTerminal(
        TZrUInt32 taskFrameStatus,
        TZrBool isolatedTransport,
        EZrDebugAsyncFaultProvenance faultProvenance,
        SZrDebugAsyncTerminalEvent *outEvent) {
    if (outEvent != ZR_NULL) {
        memset(outEvent, 0, sizeof(*outEvent));
    }
    if (outEvent == ZR_NULL) {
        return ZR_FALSE;
    }
    outEvent->taskFrameStatus = taskFrameStatus;
    if (taskFrameStatus == ZR_CORE_TASK_FRAME_STATUS_COMPLETED) {
        outEvent->terminalState = isolatedTransport
                                          ? ZR_DEBUG_ASYNC_TERMINAL_ISOLATED_COMPLETED
                                          : ZR_DEBUG_ASYNC_TERMINAL_ATTACHED_COMPLETED;
        return ZR_TRUE;
    }
    if (taskFrameStatus == ZR_CORE_TASK_FRAME_STATUS_FAULTED) {
        if (faultProvenance >= ZR_DEBUG_ASYNC_FAULT_MAX) {
            return ZR_FALSE;
        }
        outEvent->terminalState = isolatedTransport
                                          ? ZR_DEBUG_ASYNC_TERMINAL_ISOLATED_FAULTED
                                          : ZR_DEBUG_ASYNC_TERMINAL_ATTACHED_FAULTED;
        outEvent->faultProvenance = faultProvenance;
        outEvent->isFaulted = ZR_TRUE;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}
