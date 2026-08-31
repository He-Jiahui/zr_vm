#if !defined(_WIN32)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "perf_process.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "persistent_protocol.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

static void zr_perf_process_format_windows_error(DWORD errorCode, char *buffer, size_t bufferSize) {
    DWORD written;

    if (buffer == NULL || bufferSize == 0U) {
        return;
    }
    written = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL,
                             errorCode,
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                             buffer,
                             (DWORD)bufferSize,
                             NULL);
    if (written == 0U) {
        snprintf(buffer, bufferSize, "Windows error %lu", (unsigned long)errorCode);
    }
}

static int zr_perf_process_windows_append(char *buffer,
                                          size_t capacity,
                                          size_t *length,
                                          char value) {
    if (buffer == NULL || length == NULL || *length + 1U >= capacity) {
        return 0;
    }
    buffer[(*length)++] = value;
    buffer[*length] = '\0';
    return 1;
}

static int zr_perf_process_windows_repeat(char *buffer,
                                          size_t capacity,
                                          size_t *length,
                                          char value,
                                          size_t count) {
    size_t index;
    for (index = 0U; index < count; index++) {
        if (!zr_perf_process_windows_append(buffer, capacity, length, value)) {
            return 0;
        }
    }
    return 1;
}

static int zr_perf_process_windows_text(char *buffer,
                                        size_t capacity,
                                        size_t *length,
                                        const char *text) {
    while (text != NULL && *text != '\0') {
        if (!zr_perf_process_windows_append(buffer, capacity, length, *text++)) {
            return 0;
        }
    }
    return 1;
}

static int zr_perf_process_windows_argument(char *buffer,
                                            size_t capacity,
                                            size_t *length,
                                            const char *argument) {
    const char *cursor;
    size_t slashCount = 0U;
    int needsQuotes = argument == NULL || argument[0] == '\0';

    for (cursor = argument; !needsQuotes && cursor != NULL && *cursor != '\0'; cursor++) {
        needsQuotes = *cursor == ' ' || *cursor == '\t' || *cursor == '"';
    }
    if (!needsQuotes) {
        return zr_perf_process_windows_text(buffer, capacity, length, argument);
    }
    if (!zr_perf_process_windows_append(buffer, capacity, length, '"')) {
        return 0;
    }
    for (cursor = argument != NULL ? argument : ""; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            slashCount++;
            continue;
        }
        if (*cursor == '"') {
            if (!zr_perf_process_windows_repeat(buffer, capacity, length, '\\', slashCount * 2U + 1U)) {
                return 0;
            }
        } else if (!zr_perf_process_windows_repeat(buffer, capacity, length, '\\', slashCount)) {
            return 0;
        }
        slashCount = 0U;
        if (!zr_perf_process_windows_append(buffer, capacity, length, *cursor)) {
            return 0;
        }
    }
    return zr_perf_process_windows_repeat(buffer, capacity, length, '\\', slashCount * 2U) &&
           zr_perf_process_windows_append(buffer, capacity, length, '"');
}

static char *zr_perf_process_windows_command_line(char *const *command) {
    size_t capacity = 1U;
    size_t length = 0U;
    char *line;
    int index;

    for (index = 0; command[index] != NULL; index++) {
        if (strlen(command[index]) > (SIZE_MAX - capacity - 4U) / 2U) {
            return NULL;
        }
        capacity += strlen(command[index]) * 2U + 4U;
    }
    line = (char *)malloc(capacity);
    if (line == NULL) {
        return NULL;
    }
    line[0] = '\0';
    for (index = 0; command[index] != NULL; index++) {
        if ((index > 0 && !zr_perf_process_windows_append(line, capacity, &length, ' ')) ||
            !zr_perf_process_windows_argument(line, capacity, &length, command[index])) {
            free(line);
            return NULL;
        }
    }
    return line;
}

