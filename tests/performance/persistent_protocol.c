#if !defined(_WIN32)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "persistent_protocol.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZR_PERF_PROTOCOL_MAX_LINE 512U

static void zr_perf_protocol_error(char *buffer, size_t bufferSize, const char *message) {
    if (buffer != NULL && bufferSize > 0U) {
        snprintf(buffer, bufferSize, "%s", message != NULL ? message : "persistent protocol failure");
    }
}

static int zr_perf_protocol_is_decimal(const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;

    if (cursor == NULL || *cursor == '\0') {
        return 0;
    }
    if (*cursor == '-') {
        cursor++;
    }
    if (*cursor == '\0') {
        return 0;
    }
    while (*cursor != '\0') {
        if (!isdigit(*cursor)) {
            return 0;
        }
        cursor++;
    }
    return 1;
}

static int zr_perf_protocol_validate_ascii_line(const char *line) {
    const unsigned char *cursor = (const unsigned char *)line;

    if (cursor == NULL || *cursor == '\0') {
        return 0;
    }
    while (*cursor != '\0') {
        if (*cursor < 0x20U || *cursor > 0x7eU) {
            return 0;
        }
        cursor++;
    }
    return 1;
}

static int zr_perf_protocol_validate_ready(const char *line, const char *expectedContract) {
    const char prefix[] = "READY ";
    const size_t prefixLength = sizeof(prefix) - 1U;

    return line != NULL && expectedContract != NULL && strncmp(line, prefix, prefixLength) == 0 &&
           strcmp(line + prefixLength, expectedContract) == 0;
}

static int zr_perf_protocol_validate_response(const char *line,
                                              int expectedIndex,
                                              const char *expectedChecksum,
                                              char *errorBuffer,
                                              size_t errorBufferSize) {
    const char *cursor;
    const char *value;
    int responseIndex = 0;
    int isError = 0;

    if (line == NULL) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "malformed persistent response");
        return 0;
    }
    if (strncmp(line, "DONE ", 5U) == 0) {
        cursor = line + 5U;
    } else if (strncmp(line, "ERROR ", 6U) == 0) {
        cursor = line + 6U;
        isError = 1;
    } else {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "malformed persistent response");
        return 0;
    }
    if (*cursor < '1' || *cursor > '9') {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "malformed persistent response index");
        return 0;
    }
    while (*cursor >= '0' && *cursor <= '9') {
        if (responseIndex > (INT_MAX - (*cursor - '0')) / 10) {
            zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent response index overflow");
            return 0;
        }
        responseIndex = responseIndex * 10 + (*cursor - '0');
        cursor++;
    }
    if (*cursor != ' ' || cursor[1] == '\0') {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "malformed persistent response");
        return 0;
    }
    value = cursor + 1;
    for (cursor = value; *cursor != '\0'; cursor++) {
        const unsigned char character = (unsigned char)*cursor;
        if (character == ' ' || character == '\t' || character < 0x21U || character > 0x7eU) {
            zr_perf_protocol_error(errorBuffer, errorBufferSize, "malformed persistent response value");
            return 0;
        }
    }
    if (isError) {
        snprintf(errorBuffer,
                 errorBufferSize,
                 "persistent child ERROR index=%d code=%s",
                 responseIndex,
                 value);
        return 0;
    }
    if (!zr_perf_protocol_is_decimal(value)) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "malformed persistent DONE response");
        return 0;
    }
    if (responseIndex != expectedIndex) {
        snprintf(errorBuffer,
                 errorBufferSize,
                 "persistent response index mismatch: expected %d, got %d",
                 expectedIndex,
                 responseIndex);
        return 0;
    }
    if (strcmp(value, expectedChecksum) != 0) {
        snprintf(errorBuffer,
                 errorBufferSize,
                 "persistent checksum mismatch: expected %s, got %s",
                 expectedChecksum,
                 value);
        return 0;
    }
    return 1;
}

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

