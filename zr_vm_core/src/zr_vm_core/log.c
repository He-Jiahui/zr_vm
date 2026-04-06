//
// Created by HeJiahui on 2025/6/26.
//
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "zr_vm_core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#if !defined(PTHREAD_MUTEX_RECURSIVE) && defined(PTHREAD_MUTEX_RECURSIVE_NP)
#define PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
#endif
#endif

#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"

#if defined(_WIN32)
static INIT_ONCE g_zr_log_lock_once = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION g_zr_log_lock;

static BOOL CALLBACK zr_log_init_lock(PINIT_ONCE initOnce, PVOID parameter, PVOID context) {
    ZR_UNUSED_PARAMETER(initOnce);
    ZR_UNUSED_PARAMETER(parameter);
    ZR_UNUSED_PARAMETER(context);
    InitializeCriticalSection(&g_zr_log_lock);
    return TRUE;
}

static void zr_log_lock(void) {
    InitOnceExecuteOnce(&g_zr_log_lock_once, zr_log_init_lock, ZR_NULL, ZR_NULL);
    EnterCriticalSection(&g_zr_log_lock);
}

static void zr_log_unlock(void) {
    LeaveCriticalSection(&g_zr_log_lock);
}
#else
static pthread_once_t g_zr_log_lock_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_zr_log_lock;

static void zr_log_init_lock(void) {
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_zr_log_lock, &attr);
    pthread_mutexattr_destroy(&attr);
}

static void zr_log_lock(void) {
    pthread_once(&g_zr_log_lock_once, zr_log_init_lock);
    pthread_mutex_lock(&g_zr_log_lock);
}

static void zr_log_unlock(void) {
    pthread_mutex_unlock(&g_zr_log_lock);
}
#endif

static FILE *zr_log_channel_to_stream(EZrOutputChannel channel) {
    return channel == ZR_OUTPUT_CHANNEL_STDERR ? stderr : stdout;
}

static SZrGlobalState *zr_log_resolve_global(struct SZrState *state) {
    return state != ZR_NULL ? state->global : ZR_NULL;
}

static void zr_log_write_default_sink(EZrOutputChannel channel, TZrNativeString message) {
    FILE *stream;
    size_t length;

    if (message == ZR_NULL) {
        return;
    }

    stream = zr_log_channel_to_stream(channel);
    if (stream == ZR_NULL) {
        return;
    }

    length = strlen(message);
    if (length > 0) {
        fwrite(message, 1, length, stream);
    }
    fflush(stream);
}

static void zr_log_vwritef(struct SZrState *state,
                           EZrLogLevel level,
                           EZrOutputChannel channel,
                           EZrOutputKind kind,
                           TZrNativeString format,
                           va_list args) {
    va_list copyArgs;
    va_list formatArgs;
    int requiredLength;
    char stackBuffer[512];
    char *heapBuffer = ZR_NULL;
    TZrNativeString message = stackBuffer;

    if (format == ZR_NULL) {
        return;
    }

    va_copy(copyArgs, args);
    requiredLength = vsnprintf(stackBuffer, sizeof(stackBuffer), format, copyArgs);
    va_end(copyArgs);
    if (requiredLength < 0) {
        return;
    }

    if ((size_t)requiredLength >= sizeof(stackBuffer)) {
        heapBuffer = (char *)malloc((size_t)requiredLength + 1u);
        if (heapBuffer == ZR_NULL) {
            ZrCore_Log_Write(state,
                             ZR_LOG_LEVEL_FATAL,
                             ZR_OUTPUT_CHANNEL_STDERR,
                             ZR_OUTPUT_KIND_DIAGNOSTIC,
                             ZR_ERROR_MESSAGE_NOT_ENOUGH_MEMORY);
            return;
        }

        va_copy(formatArgs, args);
        vsnprintf(heapBuffer, (size_t)requiredLength + 1u, format, formatArgs);
        va_end(formatArgs);
        message = heapBuffer;
    }

    ZrCore_Log_Write(state, level, channel, kind, message);
    if (heapBuffer != ZR_NULL) {
        free(heapBuffer);
    }
}