static int zr_perf_process_run_once(const char *workingDirectory,
                                    char *const *command,
                                    uint32_t processTimeoutMs,
                                    SZrPerfRunSample *sample,
                                    char *errorBuffer,
                                    size_t errorBufferSize) {
    LARGE_INTEGER frequency;
    LARGE_INTEGER startTime;
    LARGE_INTEGER endTime;
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    PROCESS_MEMORY_COUNTERS counters;
    DWORD exitCode;
    DWORD waitResult;
    HANDLE nullHandle;
    HANDLE job;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobLimits;
    char *commandLine;

    memset(sample, 0, sizeof(*sample));
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    memset(&counters, 0, sizeof(counters));
    memset(&jobLimits, 0, sizeof(jobLimits));
    commandLine = zr_perf_process_windows_command_line(command);
    nullHandle = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    job = CreateJobObjectA(NULL, NULL);
    jobLimits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (commandLine == NULL || nullHandle == INVALID_HANDLE_VALUE || job == NULL ||
        !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jobLimits, sizeof(jobLimits))) {
        zr_perf_process_format_windows_error(GetLastError(), errorBuffer, errorBufferSize);
        free(commandLine);
        if (nullHandle != INVALID_HANDLE_VALUE) CloseHandle(nullHandle);
        if (job != NULL) CloseHandle(job);
        return 0;
    }
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = nullHandle;
    startup.hStdError = nullHandle;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&startTime);
    if (!CreateProcessA(command[0], commandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL,
                        workingDirectory != NULL && workingDirectory[0] != '\0' ? workingDirectory : NULL,
                        &startup, &process)) {
        zr_perf_process_format_windows_error(GetLastError(), errorBuffer, errorBufferSize);
        CloseHandle(nullHandle);
        CloseHandle(job);
        free(commandLine);
        return 0;
    }
    if (!AssignProcessToJobObject(job, process.hProcess) || ResumeThread(process.hThread) == (DWORD)-1) {
        zr_perf_process_format_windows_error(GetLastError(), errorBuffer, errorBufferSize);
        TerminateJobObject(job, 125U);
        WaitForSingleObject(process.hProcess, 1000U);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(nullHandle);
        CloseHandle(job);
        free(commandLine);
        return 0;
    }
    waitResult = WaitForSingleObject(process.hProcess, processTimeoutMs);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateJobObject(job, 124U);
        WaitForSingleObject(process.hProcess, 1000U);
        snprintf(errorBuffer, errorBufferSize, "process timeout after %" PRIu32 " ms", processTimeoutMs);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(nullHandle);
        CloseHandle(job);
        free(commandLine);
        return 0;
    }
    if (waitResult != WAIT_OBJECT_0) {
        zr_perf_process_format_windows_error(GetLastError(), errorBuffer, errorBufferSize);
        TerminateJobObject(job, 125U);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(nullHandle);
        CloseHandle(job);
        free(commandLine);
        return 0;
    }
    QueryPerformanceCounter(&endTime);
    if (!GetExitCodeProcess(process.hProcess, &exitCode)) {
        zr_perf_process_format_windows_error(GetLastError(), errorBuffer, errorBufferSize);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(nullHandle);
        CloseHandle(job);
        free(commandLine);
        return 0;
    }
    counters.cb = sizeof(counters);
    if (!GetProcessMemoryInfo(process.hProcess, &counters, sizeof(counters))) {
        counters.PeakWorkingSetSize = 0U;
    }
    sample->wallMs = ((double)(endTime.QuadPart - startTime.QuadPart) * 1000.0) /
                     (double)frequency.QuadPart;
    sample->peakWorkingSetBytes = (uint64_t)counters.PeakWorkingSetSize;
    sample->exitCode = (int)exitCode;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(nullHandle);
    CloseHandle(job);
    free(commandLine);
    return 1;
}

#else
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static double zr_perf_process_timespec_diff_ms(const struct timespec *startTime,
                                               const struct timespec *endTime) {
    return (double)(endTime->tv_sec - startTime->tv_sec) * 1000.0 +
           (double)(endTime->tv_nsec - startTime->tv_nsec) / 1000000.0;
}