typedef struct SZrPerfPersistentPlatformSession {
    HANDLE process;
    HANDLE thread;
    HANDLE inputWrite;
    HANDLE outputRead;
    HANDLE job;
    DWORD processId;
} SZrPerfPersistentPlatformSession;

static uint64_t zr_perf_protocol_now_ms(void) {
    return (uint64_t)GetTickCount64();
}

static double zr_perf_protocol_elapsed_ms(uint64_t startMs, uint64_t endMs) {
    return (double)(endMs - startMs) / 1000000.0;
}

static uint64_t zr_perf_protocol_now_high_resolution(void) {
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart / frequency.QuadPart) * 1000000000ULL +
           (uint64_t)(((counter.QuadPart % frequency.QuadPart) * 1000000000ULL) / frequency.QuadPart);
}

static int zr_perf_windows_append(char *buffer, size_t capacity, size_t *length, char value) {
    if (*length + 1U >= capacity) {
        return 0;
    }
    buffer[(*length)++] = value;
    buffer[*length] = '\0';
    return 1;
}

static int zr_perf_windows_append_arg(char *buffer, size_t capacity, size_t *length, const char *argument) {
    const char *cursor;
    size_t slashCount = 0U;
    int needsQuotes = argument == NULL || argument[0] == '\0';

    for (cursor = argument; !needsQuotes && cursor != NULL && *cursor != '\0'; cursor++) {
        needsQuotes = *cursor == ' ' || *cursor == '\t' || *cursor == '"';
    }
    if (!needsQuotes) {
        while (*argument != '\0') {
            if (!zr_perf_windows_append(buffer, capacity, length, *argument++)) {
                return 0;
            }
        }
        return 1;
    }
    if (!zr_perf_windows_append(buffer, capacity, length, '"')) {
        return 0;
    }
    for (cursor = argument != NULL ? argument : ""; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            slashCount++;
            continue;
        }
        if (*cursor == '"') {
            size_t count;
            for (count = 0U; count < slashCount * 2U + 1U; count++) {
                if (!zr_perf_windows_append(buffer, capacity, length, '\\')) {
                    return 0;
                }
            }
            slashCount = 0U;
        } else {
            size_t count;
            for (count = 0U; count < slashCount; count++) {
                if (!zr_perf_windows_append(buffer, capacity, length, '\\')) {
                    return 0;
                }
            }
            slashCount = 0U;
        }
        if (!zr_perf_windows_append(buffer, capacity, length, *cursor)) {
            return 0;
        }
    }
    while (slashCount-- > 0U) {
        if (!zr_perf_windows_append(buffer, capacity, length, '\\') ||
            !zr_perf_windows_append(buffer, capacity, length, '\\')) {
            return 0;
        }
    }
    return zr_perf_windows_append(buffer, capacity, length, '"');
}

static char *zr_perf_windows_command_line(char *const *command) {
    size_t capacity = 1U;
    size_t length = 0U;
    char *line;
    int index;

    for (index = 0; command[index] != NULL; index++) {
        capacity += strlen(command[index]) * 2U + 4U;
    }
    line = (char *)malloc(capacity);
    if (line == NULL) {
        return NULL;
    }
    line[0] = '\0';
    for (index = 0; command[index] != NULL; index++) {
        if ((index > 0 && !zr_perf_windows_append(line, capacity, &length, ' ')) ||
            !zr_perf_windows_append_arg(line, capacity, &length, command[index])) {
            free(line);
            return NULL;
        }
    }
    return line;
}

static void zr_perf_protocol_platform_cleanup(SZrPerfPersistentPlatformSession *session, int terminate) {
    if (session == NULL) {
        return;
    }
    if (terminate && session->process != NULL) {
        TerminateProcess(session->process, 125U);
        WaitForSingleObject(session->process, 1000U);
    }
    if (session->inputWrite != NULL) {
        CloseHandle(session->inputWrite);
    }
    if (session->outputRead != NULL) {
        CloseHandle(session->outputRead);
    }
    if (session->thread != NULL) {
        CloseHandle(session->thread);
    }
    if (session->process != NULL) {
        CloseHandle(session->process);
    }
    if (session->job != NULL) {
        CloseHandle(session->job);
    }
    memset(session, 0, sizeof(*session));
}

