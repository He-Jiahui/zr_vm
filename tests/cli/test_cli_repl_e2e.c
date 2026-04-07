#if !defined(_WIN32)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "path_support.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

#define CLI_REPL_E2E_PATH_CAPACITY 1024
#define CLI_REPL_E2E_ERROR_CAPACITY 512
#define CLI_REPL_E2E_IO_CHUNK_SIZE 4096
#define CLI_REPL_E2E_POLL_MS 20
#define CLI_REPL_E2E_OUTPUT_TIMEOUT_MS 1500
#define CLI_REPL_E2E_EXIT_TIMEOUT_MS 5000

typedef struct ZrCliReplE2eProcess {
#if defined(_WIN32)
    HANDLE processHandle;
    HANDLE threadHandle;
    HANDLE stdinWrite;
    HANDLE stdoutRead;
#else
    pid_t pid;
    int stdinFd;
    int stdoutFd;
#endif
    char *output;
    size_t outputLength;
    size_t outputCapacity;
    int exitCode;
    int hasExited;
} ZrCliReplE2eProcess;

static int cli_fail(const char *testName, const char *format, ...);
static void cli_normalize_separators(char *path);
static unsigned long long cli_now_ms(void);
static void cli_sleep_ms(unsigned int milliseconds);
static int cli_process_append_output(ZrCliReplE2eProcess *process, const char *text, size_t length);
static const char *cli_process_output(const ZrCliReplE2eProcess *process);
#if defined(_WIN32)
static char *cli_windows_build_command_line(const char *const *command);
#endif

static int cli_process_start(ZrCliReplE2eProcess *process,
                             const char *workingDirectory,
                             const char *const *command,
                             char *errorBuffer,
                             size_t errorBufferSize) {
    if (process == NULL || command == NULL || command[0] == NULL) {
        snprintf(errorBuffer, errorBufferSize, "process start requires command");
        return 0;
    }

    memset(process, 0, sizeof(*process));
#if !defined(_WIN32)
    process->stdinFd = -1;
    process->stdoutFd = -1;
#endif

#if defined(_WIN32)
    SECURITY_ATTRIBUTES securityAttributes;
    STARTUPINFOA startupInfo;
    PROCESS_INFORMATION processInfo;
    HANDLE stdoutWrite = NULL;
    HANDLE stdinRead = NULL;
    char *commandLine = NULL;

    memset(&securityAttributes, 0, sizeof(securityAttributes));
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));

    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    if (!CreatePipe(&process->stdoutRead, &stdoutWrite, &securityAttributes, 0)) {
        snprintf(errorBuffer, errorBufferSize, "CreatePipe stdout failed: %lu", (unsigned long)GetLastError());
        return 0;
    }
    if (!SetHandleInformation(process->stdoutRead, HANDLE_FLAG_INHERIT, 0)) {
        snprintf(errorBuffer, errorBufferSize, "SetHandleInformation stdout failed: %lu", (unsigned long)GetLastError());
        CloseHandle(stdoutWrite);
        CloseHandle(process->stdoutRead);
        process->stdoutRead = NULL;
        return 0;
    }

    if (!CreatePipe(&stdinRead, &process->stdinWrite, &securityAttributes, 0)) {
        snprintf(errorBuffer, errorBufferSize, "CreatePipe stdin failed: %lu", (unsigned long)GetLastError());
        CloseHandle(stdoutWrite);
        CloseHandle(process->stdoutRead);
        process->stdoutRead = NULL;
        return 0;
    }
    if (!SetHandleInformation(process->stdinWrite, HANDLE_FLAG_INHERIT, 0)) {
        snprintf(errorBuffer, errorBufferSize, "SetHandleInformation stdin failed: %lu", (unsigned long)GetLastError());
        CloseHandle(stdinRead);
        CloseHandle(process->stdinWrite);
        CloseHandle(stdoutWrite);
        CloseHandle(process->stdoutRead);
        process->stdinWrite = NULL;
        process->stdoutRead = NULL;
        return 0;
    }

    commandLine = cli_windows_build_command_line(command);
    if (commandLine == NULL) {
        snprintf(errorBuffer, errorBufferSize, "failed to allocate command line");
        CloseHandle(stdinRead);
        CloseHandle(process->stdinWrite);
        CloseHandle(stdoutWrite);
        CloseHandle(process->stdoutRead);
        process->stdinWrite = NULL;
        process->stdoutRead = NULL;
        return 0;
    }

    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = stdinRead;
    startupInfo.hStdOutput = stdoutWrite;
    startupInfo.hStdError = stdoutWrite;

    if (!CreateProcessA(command[0],
                        commandLine,
                        NULL,
                        NULL,
                        TRUE,
                        CREATE_NO_WINDOW,
                        NULL,
                        workingDirectory != NULL && workingDirectory[0] != '\0' ? workingDirectory : NULL,
                        &startupInfo,
                        &processInfo)) {
        snprintf(errorBuffer, errorBufferSize, "CreateProcess failed: %lu", (unsigned long)GetLastError());
        free(commandLine);
        CloseHandle(stdinRead);
        CloseHandle(process->stdinWrite);
        CloseHandle(stdoutWrite);
        CloseHandle(process->stdoutRead);
        process->stdinWrite = NULL;
        process->stdoutRead = NULL;
        return 0;
    }

    free(commandLine);
    CloseHandle(stdinRead);
    CloseHandle(stdoutWrite);
    process->processHandle = processInfo.hProcess;
    process->threadHandle = processInfo.hThread;
    return 1;