static int zr_perf_process_run_once(const char *workingDirectory,
                                    char *const *command,
                                    uint32_t processTimeoutMs,
                                    SZrPerfRunSample *sample,
                                    char *errorBuffer,
                                    size_t errorBufferSize) {
    pid_t childPid;
    int status;
    struct rusage usage;
    struct timespec startTime;
    struct timespec endTime;
    uint64_t deadlineMs;

    memset(sample, 0, sizeof(*sample));
    memset(&usage, 0, sizeof(usage));
    if (clock_gettime(CLOCK_MONOTONIC, &startTime) != 0) {
        snprintf(errorBuffer, errorBufferSize, "clock_gettime failed: %s", strerror(errno));
        return 0;
    }
    childPid = fork();
    if (childPid < 0) {
        snprintf(errorBuffer, errorBufferSize, "fork failed: %s", strerror(errno));
        return 0;
    }
    if (childPid == 0) {
        int nullFd = open("/dev/null", O_WRONLY);
        if (setpgid(0, 0) != 0) {
            _exit(125);
        }
        if (workingDirectory != NULL && workingDirectory[0] != '\0' && chdir(workingDirectory) != 0) {
            _exit(127);
        }
        if (nullFd >= 0) {
            dup2(nullFd, STDOUT_FILENO);
            dup2(nullFd, STDERR_FILENO);
            if (nullFd > STDERR_FILENO) close(nullFd);
        }
        execvp(command[0], command);
        _exit(errno == ENOENT ? 127 : 126);
    }
    if (setpgid(childPid, childPid) != 0 && errno != EACCES && errno != ESRCH) {
        kill(childPid, SIGKILL);
        waitpid(childPid, NULL, 0);
        snprintf(errorBuffer, errorBufferSize, "failed to isolate process group: %s", strerror(errno));
        return 0;
    }
    deadlineMs = (uint64_t)startTime.tv_sec * 1000U + (uint64_t)startTime.tv_nsec / 1000000U +
                 (uint64_t)processTimeoutMs;
    while (1) {
        pid_t waitResult = wait4(childPid, &status, WNOHANG, &usage);
        struct timespec now;
        uint64_t nowMs;
        if (waitResult == childPid) {
            break;
        }
        if (waitResult < 0 && errno != EINTR) {
            snprintf(errorBuffer, errorBufferSize, "wait4 failed: %s", strerror(errno));
            kill(-childPid, SIGKILL);
            while (waitpid(childPid, NULL, 0) < 0 && errno == EINTR) {
            }
            return 0;
        }
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            snprintf(errorBuffer, errorBufferSize, "clock_gettime failed: %s", strerror(errno));
            kill(-childPid, SIGKILL);
            waitpid(childPid, NULL, 0);
            return 0;
        }
        nowMs = (uint64_t)now.tv_sec * 1000U + (uint64_t)now.tv_nsec / 1000000U;
        if (nowMs >= deadlineMs) {
            kill(-childPid, SIGKILL);
            while (waitpid(childPid, NULL, 0) < 0 && errno == EINTR) {
            }
            snprintf(errorBuffer, errorBufferSize, "process timeout after %" PRIu32 " ms", processTimeoutMs);
            return 0;
        }
        {
            struct timespec delay = {0, 1000000L};
            nanosleep(&delay, NULL);
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &endTime) != 0) {
        snprintf(errorBuffer, errorBufferSize, "clock_gettime failed: %s", strerror(errno));
        return 0;
    }
    sample->wallMs = zr_perf_process_timespec_diff_ms(&startTime, &endTime);
#if defined(__APPLE__)
    sample->peakWorkingSetBytes = (uint64_t)usage.ru_maxrss;
#else
    sample->peakWorkingSetBytes = (uint64_t)usage.ru_maxrss * 1024ULL;
#endif
    sample->exitCode = WIFEXITED(status) ? WEXITSTATUS(status)
                                         : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1);
    return 1;
}
#endif

int ZrPerfProcess_RunAggregate(const char *workingDirectory,
                               char *const *command,
                               uint32_t repetitions,
                               uint32_t processTimeoutMs,
                               SZrPerfRunSample *sample,
                               char *errorBuffer,
                               size_t errorBufferSize) {
    uint32_t repetition;

    if (command == NULL || command[0] == NULL || sample == NULL || repetitions == 0U || processTimeoutMs == 0U ||
        repetitions > ZR_PERF_MAX_REPETITIONS) {
        if (errorBuffer != NULL && errorBufferSize > 0U) {
            snprintf(errorBuffer, errorBufferSize, "invalid process repetitions");
        }
        return 0;
    }
    memset(sample, 0, sizeof(*sample));
    for (repetition = 0U; repetition < repetitions; repetition++) {
        SZrPerfRunSample repetitionSample;
        if (!zr_perf_process_run_once(workingDirectory,
                                      command,
                                      processTimeoutMs,
                                      &repetitionSample,
                                      errorBuffer,
                                      errorBufferSize)) {
            return 0;
        }
        if (repetitionSample.exitCode != 0) {
            *sample = repetitionSample;
            return 1;
        }
        sample->aggregateWallMs += repetitionSample.wallMs;
        if (repetitionSample.peakWorkingSetBytes > sample->peakWorkingSetBytes) {
            sample->peakWorkingSetBytes = repetitionSample.peakWorkingSetBytes;
        }
    }
    sample->wallMs = sample->aggregateWallMs / (double)repetitions;
    return 1;
}