static int zr_perf_protocol_platform_start(const SZrPerfPersistentOptions *options,
                                           SZrPerfPersistentPlatformSession *session,
                                           char *errorBuffer,
                                           size_t errorBufferSize) {
    SECURITY_ATTRIBUTES security;
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobLimits;
    HANDLE childInputRead = NULL;
    HANDLE childOutputWrite = NULL;
    HANDLE nullHandle = INVALID_HANDLE_VALUE;
    char *commandLine = NULL;

    memset(session, 0, sizeof(*session));
    memset(&security, 0, sizeof(security));
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    memset(&jobLimits, 0, sizeof(jobLimits));
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    if (!CreatePipe(&childInputRead, &session->inputWrite, &security, 0U) ||
        !CreatePipe(&session->outputRead, &childOutputWrite, &security, 0U) ||
        !SetHandleInformation(session->inputWrite, HANDLE_FLAG_INHERIT, 0U) ||
        !SetHandleInformation(session->outputRead, HANDLE_FLAG_INHERIT, 0U)) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "failed to create persistent child pipes");
        goto fail;
    }
    nullHandle = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    commandLine = zr_perf_windows_command_line(options->command);
    if (nullHandle == INVALID_HANDLE_VALUE || commandLine == NULL) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "failed to prepare persistent child command");
        goto fail;
    }
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = childInputRead;
    startup.hStdOutput = childOutputWrite;
    startup.hStdError = nullHandle;
    if (!CreateProcessA(options->command[0], commandLine, NULL, NULL, TRUE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL,
                        options->workingDirectory != NULL && options->workingDirectory[0] != '\0'
                                ? options->workingDirectory
                                : NULL,
                        &startup, &process)) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "failed to start persistent child");
        goto fail;
    }
    session->process = process.hProcess;
    session->thread = process.hThread;
    session->processId = process.dwProcessId;
    session->job = CreateJobObjectA(NULL, NULL);
    jobLimits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (session->job == NULL ||
        !SetInformationJobObject(session->job, JobObjectExtendedLimitInformation,
                                 &jobLimits, sizeof(jobLimits)) ||
        !AssignProcessToJobObject(session->job, session->process) || ResumeThread(session->thread) == (DWORD)-1) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize,
                               "failed to contain persistent child in cleanup job");
        CloseHandle(childInputRead);
        CloseHandle(childOutputWrite);
        CloseHandle(nullHandle);
        free(commandLine);
        zr_perf_protocol_platform_cleanup(session, 1);
        return 0;
    }
    CloseHandle(childInputRead);
    CloseHandle(childOutputWrite);
    CloseHandle(nullHandle);
    free(commandLine);
    return 1;

fail:
    if (childInputRead != NULL) CloseHandle(childInputRead);
    if (childOutputWrite != NULL) CloseHandle(childOutputWrite);
    if (nullHandle != INVALID_HANDLE_VALUE) CloseHandle(nullHandle);
    free(commandLine);
    zr_perf_protocol_platform_cleanup(session, 1);
    return 0;
}

static int zr_perf_protocol_platform_write(SZrPerfPersistentPlatformSession *session,
                                           const char *text,
                                           uint32_t timeoutMs,
                                           char *errorBuffer,
                                           size_t errorBufferSize) {
    DWORD written = 0U;
    const uint64_t deadline = zr_perf_protocol_now_ms() + timeoutMs;
    if (WaitForSingleObject(session->process, 0U) == WAIT_OBJECT_0) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent child exited before request");
        return 0;
    }
    /* Requests are under 64 bytes and only one request is outstanding. The
       anonymous pipe is therefore empty before each synchronous write. */
    if (!WriteFile(session->inputWrite, text, (DWORD)strlen(text), &written, NULL) ||
        written != (DWORD)strlen(text)) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "failed to write persistent request");
        return 0;
    }
    if (zr_perf_protocol_now_ms() > deadline) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent request write timeout");
        return 0;
    }
    return 1;
}

