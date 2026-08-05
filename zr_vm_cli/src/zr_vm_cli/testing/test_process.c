#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "testing/test_process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZR_CLI_TEST_WORKER_CASE_ENV "ZR_VM_TEST_CASE_ID"

static void test_process_append_output(
        SZrCliTestProcessResult *result,
        const TZrChar *bytes,
        TZrSize length) {
    TZrSize used;
    TZrSize available;

    if (result == ZR_NULL || bytes == ZR_NULL || length == 0U) {
        return;
    }
    used = strlen(result->output);
    if (used >= sizeof(result->output) - 1U) {
        return;
    }
    available = sizeof(result->output) - used - 1U;
    if (length > available) {
        length = available;
    }
    memcpy(result->output + used, bytes, length);
    result->output[used + length] = '\0';
}

#if defined(_WIN32)

#include <windows.h>

static SRWLOCK g_test_process_environment_lock = SRWLOCK_INIT;

static TZrBool test_process_append_windows_argument(
        TZrChar *commandLine,
        TZrSize capacity,
        const TZrChar *argument) {
    TZrSize used;
    TZrSize slashCount = 0U;

    if (commandLine == ZR_NULL || capacity == 0U || argument == ZR_NULL) {
        return ZR_FALSE;
    }
    used = strlen(commandLine);
    if (used > 0U) {
        if (used + 1U >= capacity) return ZR_FALSE;
        commandLine[used++] = ' ';
        commandLine[used] = '\0';
    }
    if (used + 1U >= capacity) return ZR_FALSE;
    commandLine[used++] = '"';
    for (const TZrChar *cursor = argument;; cursor++) {
        if (*cursor == '\\') {
            slashCount++;
            continue;
        }
        if (*cursor == '"' || *cursor == '\0') {
            TZrSize emitCount = slashCount * 2U + (*cursor == '"' ? 1U : 0U);
            if (emitCount > capacity - used - 1U) return ZR_FALSE;
            while (emitCount > 0U) {
                commandLine[used++] = '\\';
                emitCount--;
            }
            slashCount = 0U;
            if (*cursor == '\0') break;
        } else {
            if (slashCount > capacity - used - 1U) return ZR_FALSE;
            while (slashCount > 0U) {
                commandLine[used++] = '\\';
                slashCount--;
            }
        }
        if (used + 1U >= capacity) return ZR_FALSE;
        commandLine[used++] = *cursor;
    }
    if (used + 2U > capacity) return ZR_FALSE;
    commandLine[used++] = '"';
    commandLine[used] = '\0';
    return ZR_TRUE;
}

static TZrBool test_process_create_windows_child(
        const SZrCliTestProcessRequest *request,
        HANDLE inputHandle,
        HANDLE outputHandle,
        PROCESS_INFORMATION *processInfo,
        DWORD *outError) {
    STARTUPINFOA startupInfo;
    TZrChar commandLine[32768];
    TZrChar *previousValue = ZR_NULL;
    DWORD previousLength;
    TZrBool hadPreviousValue = ZR_FALSE;
    BOOL created;

    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(processInfo, 0, sizeof(*processInfo));
    memset(commandLine, 0, sizeof(commandLine));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdOutput = outputHandle;
    startupInfo.hStdError = outputHandle;
    startupInfo.hStdInput = inputHandle;
    if (!test_process_append_windows_argument(commandLine, sizeof(commandLine), request->executablePath) ||
        !test_process_append_windows_argument(commandLine, sizeof(commandLine), "test") ||
        !test_process_append_windows_argument(commandLine, sizeof(commandLine), request->targetPath) ||
        !test_process_append_windows_argument(commandLine, sizeof(commandLine), "--jobs") ||
        !test_process_append_windows_argument(commandLine, sizeof(commandLine), "1")) {
        return ZR_FALSE;
    }

    AcquireSRWLockExclusive(&g_test_process_environment_lock);
    previousLength = GetEnvironmentVariableA(ZR_CLI_TEST_WORKER_CASE_ENV, ZR_NULL, 0U);
    if (previousLength > 0U) {
        previousValue = (TZrChar *)malloc(previousLength);
        hadPreviousValue = previousValue != ZR_NULL &&
                           GetEnvironmentVariableA(
                                   ZR_CLI_TEST_WORKER_CASE_ENV,
                                   previousValue,
                                   previousLength) > 0U;
    }
    if (!SetEnvironmentVariableA(ZR_CLI_TEST_WORKER_CASE_ENV, request->caseId)) {
        free(previousValue);
        ReleaseSRWLockExclusive(&g_test_process_environment_lock);
        return ZR_FALSE;
    }
    created = CreateProcessA(
            ZR_NULL,
            commandLine,
            ZR_NULL,
            ZR_NULL,
            TRUE,
            CREATE_NO_WINDOW,
            ZR_NULL,
            ZR_NULL,
            &startupInfo,
            processInfo);
    if (outError != ZR_NULL) {
        *outError = created ? ERROR_SUCCESS : GetLastError();
    }
    SetEnvironmentVariableA(
            ZR_CLI_TEST_WORKER_CASE_ENV,
            hadPreviousValue ? previousValue : ZR_NULL);
    free(previousValue);
    ReleaseSRWLockExclusive(&g_test_process_environment_lock);
    return created ? ZR_TRUE : ZR_FALSE;
}

