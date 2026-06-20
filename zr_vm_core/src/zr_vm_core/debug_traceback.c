#include "zr_vm_core/debug.h"

#include <stdio.h>
#include <string.h>

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