static int zr_perf_protocol_platform_read_line(SZrPerfPersistentPlatformSession *session,
                                               uint32_t timeoutMs,
                                               char *line,
                                               size_t lineCapacity,
                                               char *errorBuffer,
                                               size_t errorBufferSize) {
    const uint64_t deadline = zr_perf_protocol_now_ms() + timeoutMs;
    size_t length = 0U;

    while (zr_perf_protocol_now_ms() <= deadline) {
        DWORD available = 0U;
        DWORD readCount = 0U;
        char value;
        if (PeekNamedPipe(session->outputRead, NULL, 0U, NULL, &available, NULL) && available > 0U) {
            if (!ReadFile(session->outputRead, &value, 1U, &readCount, NULL) || readCount != 1U) {
                zr_perf_protocol_error(errorBuffer, errorBufferSize, "failed to read persistent response");
                return 0;
            }
            if (value == '\n') {
                if (length > 0U && line[length - 1U] == '\r') length--;
                line[length] = '\0';
                if (!zr_perf_protocol_validate_ascii_line(line)) {
                    zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent response is not strict ASCII");
                    return 0;
                }
                return 1;
            }
            if (length + 1U >= lineCapacity) {
                zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent response line is overlong");
                return 0;
            }
            line[length++] = value;
            continue;
        }
        if (WaitForSingleObject(session->process, 0U) == WAIT_OBJECT_0) {
            zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent child exited before response");
            return 0;
        }
        Sleep(1U);
    }
    zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent response timeout");
    return 0;
}

static int zr_perf_protocol_platform_finish(SZrPerfPersistentPlatformSession *session,
                                            uint32_t timeoutMs,
                                            SZrPerfPersistentSessionInfo *info,
                                            char *errorBuffer,
                                            size_t errorBufferSize) {
    PROCESS_MEMORY_COUNTERS counters;
    DWORD exitCode = 0U;
    memset(&counters, 0, sizeof(counters));
    counters.cb = sizeof(counters);
    if (WaitForSingleObject(session->process, timeoutMs) != WAIT_OBJECT_0) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent STOP timeout");
        return 0;
    }
    GetExitCodeProcess(session->process, &exitCode);
    GetProcessMemoryInfo(session->process, &counters, sizeof(counters));
    info->processId = (uint64_t)session->processId;
    info->peakWorkingSetBytes = (uint64_t)counters.PeakWorkingSetSize;
    info->exitCode = (int)exitCode;
    if (exitCode != 0U) {
        snprintf(errorBuffer, errorBufferSize, "persistent child exited nonzero after STOP: %lu",
                 (unsigned long)exitCode);
        return 0;
    }
    return 1;
}

#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct SZrPerfPersistentPlatformSession {
    pid_t processId;
    pid_t processGroupId;
    int inputFd;
    int outputFd;
    struct sigaction oldSigpipeAction;
    int sigpipeActionChanged;
} SZrPerfPersistentPlatformSession;

static uint64_t zr_perf_protocol_now_ms(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000ULL + (uint64_t)now.tv_nsec / 1000000ULL;
}

static double zr_perf_protocol_elapsed_ms(uint64_t startMs, uint64_t endMs) {
    return (double)(endMs - startMs) / 1000000.0;
}

static uint64_t zr_perf_protocol_now_high_resolution(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
}

static void zr_perf_protocol_platform_cleanup(SZrPerfPersistentPlatformSession *session, int terminate) {
    int status;
    if (session == NULL) return;
    if (session->inputFd >= 0) close(session->inputFd);
    if (session->outputFd >= 0) close(session->outputFd);
    if (terminate && session->processGroupId > 0) {
        kill(-session->processGroupId, SIGKILL);
    }
    if (terminate && session->processId > 0) {
        while (waitpid(session->processId, &status, 0) < 0 && errno == EINTR) {
        }
    }
    if (session->sigpipeActionChanged) {
        sigaction(SIGPIPE, &session->oldSigpipeAction, NULL);
    }
    session->inputFd = -1;
    session->outputFd = -1;
    session->processId = -1;
    session->processGroupId = -1;
    session->sigpipeActionChanged = 0;
}