#else
    int stdinPipe[2];
    int stdoutPipe[2];
    int flags;

    if (pipe(stdinPipe) != 0) {
        snprintf(errorBuffer, errorBufferSize, "stdin pipe failed: %s", strerror(errno));
        return 0;
    }
    if (pipe(stdoutPipe) != 0) {
        snprintf(errorBuffer, errorBufferSize, "stdout pipe failed: %s", strerror(errno));
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        return 0;
    }

    process->pid = fork();
    if (process->pid < 0) {
        snprintf(errorBuffer, errorBufferSize, "fork failed: %s", strerror(errno));
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        process->pid = 0;
        return 0;
    }

    if (process->pid == 0) {
        char *const *execCommand = (char *const *)command;

        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        if (workingDirectory != NULL && workingDirectory[0] != '\0') {
            (void)chdir(workingDirectory);
        }
        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stdoutPipe[1], STDERR_FILENO);
        close(stdinPipe[0]);
        close(stdoutPipe[1]);
        execv(command[0], execCommand);
        _exit(127);
    }

    close(stdinPipe[0]);
    close(stdoutPipe[1]);
    process->stdinFd = stdinPipe[1];
    process->stdoutFd = stdoutPipe[0];
    flags = fcntl(process->stdoutFd, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(process->stdoutFd, F_SETFL, flags | O_NONBLOCK);
    }
    return 1;
#endif
}

static int cli_process_write_line(ZrCliReplE2eProcess *process, const char *line) {
    size_t lineLength;
    static const char newline[] = "\n";

    if (process == NULL || line == NULL) {
        return 0;
    }

    lineLength = strlen(line);
#if defined(_WIN32)
    {
        DWORD bytesWritten = 0;

        if (process->stdinWrite == NULL) {
            return 0;
        }
        if ((lineLength > 0 &&
             (!WriteFile(process->stdinWrite, line, (DWORD)lineLength, &bytesWritten, NULL) ||
              bytesWritten != (DWORD)lineLength)) ||
            !WriteFile(process->stdinWrite, newline, 1, &bytesWritten, NULL) ||
            bytesWritten != 1) {
            return 0;
        }
    }
    return 1;
#else
    ssize_t bytesWritten;

    if (process->stdinFd < 0) {
        return 0;
    }
    if (lineLength > 0) {
        bytesWritten = write(process->stdinFd, line, lineLength);
        if (bytesWritten < 0 || (size_t)bytesWritten != lineLength) {
            return 0;
        }
    }
    bytesWritten = write(process->stdinFd, newline, 1);
    return bytesWritten == 1 ? 1 : 0;
#endif
}