void ZrCore_Log_Write(struct SZrState *state,
                      EZrLogLevel level,
                      EZrOutputChannel channel,
                      EZrOutputKind kind,
                      TZrNativeString message) {
    SZrGlobalState *global = zr_log_resolve_global(state);

    zr_log_lock();
    zr_log_write_default_sink(channel, message);
    if (global != ZR_NULL && global->logFunction != ZR_NULL) {
        global->logFunction(state, level, channel, kind, message != ZR_NULL ? message : "");
    }
    zr_log_unlock();
}

void ZrCore_Log_Printf(struct SZrState *state,
                       EZrLogLevel level,
                       EZrOutputChannel channel,
                       EZrOutputKind kind,
                       TZrNativeString format,
                       ...) {
    va_list args;

    va_start(args, format);
    zr_log_vwritef(state, level, channel, kind, format, args);
    va_end(args);
}

void ZrCore_Log_Resultf(struct SZrState *state, TZrNativeString format, ...) {
    va_list args;

    va_start(args, format);
    zr_log_vwritef(state, ZR_LOG_LEVEL_INFO, ZR_OUTPUT_CHANNEL_STDOUT, ZR_OUTPUT_KIND_RESULT, format, args);
    va_end(args);
}

void ZrCore_Log_Helpf(struct SZrState *state, TZrNativeString format, ...) {
    va_list args;

    va_start(args, format);
    zr_log_vwritef(state, ZR_LOG_LEVEL_INFO, ZR_OUTPUT_CHANNEL_STDOUT, ZR_OUTPUT_KIND_HELP, format, args);
    va_end(args);
}

void ZrCore_Log_Metaf(struct SZrState *state, TZrNativeString format, ...) {
    va_list args;

    va_start(args, format);
    zr_log_vwritef(state, ZR_LOG_LEVEL_INFO, ZR_OUTPUT_CHANNEL_STDOUT, ZR_OUTPUT_KIND_META, format, args);
    va_end(args);
}

void ZrCore_Log_Diagnosticf(struct SZrState *state,
                            EZrLogLevel level,
                            EZrOutputChannel channel,
                            TZrNativeString format,
                            ...) {
    va_list args;

    va_start(args, format);
    zr_log_vwritef(state, level, channel, ZR_OUTPUT_KIND_DIAGNOSTIC, format, args);
    va_end(args);
}

void ZrCore_Log_Error(struct SZrState *state, TZrNativeString format, ...) {
    va_list args;

    va_start(args, format);
    zr_log_vwritef(state,
                   ZR_LOG_LEVEL_ERROR,
                   ZR_OUTPUT_CHANNEL_STDERR,
                   ZR_OUTPUT_KIND_DIAGNOSTIC,
                   format,
                   args);
    va_end(args);
}

void ZrCore_Log_Exception(struct SZrState *state, TZrNativeString format, ...) {
    va_list args;

    va_start(args, format);
    zr_log_vwritef(state,
                   ZR_LOG_LEVEL_EXCEPTION,
                   ZR_OUTPUT_CHANNEL_STDERR,
                   ZR_OUTPUT_KIND_DIAGNOSTIC,
                   format,
                   args);
    va_end(args);
}

void ZrCore_Log_Fatal(struct SZrState *state, TZrNativeString format, ...) {
    va_list args;

    va_start(args, format);
    zr_log_vwritef(state,
                   ZR_LOG_LEVEL_FATAL,
                   ZR_OUTPUT_CHANNEL_STDERR,
                   ZR_OUTPUT_KIND_DIAGNOSTIC,
                   format,
                   args);
    va_end(args);
}

void ZrCore_Log_FlushDefaultSinks(void) {
    zr_log_lock();
    fflush(stdout);
    fflush(stderr);
    zr_log_unlock();
}