TZrBool ZrCli_TestProcess_Run(
        const SZrCliTestProcessRequest *request,
        SZrCliTestProcessResult *outResult) {
    SECURITY_ATTRIBUTES securityAttributes;
    PROCESS_INFORMATION processInfo;
    TZrChar temporaryDirectory[MAX_PATH];
    TZrChar temporaryPath[MAX_PATH];
    HANDLE inputHandle = INVALID_HANDLE_VALUE;
    HANDLE outputHandle = INVALID_HANDLE_VALUE;
    ULONGLONG startedAt;
    DWORD waitMilliseconds;
    DWORD waitResult;
    DWORD exitCode = 3U;
    TZrChar readBuffer[512];
    DWORD readLength;
    DWORD launchError = ERROR_SUCCESS;
    TZrBool success = ZR_FALSE;

    if (request == ZR_NULL || outResult == ZR_NULL || request->executablePath == ZR_NULL ||
        request->targetPath == ZR_NULL || request->caseId == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outResult, 0, sizeof(*outResult));
    outResult->exitCode = 3;
    memset(&securityAttributes, 0, sizeof(securityAttributes));
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;
    if (GetTempPathA(sizeof(temporaryDirectory), temporaryDirectory) == 0U ||
        GetTempFileNameA(temporaryDirectory, "zrt", 0U, temporaryPath) == 0U) {
        return ZR_FALSE;
    }
    outputHandle = CreateFileA(
            temporaryPath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            &securityAttributes,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
            ZR_NULL);
    if (outputHandle == INVALID_HANDLE_VALUE) {
        DeleteFileA(temporaryPath);
        return ZR_FALSE;
    }
    inputHandle = CreateFileA(
            "NUL",
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &securityAttributes,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            ZR_NULL);
    if (inputHandle == INVALID_HANDLE_VALUE) {
        CloseHandle(outputHandle);
        DeleteFileA(temporaryPath);
        return ZR_FALSE;
    }
    startedAt = GetTickCount64();
    if (!test_process_create_windows_child(
                request,
                inputHandle,
                outputHandle,
                &processInfo,
                &launchError)) {
        snprintf(
                outResult->output,
                sizeof(outResult->output),
                "CreateProcessA failed with Windows error %lu",
                (unsigned long)launchError);
        CloseHandle(inputHandle);
        CloseHandle(outputHandle);
        DeleteFileA(temporaryPath);
        return ZR_FALSE;
    }
    CloseHandle(inputHandle);
    waitMilliseconds = request->timeoutMilliseconds == 0U ||
                       request->timeoutMilliseconds >= (TZrUInt64)INFINITE
                       ? INFINITE
                       : (DWORD)request->timeoutMilliseconds;
    waitResult = WaitForSingleObject(processInfo.hProcess, waitMilliseconds);
    if (waitResult == WAIT_TIMEOUT) {
        outResult->timedOut = ZR_TRUE;
        TerminateProcess(processInfo.hProcess, 1U);
        WaitForSingleObject(processInfo.hProcess, INFINITE);
    } else if (waitResult != WAIT_OBJECT_0) {
        TerminateProcess(processInfo.hProcess, 3U);
        WaitForSingleObject(processInfo.hProcess, INFINITE);
    }
    if (GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
        outResult->exitCode = (int)exitCode;
        success = ZR_TRUE;
    }
    outResult->durationMilliseconds = (TZrUInt64)(GetTickCount64() - startedAt);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    SetFilePointer(outputHandle, 0, ZR_NULL, FILE_BEGIN);
    while (ReadFile(outputHandle, readBuffer, sizeof(readBuffer), &readLength, ZR_NULL) &&
           readLength > 0U) {
        test_process_append_output(outResult, readBuffer, (TZrSize)readLength);
    }
    CloseHandle(outputHandle);
    DeleteFileA(temporaryPath);
    return success;
}

#else

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t g_test_process_environment_lock = PTHREAD_MUTEX_INITIALIZER;

static TZrUInt64 test_process_monotonic_milliseconds(void) {
    struct timespec value;

    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        return 0U;
    }
    return (TZrUInt64)value.tv_sec * UINT64_C(1000) +
           (TZrUInt64)value.tv_nsec / UINT64_C(1000000);
}

static void test_process_read_pipe(int pipeFd, SZrCliTestProcessResult *result) {
    TZrChar buffer[512];
    ssize_t count;

    do {
        count = read(pipeFd, buffer, sizeof(buffer));
        if (count > 0) {
            test_process_append_output(result, buffer, (TZrSize)count);
        }
    } while (count > 0);
}