static int cli_process_update_exit_status(ZrCliReplE2eProcess *process) {
    if (process == NULL || process->hasExited) {
        return 1;
    }

#if defined(_WIN32)
    DWORD waitStatus;
    DWORD exitCode;

    waitStatus = WaitForSingleObject(process->processHandle, 0);
    if (waitStatus == WAIT_TIMEOUT) {
        return 1;
    }
    if (waitStatus != WAIT_OBJECT_0) {
        return 0;
    }
    if (!GetExitCodeProcess(process->processHandle, &exitCode)) {
        return 0;
    }
    process->exitCode = (int)exitCode;
    process->hasExited = 1;
    return 1;
#else
    int status = 0;
    pid_t waitResult = waitpid(process->pid, &status, WNOHANG);

    if (waitResult == 0) {
        return 1;
    }
    if (waitResult < 0) {
        return 0;
    }

    if (WIFEXITED(status)) {
        process->exitCode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        process->exitCode = 128 + WTERMSIG(status);
    } else {
        process->exitCode = status;
    }
    process->hasExited = 1;
    return 1;
#endif
}

static int cli_process_read_available(ZrCliReplE2eProcess *process) {
    char buffer[CLI_REPL_E2E_IO_CHUNK_SIZE];

    if (process == NULL) {
        return 0;
    }

#if defined(_WIN32)
    if (process->stdoutRead == NULL) {
        return 1;
    }

    for (;;) {
        DWORD bytesAvailable = 0;
        DWORD bytesRead = 0;

        if (!PeekNamedPipe(process->stdoutRead, NULL, 0, NULL, &bytesAvailable, NULL)) {
            DWORD errorCode = GetLastError();

            if (errorCode == ERROR_BROKEN_PIPE) {
                return 1;
            }
            return 0;
        }

        if (bytesAvailable == 0) {
            return 1;
        }

        if (!ReadFile(process->stdoutRead,
                      buffer,
                      bytesAvailable < sizeof(buffer) ? bytesAvailable : (DWORD)sizeof(buffer),
                      &bytesRead,
                      NULL)) {
            DWORD errorCode = GetLastError();

            if (errorCode == ERROR_BROKEN_PIPE) {
                return 1;
            }
            return 0;
        }

        if (bytesRead == 0) {
            return 1;
        }

        if (!cli_process_append_output(process, buffer, (size_t)bytesRead)) {
            return 0;
        }
    }
#else
    if (process->stdoutFd < 0) {
        return 1;
    }

    for (;;) {
        ssize_t bytesRead = read(process->stdoutFd, buffer, sizeof(buffer));

        if (bytesRead > 0) {
            if (!cli_process_append_output(process, buffer, (size_t)bytesRead)) {
                return 0;
            }
            continue;
        }

        if (bytesRead == 0) {
            return 1;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 1;
        }
        return 0;
    }
#endif
}

static int cli_process_wait_for_substring(ZrCliReplE2eProcess *process, const char *needle, unsigned int timeoutMs) {
    unsigned long long deadline;

    if (process == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }

    deadline = cli_now_ms() + (unsigned long long)timeoutMs;
    while (cli_now_ms() <= deadline) {
        if (!cli_process_update_exit_status(process) || !cli_process_read_available(process)) {
            return 0;
        }
        if (strstr(cli_process_output(process), needle) != NULL) {
            return 1;
        }
        if (process->hasExited) {
            break;
        }
        cli_sleep_ms(CLI_REPL_E2E_POLL_MS);
    }

    if (!cli_process_update_exit_status(process) || !cli_process_read_available(process)) {
        return 0;
    }
    return strstr(cli_process_output(process), needle) != NULL ? 1 : 0;
}