static int zr_perf_protocol_platform_start(const SZrPerfPersistentOptions *options,
                                           SZrPerfPersistentPlatformSession *session,
                                           char *errorBuffer,
                                           size_t errorBufferSize) {
    int inputPipe[2];
    int outputPipe[2];
    pid_t child;
    struct sigaction ignoreSigpipe;
    session->inputFd = -1;
    session->outputFd = -1;
    session->processId = -1;
    session->processGroupId = -1;
    session->sigpipeActionChanged = 0;
    memset(&ignoreSigpipe, 0, sizeof(ignoreSigpipe));
    ignoreSigpipe.sa_handler = SIG_IGN;
    sigemptyset(&ignoreSigpipe.sa_mask);
    if (sigaction(SIGPIPE, &ignoreSigpipe, &session->oldSigpipeAction) != 0) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "failed to ignore SIGPIPE for persistent transport");
        return 0;
    }
    session->sigpipeActionChanged = 1;
    if (pipe(inputPipe) != 0) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "failed to create persistent child pipes");
        zr_perf_protocol_platform_cleanup(session, 0);
        return 0;
    }
    if (pipe(outputPipe) != 0) {
        close(inputPipe[0]);
        close(inputPipe[1]);
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "failed to create persistent child pipes");
        zr_perf_protocol_platform_cleanup(session, 0);
        return 0;
    }
    child = fork();
    if (child < 0) {
        close(inputPipe[0]); close(inputPipe[1]); close(outputPipe[0]); close(outputPipe[1]);
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "failed to fork persistent child");
        zr_perf_protocol_platform_cleanup(session, 0);
        return 0;
    }
    if (child == 0) {
        int nullFd = open("/dev/null", O_WRONLY);
        sigaction(SIGPIPE, &session->oldSigpipeAction, NULL);
        if (setpgid(0, 0) != 0) {
            _exit(125);
        }
        dup2(inputPipe[0], STDIN_FILENO);
        dup2(outputPipe[1], STDOUT_FILENO);
        if (nullFd >= 0) dup2(nullFd, STDERR_FILENO);
        close(inputPipe[0]); close(inputPipe[1]); close(outputPipe[0]); close(outputPipe[1]);
        if (nullFd > STDERR_FILENO) close(nullFd);
        if (options->workingDirectory != NULL && options->workingDirectory[0] != '\0' &&
            chdir(options->workingDirectory) != 0) {
            _exit(127);
        }
        execvp(options->command[0], options->command);
        _exit(errno == ENOENT ? 127 : 126);
    }
    close(inputPipe[0]);
    close(outputPipe[1]);
    if (setpgid(child, child) != 0 && errno != EACCES && errno != ESRCH) {
        close(inputPipe[1]);
        close(outputPipe[0]);
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "failed to isolate persistent child process group");
        zr_perf_protocol_platform_cleanup(session, 0);
        return 0;
    }
    session->processId = child;
    session->processGroupId = child;
    session->inputFd = inputPipe[1];
    session->outputFd = outputPipe[0];
    return 1;
}