TZrBool ZrCli_TestProcess_Run(
        const SZrCliTestProcessRequest *request,
        SZrCliTestProcessResult *outResult) {
    int outputPipe[2];
    pid_t processId;
    TZrUInt64 startedAt;
    TZrUInt64 now;
    int status = 0;
    TZrBool completed = ZR_FALSE;
    struct timespec pollingDelay = {0, 10 * 1000 * 1000};
    TZrChar *previousCaseId = ZR_NULL;
    const TZrChar *existingCaseId;

    if (request == ZR_NULL || outResult == ZR_NULL || request->executablePath == ZR_NULL ||
        request->targetPath == ZR_NULL || request->caseId == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outResult, 0, sizeof(*outResult));
    outResult->exitCode = 3;
    if (pipe(outputPipe) != 0) {
        return ZR_FALSE;
    }
    startedAt = test_process_monotonic_milliseconds();
    if (pthread_mutex_lock(&g_test_process_environment_lock) != 0) {
        close(outputPipe[0]);
        close(outputPipe[1]);
        return ZR_FALSE;
    }
    existingCaseId = getenv(ZR_CLI_TEST_WORKER_CASE_ENV);
    if (existingCaseId != ZR_NULL) {
        previousCaseId = strdup(existingCaseId);
        if (previousCaseId == ZR_NULL) {
            pthread_mutex_unlock(&g_test_process_environment_lock);
            close(outputPipe[0]);
            close(outputPipe[1]);
            return ZR_FALSE;
        }
    }
    if (setenv(ZR_CLI_TEST_WORKER_CASE_ENV, request->caseId, 1) != 0) {
        free(previousCaseId);
        pthread_mutex_unlock(&g_test_process_environment_lock);
        close(outputPipe[0]);
        close(outputPipe[1]);
        return ZR_FALSE;
    }
    processId = fork();
    if (processId < 0) {
        if (previousCaseId != ZR_NULL) {
            setenv(ZR_CLI_TEST_WORKER_CASE_ENV, previousCaseId, 1);
        } else {
            unsetenv(ZR_CLI_TEST_WORKER_CASE_ENV);
        }
        free(previousCaseId);
        pthread_mutex_unlock(&g_test_process_environment_lock);
        close(outputPipe[0]);
        close(outputPipe[1]);
        return ZR_FALSE;
    }
    if (processId == 0) {
        close(outputPipe[0]);
        dup2(outputPipe[1], STDOUT_FILENO);
        dup2(outputPipe[1], STDERR_FILENO);
        close(outputPipe[1]);
        execlp(
                request->executablePath,
                request->executablePath,
                "test",
                request->targetPath,
                "--jobs",
                "1",
                (TZrChar *)ZR_NULL);
        _exit(127);
    }
    if (previousCaseId != ZR_NULL) {
        setenv(ZR_CLI_TEST_WORKER_CASE_ENV, previousCaseId, 1);
    } else {
        unsetenv(ZR_CLI_TEST_WORKER_CASE_ENV);
    }
    free(previousCaseId);
    pthread_mutex_unlock(&g_test_process_environment_lock);
    close(outputPipe[1]);
    fcntl(outputPipe[0], F_SETFL, fcntl(outputPipe[0], F_GETFL, 0) | O_NONBLOCK);
    while (!completed) {
        pid_t waitResult;

        test_process_read_pipe(outputPipe[0], outResult);
        waitResult = waitpid(processId, &status, WNOHANG);
        if (waitResult == processId) {
            completed = ZR_TRUE;
            break;
        }
        if (waitResult < 0 && errno != EINTR) {
            close(outputPipe[0]);
            return ZR_FALSE;
        }
        now = test_process_monotonic_milliseconds();
        if (request->timeoutMilliseconds > 0U &&
            now - startedAt >= request->timeoutMilliseconds) {
            TZrUInt64 graceDeadline;
            pid_t graceResult = 0;

            outResult->timedOut = ZR_TRUE;
            kill(processId, SIGTERM);
            graceDeadline = now + 100U;
            while ((graceResult = waitpid(processId, &status, WNOHANG)) == 0 &&
                   test_process_monotonic_milliseconds() < graceDeadline) {
                test_process_read_pipe(outputPipe[0], outResult);
                nanosleep(&pollingDelay, ZR_NULL);
            }
            if (graceResult <= 0) {
                kill(processId, SIGKILL);
                while (waitpid(processId, &status, 0) < 0 && errno == EINTR) { }
            }
            completed = ZR_TRUE;
            break;
        }
        nanosleep(&pollingDelay, ZR_NULL);
    }
    test_process_read_pipe(outputPipe[0], outResult);
    close(outputPipe[0]);
    outResult->durationMilliseconds =
            test_process_monotonic_milliseconds() - startedAt;
    if (outResult->timedOut) {
        outResult->exitCode = 1;
    } else if (WIFEXITED(status)) {
        outResult->exitCode = WEXITSTATUS(status);
    } else {
        outResult->exitCode = 3;
    }
    return ZR_TRUE;
}

#endif