static int cli_process_wait_for_exit(ZrCliReplE2eProcess *process, unsigned int timeoutMs) {
    unsigned long long deadline;
    int drainPass;

    if (process == NULL) {
        return 0;
    }

    deadline = cli_now_ms() + (unsigned long long)timeoutMs;
    while (cli_now_ms() <= deadline) {
        if (!cli_process_update_exit_status(process) || !cli_process_read_available(process)) {
            return 0;
        }
        if (process->hasExited) {
            break;
        }
        cli_sleep_ms(CLI_REPL_E2E_POLL_MS);
    }

    if (!process->hasExited) {
        return 0;
    }

    for (drainPass = 0; drainPass < 8; drainPass++) {
        if (!cli_process_read_available(process)) {
            return 0;
        }
        cli_sleep_ms(CLI_REPL_E2E_POLL_MS);
    }
    return 1;
}

static void cli_process_terminate(ZrCliReplE2eProcess *process) {
    if (process == NULL || process->hasExited) {
        return;
    }

#if defined(_WIN32)
    if (process->processHandle != NULL) {
        (void)TerminateProcess(process->processHandle, 1);
        (void)WaitForSingleObject(process->processHandle, 1000);
        process->hasExited = 1;
        process->exitCode = 1;
    }
#else
    if (process->pid > 0) {
        (void)kill(process->pid, SIGKILL);
        (void)waitpid(process->pid, NULL, 0);
        process->hasExited = 1;
        process->exitCode = 1;
    }
#endif
}

static void cli_process_close(ZrCliReplE2eProcess *process) {
    if (process == NULL) {
        return;
    }

#if defined(_WIN32)
    if (process->stdinWrite != NULL) {
        CloseHandle(process->stdinWrite);
        process->stdinWrite = NULL;
    }
    if (process->threadHandle != NULL) {
        CloseHandle(process->threadHandle);
        process->threadHandle = NULL;
    }
    if (process->processHandle != NULL) {
        CloseHandle(process->processHandle);
        process->processHandle = NULL;
    }
    if (process->stdoutRead != NULL) {
        CloseHandle(process->stdoutRead);
        process->stdoutRead = NULL;
    }
#else
    if (process->stdinFd >= 0) {
        close(process->stdinFd);
        process->stdinFd = -1;
    }
    if (process->stdoutFd >= 0) {
        close(process->stdoutFd);
        process->stdoutFd = -1;
    }
#endif

    free(process->output);
    process->output = NULL;
    process->outputLength = 0;
    process->outputCapacity = 0;
}

static int cli_get_executable_directory(char *buffer, size_t bufferSize) {
#if defined(_WIN32)
    DWORD length = GetModuleFileNameA(NULL, buffer, (DWORD)bufferSize);
#else
    ssize_t length = readlink("/proc/self/exe", buffer, bufferSize - 1);
#endif
    char *lastSlash;

    if (buffer == NULL || bufferSize == 0) {
        return 0;
    }

#if defined(_WIN32)
    if (length == 0 || length >= bufferSize) {
        return 0;
    }
#else
    if (length <= 0 || (size_t)length >= bufferSize) {
        return 0;
    }
    buffer[length] = '\0';
#endif

    cli_normalize_separators(buffer);
    lastSlash = strrchr(buffer, '/');
    if (lastSlash == NULL) {
        return 0;
    }
    *lastSlash = '\0';
    return 1;
}

static int cli_build_cli_executable_path(char *buffer,
                                         size_t bufferSize,
                                         char *workingDirectory,
                                         size_t workingDirectorySize) {
    char executableDirectory[CLI_REPL_E2E_PATH_CAPACITY];

    if (!cli_get_executable_directory(executableDirectory, sizeof(executableDirectory))) {
        return 0;
    }

    if (workingDirectory != NULL && workingDirectorySize > 0) {
        snprintf(workingDirectory, workingDirectorySize, "%s", executableDirectory);
        workingDirectory[workingDirectorySize - 1] = '\0';
    }

#if defined(_WIN32)
    snprintf(buffer, bufferSize, "%s/zr_vm_cli.exe", executableDirectory);
#else
    snprintf(buffer, bufferSize, "%s/zr_vm_cli", executableDirectory);
#endif
    buffer[bufferSize - 1] = '\0';
    return ZrTests_File_Exists(buffer) ? 1 : 0;
}