static int zr_perf_protocol_poll(int fd, short events, uint64_t deadline, pid_t processId,
                                 char *errorBuffer, size_t errorBufferSize) {
    while (1) {
        struct pollfd descriptor;
        const uint64_t now = zr_perf_protocol_now_ms();
        const uint64_t remaining64 = now < deadline ? deadline - now : 0U;
        int remaining = remaining64 > (uint64_t)INT_MAX ? INT_MAX : (int)remaining64;
        int result;
        descriptor.fd = fd;
        descriptor.events = events;
        descriptor.revents = 0;
        result = poll(&descriptor, 1, remaining > 0 ? remaining : 0);
        if (result > 0 && (descriptor.revents & events) != 0) return 1;
        if (result > 0 && (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent child exited before response");
            return 0;
        }
        if (result < 0 && errno == EINTR) continue;
        if (result < 0) {
            zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent protocol poll failed");
            return 0;
        }
        if (waitpid(processId, NULL, WNOHANG) == processId) {
            zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent child exited before response");
            return 0;
        }
        if (now >= deadline || result == 0) break;
    }
    zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent response timeout");
    return 0;
}

static int zr_perf_protocol_platform_write(SZrPerfPersistentPlatformSession *session,
                                           const char *text,
                                           uint32_t timeoutMs,
                                           char *errorBuffer,
                                           size_t errorBufferSize) {
    const uint64_t deadline = zr_perf_protocol_now_ms() + timeoutMs;
    size_t offset = 0U;
    size_t length = strlen(text);
    while (offset < length) {
        ssize_t written;
        if (!zr_perf_protocol_poll(session->inputFd, POLLOUT, deadline, session->processId,
                                   errorBuffer, errorBufferSize)) return 0;
        written = write(session->inputFd, text + offset, length - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) {
            zr_perf_protocol_error(errorBuffer, errorBufferSize, "failed to write persistent request");
            return 0;
        }
        offset += (size_t)written;
    }
    return 1;
}

static int zr_perf_protocol_platform_read_line(SZrPerfPersistentPlatformSession *session,
                                               uint32_t timeoutMs,
                                               char *line,
                                               size_t lineCapacity,
                                               char *errorBuffer,
                                               size_t errorBufferSize) {
    const uint64_t deadline = zr_perf_protocol_now_ms() + timeoutMs;
    size_t length = 0U;
    while (1) {
        char value;
        ssize_t readCount;
        if (!zr_perf_protocol_poll(session->outputFd, POLLIN, deadline, session->processId,
                                   errorBuffer, errorBufferSize)) return 0;
        readCount = read(session->outputFd, &value, 1U);
        if (readCount < 0 && errno == EINTR) continue;
        if (readCount <= 0) {
            zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent child exited before response");
            return 0;
        }
        if (value == '\n') {
            if (length > 0U && line[length - 1U] == '\r') length--;
            line[length] = '\0';
            if (!zr_perf_protocol_validate_ascii_line(line)) {
                zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent response is not strict ASCII");
                return 0;
            }
            return 1;
        }
        if (length + 1U >= lineCapacity) {
            zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent response line is overlong");
            return 0;
        }
        line[length++] = value;
    }
}

static int zr_perf_protocol_platform_finish(SZrPerfPersistentPlatformSession *session,
                                            uint32_t timeoutMs,
                                            SZrPerfPersistentSessionInfo *info,
                                            char *errorBuffer,
                                            size_t errorBufferSize) {
    const uint64_t deadline = zr_perf_protocol_now_ms() + timeoutMs;
    int status = 0;
    struct rusage usage;
    memset(&usage, 0, sizeof(usage));
    while (zr_perf_protocol_now_ms() <= deadline) {
        pid_t result = wait4(session->processId, &status, WNOHANG, &usage);
        if (result == session->processId) {
            info->processId = (uint64_t)session->processId;
#if defined(__APPLE__)
            info->peakWorkingSetBytes = (uint64_t)usage.ru_maxrss;
#else
            info->peakWorkingSetBytes = (uint64_t)usage.ru_maxrss * 1024ULL;
#endif
            info->exitCode = WIFEXITED(status) ? WEXITSTATUS(status)
                                               : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1);
            if (info->exitCode != 0) {
                snprintf(errorBuffer, errorBufferSize,
                         "persistent child exited nonzero after STOP: %d", info->exitCode);
                return 0;
            }
            session->processId = -1;
            if (session->processGroupId > 0) {
                kill(-session->processGroupId, SIGKILL);
            }
            return 1;
        }
        if (result < 0 && errno != EINTR) break;
        {
            struct timespec delay = {0, 1000000L};
            nanosleep(&delay, NULL);
        }
    }
    zr_perf_protocol_error(errorBuffer, errorBufferSize, "persistent STOP timeout");
    return 0;
}
#endif

typedef struct SZrPerfPersistentSessionImplementation {
    SZrPerfPersistentPlatformSession platform;
    SZrPerfPersistentOptions options;
} SZrPerfPersistentSessionImplementation;

static int zr_perf_protocol_send_request(SZrPerfPersistentPlatformSession *session,
                                         const SZrPerfPersistentOptions *options,
                                         const char *kind,
                                         int index,
                                         uint32_t repetitions,
                                         SZrPerfPersistentSample *sample,
                                         char *errorBuffer,
                                         size_t errorBufferSize) {
    char request[64];
    char response[ZR_PERF_PROTOCOL_MAX_LINE];
    uint64_t startTime;
    uint64_t endTime;

    snprintf(request, sizeof(request), "%s %d %" PRIu32 "\n", kind, index, repetitions);
    startTime = zr_perf_protocol_now_high_resolution();
    if (!zr_perf_protocol_platform_write(session, request, options->requestTimeoutMs,
                                         errorBuffer, errorBufferSize) ||
        !zr_perf_protocol_platform_read_line(session, options->requestTimeoutMs, response,
                                             sizeof(response), errorBuffer, errorBufferSize)) {
        return 0;
    }
    endTime = zr_perf_protocol_now_high_resolution();
    if (!zr_perf_protocol_validate_response(response, index, options->expectedChecksum,
                                            errorBuffer, errorBufferSize)) {
        return 0;
    }
    if (sample != NULL) {
        sample->wallMs = zr_perf_protocol_elapsed_ms(startTime, endTime);
#if defined(_WIN32)
        sample->processId = (uint64_t)session->processId;
#else
        sample->processId = (uint64_t)session->processId;
#endif
    }
    return 1;
}

static int zr_perf_protocol_validate_options(const SZrPerfPersistentOptions *options) {
    return options != NULL && options->command != NULL && options->command[0] != NULL &&
           options->checksumContract != NULL && options->checksumContract[0] != '\0' &&
           options->expectedChecksum != NULL && zr_perf_protocol_is_decimal(options->expectedChecksum) &&
           options->readyTimeoutMs != 0U && options->requestTimeoutMs != 0U && options->stopTimeoutMs != 0U;
}

int ZrPerfPersistentSession_Start(SZrPerfPersistentSession *session,
                                  const SZrPerfPersistentOptions *options,
                                  char *errorBuffer,
                                  size_t errorBufferSize) {
    SZrPerfPersistentSessionImplementation *implementation;
    char readyLine[ZR_PERF_PROTOCOL_MAX_LINE];

    if (session == NULL || session->implementation != NULL || !zr_perf_protocol_validate_options(options)) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "invalid persistent protocol options");
        return 0;
    }
    implementation = (SZrPerfPersistentSessionImplementation *)calloc(1U, sizeof(*implementation));
    if (implementation == NULL) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "failed to allocate persistent session");
        return 0;
    }
    implementation->options = *options;
    if (!zr_perf_protocol_platform_start(options, &implementation->platform, errorBuffer, errorBufferSize) ||
        !zr_perf_protocol_platform_read_line(&implementation->platform,
                                             options->readyTimeoutMs,
                                             readyLine,
                                             sizeof(readyLine),
                                             errorBuffer,
                                             errorBufferSize)) {
        zr_perf_protocol_platform_cleanup(&implementation->platform, 1);
        free(implementation);
        return 0;
    }
    if (!zr_perf_protocol_validate_ready(readyLine, options->checksumContract)) {
        snprintf(errorBuffer, errorBufferSize, "persistent READY contract mismatch: %s", readyLine);
        zr_perf_protocol_platform_cleanup(&implementation->platform, 1);
        free(implementation);
        return 0;
    }
    session->implementation = implementation;
    return 1;
}