static int test_repl_help_is_visible_before_quit(void) {
    const char *testName = "repl_help_is_visible_before_quit";
    ZrCliReplE2eProcess process;
    char cliPath[CLI_REPL_E2E_PATH_CAPACITY];
    char cliWorkingDirectory[CLI_REPL_E2E_PATH_CAPACITY];
    char error[CLI_REPL_E2E_ERROR_CAPACITY];
    const char *command[2];
    int status = 1;

    memset(&process, 0, sizeof(process));
#if !defined(_WIN32)
    process.stdinFd = -1;
    process.stdoutFd = -1;
#endif

    if (!cli_build_cli_executable_path(cliPath, sizeof(cliPath), cliWorkingDirectory, sizeof(cliWorkingDirectory))) {
        return cli_fail(testName, "failed to resolve zr_vm_cli beside test executable");
    }

    command[0] = cliPath;
    command[1] = NULL;
    if (!cli_process_start(&process, cliWorkingDirectory, command, error, sizeof(error))) {
        return cli_fail(testName, "%s", error);
    }

    if (!cli_process_wait_for_substring(&process, "ZR VM REPL", CLI_REPL_E2E_OUTPUT_TIMEOUT_MS)) {
        cli_process_terminate(&process);
        status = cli_fail(testName,
                          "timed out waiting for REPL banner\nOutput so far:\n%s",
                          cli_process_output(&process));
        goto cleanup;
    }

    if (!cli_process_write_line(&process, ":help")) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "failed to write :help to repl stdin");
        goto cleanup;
    }

    if (!cli_process_wait_for_substring(&process, "Available commands:", CLI_REPL_E2E_OUTPUT_TIMEOUT_MS)) {
        cli_process_terminate(&process);
        status = cli_fail(testName,
                          ":help output was not visible before quit\nOutput so far:\n%s",
                          cli_process_output(&process));
        goto cleanup;
    }

    if (strstr(cli_process_output(&process), ":reset  Clear the pending input buffer.") == NULL) {
        cli_process_terminate(&process);
        status = cli_fail(testName,
                          "help text did not include :reset before quit\nOutput so far:\n%s",
                          cli_process_output(&process));
        goto cleanup;
    }

    if (!cli_process_write_line(&process, ":quit")) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "failed to write :quit to repl stdin");
        goto cleanup;
    }

    if (!cli_process_wait_for_exit(&process, CLI_REPL_E2E_EXIT_TIMEOUT_MS)) {
        cli_process_terminate(&process);
        status = cli_fail(testName,
                          "timed out waiting for repl exit after :quit\nOutput:\n%s",
                          cli_process_output(&process));
        goto cleanup;
    }

    if (process.exitCode != 0) {
        status = cli_fail(testName, "repl exited with code %d\nOutput:\n%s", process.exitCode, cli_process_output(&process));
        goto cleanup;
    }

    status = 0;

cleanup:
    if (!process.hasExited) {
        cli_process_terminate(&process);
    }
    cli_process_close(&process);
    return status;
}

int main(void) {
    if (test_repl_help_is_visible_before_quit() != 0) {
        return 1;
    }
    return 0;
}

static int cli_fail(const char *testName, const char *format, ...) {
    va_list args;

    fprintf(stderr, "%s: ", testName != NULL ? testName : "cli_repl_e2e");
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    return 1;
}

static void cli_normalize_separators(char *path) {
    if (path == NULL) {
        return;
    }

    while (*path != '\0') {
        if (*path == '\\') {
            *path = '/';
        }
        path++;
    }
}

static unsigned long long cli_now_ms(void) {
#if defined(_WIN32)
    return (unsigned long long)GetTickCount64();
#else
    struct timespec timeValue;

    clock_gettime(CLOCK_MONOTONIC, &timeValue);
    return ((unsigned long long)timeValue.tv_sec * 1000ull) + (unsigned long long)(timeValue.tv_nsec / 1000000ull);
#endif
}