int ZrPerfPersistentSession_Request(SZrPerfPersistentSession *session,
                                    int isWarmup,
                                    int index,
                                    uint32_t repetitions,
                                    SZrPerfPersistentSample *sample,
                                    char *errorBuffer,
                                    size_t errorBufferSize) {
    SZrPerfPersistentSessionImplementation *implementation;

    if (session == NULL || session->implementation == NULL || (isWarmup != 0 && isWarmup != 1) || index <= 0 ||
        repetitions == 0U || repetitions > ZR_PERF_MAX_REPETITIONS || (!isWarmup && sample == NULL)) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "invalid persistent request");
        return 0;
    }
    implementation = (SZrPerfPersistentSessionImplementation *)session->implementation;
    return zr_perf_protocol_send_request(&implementation->platform,
                                         &implementation->options,
                                         isWarmup ? "WARMUP" : "RUN",
                                         index,
                                         repetitions,
                                         sample,
                                         errorBuffer,
                                         errorBufferSize);
}

int ZrPerfPersistentSession_Finish(SZrPerfPersistentSession *session,
                                   SZrPerfPersistentSessionInfo *sessionInfo,
                                   char *errorBuffer,
                                   size_t errorBufferSize) {
    SZrPerfPersistentSessionImplementation *implementation;
    int succeeded;

    if (session == NULL || session->implementation == NULL || sessionInfo == NULL) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "invalid persistent session finish");
        return 0;
    }
    implementation = (SZrPerfPersistentSessionImplementation *)session->implementation;
    memset(sessionInfo, 0, sizeof(*sessionInfo));
    succeeded = zr_perf_protocol_platform_write(&implementation->platform,
                                                "STOP\n",
                                                implementation->options.stopTimeoutMs,
                                                errorBuffer,
                                                errorBufferSize) &&
                zr_perf_protocol_platform_finish(&implementation->platform,
                                                 implementation->options.stopTimeoutMs,
                                                 sessionInfo,
                                                 errorBuffer,
                                                 errorBufferSize);
    zr_perf_protocol_platform_cleanup(&implementation->platform, succeeded ? 0 : 1);
    free(implementation);
    session->implementation = NULL;
    return succeeded;
}

void ZrPerfPersistentSession_Abort(SZrPerfPersistentSession *session) {
    SZrPerfPersistentSessionImplementation *implementation;

    if (session == NULL || session->implementation == NULL) {
        return;
    }
    implementation = (SZrPerfPersistentSessionImplementation *)session->implementation;
    zr_perf_protocol_platform_cleanup(&implementation->platform, 1);
    free(implementation);
    session->implementation = NULL;
}

int ZrPerfPersistent_Run(const SZrPerfPersistentOptions *options,
                         int warmupCount,
                         int iterationCount,
                         SZrPerfPersistentSample *samples,
                         SZrPerfPersistentSessionInfo *sessionInfo,
                         char *errorBuffer,
                         size_t errorBufferSize) {
    SZrPerfPersistentSession session;
    int index;

    memset(&session, 0, sizeof(session));
    if (!zr_perf_protocol_validate_options(options) || warmupCount < 0 || iterationCount <= 0 || samples == NULL ||
        sessionInfo == NULL) {
        zr_perf_protocol_error(errorBuffer, errorBufferSize, "invalid persistent protocol options");
        return 0;
    }
    memset(sessionInfo, 0, sizeof(*sessionInfo));
    if (!ZrPerfPersistentSession_Start(&session, options, errorBuffer, errorBufferSize)) {
        return 0;
    }
    for (index = 0; index < warmupCount; index++) {
        if (!ZrPerfPersistentSession_Request(&session, 1, index + 1, 1U, NULL, errorBuffer, errorBufferSize)) {
            ZrPerfPersistentSession_Abort(&session);
            return 0;
        }
    }
    for (index = 0; index < iterationCount; index++) {
        if (!ZrPerfPersistentSession_Request(&session, 0, index + 1, 1U, &samples[index],
                                             errorBuffer, errorBufferSize)) {
            ZrPerfPersistentSession_Abort(&session);
            return 0;
        }
    }
    return ZrPerfPersistentSession_Finish(&session, sessionInfo, errorBuffer, errorBufferSize);
}