static void cli_sleep_ms(unsigned int milliseconds) {
#if defined(_WIN32)
    Sleep(milliseconds);
#else
    struct timespec request;

    request.tv_sec = (time_t)(milliseconds / 1000u);
    request.tv_nsec = (long)((milliseconds % 1000u) * 1000000u);
    while (nanosleep(&request, &request) != 0 && errno == EINTR) {
    }
#endif
}

static int cli_process_append_output(ZrCliReplE2eProcess *process, const char *text, size_t length) {
    size_t requiredCapacity;
    size_t newCapacity;
    char *newBuffer;

    if (process == NULL || text == NULL || length == 0) {
        return 1;
    }

    requiredCapacity = process->outputLength + length + 1;
    if (requiredCapacity <= process->outputCapacity) {
        memcpy(process->output + process->outputLength, text, length);
        process->outputLength += length;
        process->output[process->outputLength] = '\0';
        return 1;
    }

    newCapacity = process->outputCapacity > 0 ? process->outputCapacity : CLI_REPL_E2E_IO_CHUNK_SIZE;
    while (newCapacity < requiredCapacity) {
        newCapacity *= 2;
    }

    newBuffer = (char *)realloc(process->output, newCapacity);
    if (newBuffer == NULL) {
        return 0;
    }

    process->output = newBuffer;
    process->outputCapacity = newCapacity;
    memcpy(process->output + process->outputLength, text, length);
    process->outputLength += length;
    process->output[process->outputLength] = '\0';
    return 1;
}

static const char *cli_process_output(const ZrCliReplE2eProcess *process) {
    if (process == NULL || process->output == NULL) {
        return "";
    }
    return process->output;
}

#if defined(_WIN32)
static int cli_windows_append_char(char *buffer, size_t bufferSize, size_t *length, char value) {
    if (*length + 1 >= bufferSize) {
        return 0;
    }

    buffer[*length] = value;
    (*length)++;
    buffer[*length] = '\0';
    return 1;
}

static int cli_windows_append_repeated(char *buffer, size_t bufferSize, size_t *length, char value, size_t count) {
    size_t index;

    for (index = 0; index < count; index++) {
        if (!cli_windows_append_char(buffer, bufferSize, length, value)) {
            return 0;
        }
    }
    return 1;
}

static int cli_windows_append_quoted_arg(char *buffer, size_t bufferSize, size_t *length, const char *argument) {
    const char *cursor;
    size_t backslashCount = 0;

    if (!cli_windows_append_char(buffer, bufferSize, length, '"')) {
        return 0;
    }

    for (cursor = argument; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            backslashCount++;
            continue;
        }

        if (*cursor == '"') {
            if (!cli_windows_append_repeated(buffer, bufferSize, length, '\\', backslashCount * 2 + 1)) {
                return 0;
            }
            if (!cli_windows_append_char(buffer, bufferSize, length, '"')) {
                return 0;
            }
            backslashCount = 0;
            continue;
        }

        if (!cli_windows_append_repeated(buffer, bufferSize, length, '\\', backslashCount)) {
            return 0;
        }
        if (!cli_windows_append_char(buffer, bufferSize, length, *cursor)) {
            return 0;
        }
        backslashCount = 0;
    }

    if (!cli_windows_append_repeated(buffer, bufferSize, length, '\\', backslashCount * 2)) {
        return 0;
    }
    return cli_windows_append_char(buffer, bufferSize, length, '"');
}

static char *cli_windows_build_command_line(const char *const *command) {
    size_t requiredLength = 1;
    size_t length = 0;
    char *buffer;
    int index;

    for (index = 0; command[index] != NULL; index++) {
        requiredLength += strlen(command[index]) * 2 + 4;
    }

    buffer = (char *)malloc(requiredLength);
    if (buffer == NULL) {
        return NULL;
    }
    buffer[0] = '\0';

    for (index = 0; command[index] != NULL; index++) {
        if (index > 0 && !cli_windows_append_char(buffer, requiredLength, &length, ' ')) {
            free(buffer);
            return NULL;
        }
        if (!cli_windows_append_quoted_arg(buffer, requiredLength, &length, command[index])) {
            free(buffer);
            return NULL;
        }
    }

    return buffer;
}
#endif
