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

#include "cJSON/cJSON.h"
#include "path_support.h"
#include "zr_vm_lib_network/network.h"

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

#define CLI_E2E_PATH_CAPACITY 1024
#define CLI_E2E_ERROR_CAPACITY 512
#define CLI_E2E_IO_CHUNK_SIZE 4096
#define CLI_E2E_POLL_MS 20
#define CLI_E2E_EXIT_TIMEOUT_MS 5000
#define CLI_E2E_ENDPOINT_TIMEOUT_MS 2000
#define CLI_E2E_PROTOCOL_TIMEOUT_MS 5000

typedef struct ZrCliE2eProcess {
#if defined(_WIN32)
    HANDLE processHandle;
    HANDLE threadHandle;
    HANDLE stdoutRead;
#else
    pid_t pid;
    int stdoutFd;
#endif
    char *output;
    size_t outputLength;
    size_t outputCapacity;
    int exitCode;
    int hasExited;
} ZrCliE2eProcess;

static int cli_fail(const char *testName, const char *format, ...);
static void cli_normalize_separators(char *path);
static unsigned long long cli_now_ms(void);
static void cli_sleep_ms(unsigned int milliseconds);
static int cli_process_append_output(ZrCliE2eProcess *process, const char *text, size_t length);
static const char *cli_process_output(const ZrCliE2eProcess *process);
#if defined(_WIN32)
static char *cli_windows_build_command_line(const char *const *command);
#endif

static int cli_process_start(ZrCliE2eProcess *process,
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
    process->stdoutFd = -1;
#endif

#if defined(_WIN32)
    SECURITY_ATTRIBUTES securityAttributes;
    STARTUPINFOA startupInfo;
    PROCESS_INFORMATION processInfo;
    HANDLE stdoutWrite = NULL;
    char *commandLine = NULL;

    memset(&securityAttributes, 0, sizeof(securityAttributes));
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));

    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    if (!CreatePipe(&process->stdoutRead, &stdoutWrite, &securityAttributes, 0)) {
        snprintf(errorBuffer, errorBufferSize, "CreatePipe failed: %lu", (unsigned long)GetLastError());
        return 0;
    }
    if (!SetHandleInformation(process->stdoutRead, HANDLE_FLAG_INHERIT, 0)) {
        snprintf(errorBuffer, errorBufferSize, "SetHandleInformation failed: %lu", (unsigned long)GetLastError());
        CloseHandle(stdoutWrite);
        CloseHandle(process->stdoutRead);
        process->stdoutRead = NULL;
        return 0;
    }

    commandLine = cli_windows_build_command_line(command);
    if (commandLine == NULL) {
        snprintf(errorBuffer, errorBufferSize, "failed to allocate command line");
        CloseHandle(stdoutWrite);
        CloseHandle(process->stdoutRead);
        process->stdoutRead = NULL;
        return 0;
    }

    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
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
        CloseHandle(stdoutWrite);
        CloseHandle(process->stdoutRead);
        process->stdoutRead = NULL;
        return 0;
    }

    free(commandLine);
    CloseHandle(stdoutWrite);
    process->processHandle = processInfo.hProcess;
    process->threadHandle = processInfo.hThread;
    return 1;
#else
    int pipeFd[2];
    int flags;

    if (pipe(pipeFd) != 0) {
        snprintf(errorBuffer, errorBufferSize, "pipe failed: %s", strerror(errno));
        return 0;
    }

    process->pid = fork();
    if (process->pid < 0) {
        snprintf(errorBuffer, errorBufferSize, "fork failed: %s", strerror(errno));
        close(pipeFd[0]);
        close(pipeFd[1]);
        process->pid = 0;
        return 0;
    }

    if (process->pid == 0) {
        char *const *execCommand = (char *const *)command;

        close(pipeFd[0]);
        if (workingDirectory != NULL && workingDirectory[0] != '\0') {
            (void)chdir(workingDirectory);
        }
        dup2(pipeFd[1], STDOUT_FILENO);
        dup2(pipeFd[1], STDERR_FILENO);
        close(pipeFd[1]);
        execv(command[0], execCommand);
        _exit(127);
    }

    close(pipeFd[1]);
    process->stdoutFd = pipeFd[0];
    flags = fcntl(process->stdoutFd, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(process->stdoutFd, F_SETFL, flags | O_NONBLOCK);
    }
    return 1;
#endif
}

static int cli_process_update_exit_status(ZrCliE2eProcess *process) {
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

static int cli_process_read_available(ZrCliE2eProcess *process) {
    char buffer[CLI_E2E_IO_CHUNK_SIZE];

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

static int cli_process_wait_for_substring(ZrCliE2eProcess *process, const char *needle, unsigned int timeoutMs) {
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
        cli_sleep_ms(CLI_E2E_POLL_MS);
    }

    if (!cli_process_update_exit_status(process) || !cli_process_read_available(process)) {
        return 0;
    }
    return strstr(cli_process_output(process), needle) != NULL ? 1 : 0;
}

static int cli_process_wait_for_exit(ZrCliE2eProcess *process, unsigned int timeoutMs) {
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
        cli_sleep_ms(CLI_E2E_POLL_MS);
    }

    if (!process->hasExited) {
        return 0;
    }

    for (drainPass = 0; drainPass < 8; drainPass++) {
        if (!cli_process_read_available(process)) {
            return 0;
        }
        cli_sleep_ms(CLI_E2E_POLL_MS);
    }
    return 1;
}

static void cli_process_terminate(ZrCliE2eProcess *process) {
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

static void cli_process_close(ZrCliE2eProcess *process) {
    if (process == NULL) {
        return;
    }

#if defined(_WIN32)
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

static int cli_extract_prefixed_line_value(const char *text,
                                           const char *prefix,
                                           char *buffer,
                                           size_t bufferSize) {
    const char *valueStart;
    const char *valueEnd;
    size_t valueLength;

    if (text == NULL || prefix == NULL || buffer == NULL || bufferSize == 0) {
        return 0;
    }

    valueStart = strstr(text, prefix);
    if (valueStart == NULL) {
        return 0;
    }
    valueStart += strlen(prefix);
    valueEnd = valueStart;
    while (*valueEnd != '\0' && *valueEnd != '\n' && *valueEnd != '\r') {
        valueEnd++;
    }

    valueLength = (size_t)(valueEnd - valueStart);
    if (valueLength == 0 || valueLength >= bufferSize) {
        return 0;
    }

    memcpy(buffer, valueStart, valueLength);
    buffer[valueLength] = '\0';
    return 1;
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

static int cli_build_cli_executable_path(char *buffer, size_t bufferSize, char *workingDirectory, size_t workingDirectorySize) {
    char executableDirectory[CLI_E2E_PATH_CAPACITY];

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

static int cli_debug_connect(const char *endpointText, SZrNetworkStream *stream, char *errorBuffer, size_t errorBufferSize) {
    SZrNetworkEndpoint endpoint;

    if (endpointText == NULL || stream == NULL) {
        snprintf(errorBuffer, errorBufferSize, "debug connect requires endpoint and stream");
        return 0;
    }

    memset(stream, 0, sizeof(*stream));
    if (!ZrNetwork_ParseEndpoint(endpointText, &endpoint, errorBuffer, errorBufferSize)) {
        return 0;
    }
    if (!ZrNetwork_StreamConnectLoopback(&endpoint, 3000, stream, errorBuffer, errorBufferSize)) {
        return 0;
    }
    return 1;
}

static int cli_debug_send_request(SZrNetworkStream *stream,
                                  int id,
                                  const char *method,
                                  cJSON *params,
                                  char *errorBuffer,
                                  size_t errorBufferSize) {
    cJSON *request = cJSON_CreateObject();
    char *text = NULL;
    int success = 0;

    if (stream == NULL || method == NULL) {
        snprintf(errorBuffer, errorBufferSize, "debug request requires stream and method");
        return 0;
    }

    if (request == NULL) {
        snprintf(errorBuffer, errorBufferSize, "failed to allocate request json");
        if (params != NULL) {
            cJSON_Delete(params);
        }
        return 0;
    }

    cJSON_AddStringToObject(request, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(request, "id", id);
    cJSON_AddStringToObject(request, "method", method);
    cJSON_AddItemToObject(request, "params", params != NULL ? params : cJSON_CreateObject());

    text = cJSON_PrintUnformatted(request);
    if (text == NULL) {
        snprintf(errorBuffer, errorBufferSize, "failed to format request json");
        cJSON_Delete(request);
        return 0;
    }

    success = ZrNetwork_StreamWriteFrame(stream, text, strlen(text)) ? 1 : 0;
    if (!success) {
        snprintf(errorBuffer, errorBufferSize, "failed to write debug request '%s'", method);
    }

    cJSON_free(text);
    cJSON_Delete(request);
    return success;
}

static cJSON *cli_debug_read_message(SZrNetworkStream *stream, char *errorBuffer, size_t errorBufferSize) {
    char frame[ZR_NETWORK_FRAME_BUFFER_CAPACITY];
    TZrSize frameLength = 0;
    cJSON *message;

    if (stream == NULL) {
        snprintf(errorBuffer, errorBufferSize, "debug read requires stream");
        return NULL;
    }

    if (!ZrNetwork_StreamReadFrame(stream,
                                   CLI_E2E_PROTOCOL_TIMEOUT_MS,
                                   frame,
                                   sizeof(frame),
                                   &frameLength)) {
        snprintf(errorBuffer, errorBufferSize, "timed out while reading debug frame");
        return NULL;
    }

    message = cJSON_Parse(frame);
    if (message == NULL) {
        snprintf(errorBuffer, errorBufferSize, "failed to parse debug json frame");
        return NULL;
    }
    return message;
}

static int cli_debug_expect_response(SZrNetworkStream *stream,
                                     int expectedId,
                                     cJSON **outMessage,
                                     char *errorBuffer,
                                     size_t errorBufferSize) {
    cJSON *message = cli_debug_read_message(stream, errorBuffer, errorBufferSize);
    cJSON *idItem;
    cJSON *errorItem;

    if (message == NULL) {
        return 0;
    }

    idItem = cJSON_GetObjectItemCaseSensitive(message, "id");
    errorItem = cJSON_GetObjectItemCaseSensitive(message, "error");
    if (!cJSON_IsNumber(idItem) || (int)idItem->valuedouble != expectedId) {
        snprintf(errorBuffer, errorBufferSize, "unexpected response id");
        cJSON_Delete(message);
        return 0;
    }
    if (errorItem != NULL) {
        cJSON *messageItem = cJSON_GetObjectItemCaseSensitive(errorItem, "message");
        snprintf(errorBuffer,
                 errorBufferSize,
                 "debug response error: %s",
                 cJSON_IsString(messageItem) ? messageItem->valuestring : "unknown");
        cJSON_Delete(message);
        return 0;
    }

    if (outMessage != NULL) {
        *outMessage = message;
    } else {
        cJSON_Delete(message);
    }
    return 1;
}

static int cli_debug_expect_event(SZrNetworkStream *stream,
                                  const char *expectedMethod,
                                  cJSON **outMessage,
                                  char *errorBuffer,
                                  size_t errorBufferSize) {
    cJSON *message = cli_debug_read_message(stream, errorBuffer, errorBufferSize);
    cJSON *methodItem;
    char *messageText = NULL;

    if (message == NULL) {
        return 0;
    }

    methodItem = cJSON_GetObjectItemCaseSensitive(message, "method");
    if (!cJSON_IsString(methodItem) || strcmp(methodItem->valuestring, expectedMethod) != 0) {
        messageText = cJSON_PrintUnformatted(message);
        snprintf(errorBuffer,
                 errorBufferSize,
                 "unexpected event '%s' while waiting for '%s'%s%s",
                 cJSON_IsString(methodItem) ? methodItem->valuestring : "",
                 expectedMethod != NULL ? expectedMethod : "",
                 messageText != NULL ? ": " : "",
                 messageText != NULL ? messageText : "");
        if (messageText != NULL) {
            cJSON_free(messageText);
        }
        cJSON_Delete(message);
        return 0;
    }

    if (outMessage != NULL) {
        *outMessage = message;
    } else {
        cJSON_Delete(message);
    }
    return 1;
}

static const char *cli_debug_json_string(cJSON *object, const char *field) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, field);

    return cJSON_IsString(item) ? item->valuestring : "";
}

static int cli_debug_json_bool(cJSON *object, const char *field) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, field);

    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item) ? 1 : 0;
    }
    return cJSON_IsNumber(item) && item->valuedouble != 0.0 ? 1 : 0;
}

static int cli_debug_json_int(cJSON *object, const char *field) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, field);

    return cJSON_IsNumber(item) ? (int)item->valuedouble : 0;
}

static int test_debug_print_endpoint_without_wait(void) {
    const char *testName = "debug_print_endpoint_without_wait";
    ZrCliE2eProcess process;
    char cliPath[CLI_E2E_PATH_CAPACITY];
    char cliWorkingDirectory[CLI_E2E_PATH_CAPACITY];
    char projectPath[CLI_E2E_PATH_CAPACITY];
    char endpoint[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];
    char error[CLI_E2E_ERROR_CAPACITY];
    const char *command[7];
    int status = 1;

    memset(&process, 0, sizeof(process));
#if !defined(_WIN32)
    process.stdoutFd = -1;
#endif

    if (!cli_build_cli_executable_path(cliPath, sizeof(cliPath), cliWorkingDirectory, sizeof(cliWorkingDirectory))) {
        return cli_fail(testName, "failed to resolve zr_vm_cli beside test executable");
    }
    if (!ZrTests_Path_GetProjectFile("hello_world", "hello_world.zrp", projectPath, sizeof(projectPath))) {
        return cli_fail(testName, "failed to resolve hello_world project fixture");
    }

    command[0] = cliPath;
    command[1] = projectPath;
    command[2] = "--debug";
    command[3] = "--debug-address";
    command[4] = "127.0.0.1:0";
    command[5] = "--debug-print-endpoint";
    command[6] = NULL;

    if (!cli_process_start(&process, cliWorkingDirectory, command, error, sizeof(error))) {
        return cli_fail(testName, "%s", error);
    }

    if (!cli_process_wait_for_exit(&process, CLI_E2E_EXIT_TIMEOUT_MS)) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "timed out waiting for cli exit");
        goto cleanup;
    }
    if (process.exitCode != 0) {
        status = cli_fail(testName, "cli exited with code %d\nOutput:\n%s", process.exitCode, cli_process_output(&process));
        goto cleanup;
    }
    if (!cli_extract_prefixed_line_value(cli_process_output(&process), "debug_endpoint=", endpoint, sizeof(endpoint))) {
        status = cli_fail(testName, "missing debug endpoint in output\nOutput:\n%s", cli_process_output(&process));
        goto cleanup;
    }
    if (strncmp(endpoint, "127.0.0.1:", 10) != 0) {
        status = cli_fail(testName, "unexpected endpoint '%s'", endpoint);
        goto cleanup;
    }
    if (strstr(cli_process_output(&process), "hello world") == NULL) {
        status = cli_fail(testName, "missing hello world output\nOutput:\n%s", cli_process_output(&process));
        goto cleanup;
    }

    status = 0;

cleanup:
    cli_process_close(&process);
    return status;
}

static int test_debug_wait_prints_endpoint_and_accepts_client(void) {
    const char *testName = "debug_wait_prints_endpoint_and_accepts_client";
    ZrCliE2eProcess process;
    SZrNetworkStream client;
    char cliPath[CLI_E2E_PATH_CAPACITY];
    char cliWorkingDirectory[CLI_E2E_PATH_CAPACITY];
    char projectPath[CLI_E2E_PATH_CAPACITY];
    char endpoint[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];
    char error[CLI_E2E_ERROR_CAPACITY];
    const char *command[8];
    cJSON *message = NULL;
    int status = 1;

    memset(&process, 0, sizeof(process));
    memset(&client, 0, sizeof(client));
#if !defined(_WIN32)
    process.stdoutFd = -1;
#endif

    if (!cli_build_cli_executable_path(cliPath, sizeof(cliPath), cliWorkingDirectory, sizeof(cliWorkingDirectory))) {
        return cli_fail(testName, "failed to resolve zr_vm_cli beside test executable");
    }
    if (!ZrTests_Path_GetProjectFile("hello_world", "hello_world.zrp", projectPath, sizeof(projectPath))) {
        return cli_fail(testName, "failed to resolve hello_world project fixture");
    }

    command[0] = cliPath;
    command[1] = projectPath;
    command[2] = "--debug";
    command[3] = "--debug-address";
    command[4] = "127.0.0.1:0";
    command[5] = "--debug-wait";
    command[6] = "--debug-print-endpoint";
    command[7] = NULL;

    if (!cli_process_start(&process, cliWorkingDirectory, command, error, sizeof(error))) {
        return cli_fail(testName, "%s", error);
    }

    if (!cli_process_wait_for_substring(&process, "debug_endpoint=", CLI_E2E_ENDPOINT_TIMEOUT_MS)) {
        cli_process_terminate(&process);
        status = cli_fail(testName,
                          "timed out waiting for debug endpoint\nOutput so far:\n%s",
                          cli_process_output(&process));
        goto cleanup;
    }
    if (!cli_extract_prefixed_line_value(cli_process_output(&process), "debug_endpoint=", endpoint, sizeof(endpoint))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "failed to extract debug endpoint\nOutput:\n%s", cli_process_output(&process));
        goto cleanup;
    }
    if (!cli_debug_connect(endpoint, &client, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }

    if (!cli_debug_send_request(&client, 1, "initialize", cJSON_CreateObject(), error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }

    if (!cli_debug_expect_response(&client, 1, &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    if (strcmp(cli_debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "result"), "protocol"), "zrdbg/1") != 0) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "initialize response did not report zrdbg/1");
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_expect_event(&client, "initialized", &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_expect_event(&client, "moduleLoaded", &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    if (strcmp(cli_debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"), "main") != 0) {
        cli_process_terminate(&process);
        status = cli_fail(testName,
                          "unexpected module name '%s'",
                          cli_debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_expect_event(&client, "stopped", &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    if (strcmp(cli_debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"), "entry") != 0) {
        cli_process_terminate(&process);
        status = cli_fail(testName,
                          "expected entry stop, got '%s'",
                          cli_debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_send_request(&client, 2, "continue", cJSON_CreateObject(), error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    if (!cli_debug_expect_response(&client, 2, &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_expect_event(&client, "continued", &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_expect_event(&client, "terminated", &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    if (!cli_debug_json_bool(cJSON_GetObjectItemCaseSensitive(message, "params"), "success")) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "terminated event did not report success");
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_process_wait_for_exit(&process, CLI_E2E_EXIT_TIMEOUT_MS)) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "timed out waiting for cli exit after continue");
        goto cleanup;
    }
    if (process.exitCode != 0) {
        status = cli_fail(testName, "cli exited with code %d\nOutput:\n%s", process.exitCode, cli_process_output(&process));
        goto cleanup;
    }
    if (strstr(cli_process_output(&process), "hello world") == NULL) {
        status = cli_fail(testName, "missing hello world output\nOutput:\n%s", cli_process_output(&process));
        goto cleanup;
    }

    status = 0;

cleanup:
    if (message != NULL) {
        cJSON_Delete(message);
    }
    if (client.isOpen) {
        ZrNetwork_StreamClose(&client);
    }
    if (!process.hasExited) {
        cli_process_terminate(&process);
    }
    cli_process_close(&process);
    return status;
}

static int test_debug_wait_hits_import_basic_launch_breakpoint(void) {
    const char *testName = "debug_wait_hits_import_basic_launch_breakpoint";
    ZrCliE2eProcess process;
    SZrNetworkStream client;
    char cliPath[CLI_E2E_PATH_CAPACITY];
    char cliWorkingDirectory[CLI_E2E_PATH_CAPACITY];
    char projectPath[CLI_E2E_PATH_CAPACITY];
    char sourcePath[CLI_E2E_PATH_CAPACITY];
    char runtimeModuleSource[CLI_E2E_PATH_CAPACITY];
    char runtimeEntrySource[CLI_E2E_PATH_CAPACITY];
    int runtimeEntryLine = 0;
    char endpoint[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];
    char error[CLI_E2E_ERROR_CAPACITY];
    const char *command[8];
    cJSON *message = NULL;
    cJSON *params = NULL;
    cJSON *breakpoints = NULL;
    cJSON *breakpoint = NULL;
    cJSON *result = NULL;
    cJSON *resultBreakpoints = NULL;
    cJSON *firstBreakpoint = NULL;
    int status = 1;

    memset(&process, 0, sizeof(process));
    memset(&client, 0, sizeof(client));
    runtimeModuleSource[0] = '\0';
    runtimeEntrySource[0] = '\0';
    runtimeEntryLine = 0;
#if !defined(_WIN32)
    process.stdoutFd = -1;
#endif

    if (!cli_build_cli_executable_path(cliPath, sizeof(cliPath), cliWorkingDirectory, sizeof(cliWorkingDirectory))) {
        return cli_fail(testName, "failed to resolve zr_vm_cli beside test executable");
    }
    if (!ZrTests_Path_GetProjectFile("import_basic", "import_basic.zrp", projectPath, sizeof(projectPath))) {
        return cli_fail(testName, "failed to resolve import_basic project fixture");
    }
    if (!ZrTests_Path_GetProjectFile("import_basic", "src/main.zr", sourcePath, sizeof(sourcePath))) {
        return cli_fail(testName, "failed to resolve import_basic main.zr fixture");
    }

    command[0] = cliPath;
    command[1] = projectPath;
    command[2] = "--debug";
    command[3] = "--debug-address";
    command[4] = "127.0.0.1:0";
    command[5] = "--debug-wait";
    command[6] = "--debug-print-endpoint";
    command[7] = NULL;

    if (!cli_process_start(&process, cliWorkingDirectory, command, error, sizeof(error))) {
        return cli_fail(testName, "%s", error);
    }

    if (!cli_process_wait_for_substring(&process, "debug_endpoint=", CLI_E2E_ENDPOINT_TIMEOUT_MS)) {
        cli_process_terminate(&process);
        status = cli_fail(testName,
                          "timed out waiting for debug endpoint\nOutput so far:\n%s",
                          cli_process_output(&process));
        goto cleanup;
    }
    if (!cli_extract_prefixed_line_value(cli_process_output(&process), "debug_endpoint=", endpoint, sizeof(endpoint))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "failed to extract debug endpoint\nOutput:\n%s", cli_process_output(&process));
        goto cleanup;
    }
    if (!cli_debug_connect(endpoint, &client, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }

    if (!cli_debug_send_request(&client, 1, "initialize", cJSON_CreateObject(), error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    if (!cli_debug_expect_response(&client, 1, &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_expect_event(&client, "initialized", &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_expect_event(&client, "moduleLoaded", &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    snprintf(runtimeModuleSource,
             sizeof(runtimeModuleSource),
             "%s",
             cli_debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "sourceFile"));
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_expect_event(&client, "stopped", &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    if (strcmp(cli_debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"), "entry") != 0) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "expected entry stop before breakpoint setup");
        goto cleanup;
    }
    snprintf(runtimeEntrySource,
             sizeof(runtimeEntrySource),
             "%s",
             cli_debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "sourceFile"));
    runtimeEntryLine = cli_debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line");
    cJSON_Delete(message);
    message = NULL;

    params = cJSON_CreateObject();
    breakpoints = cJSON_CreateArray();
    breakpoint = cJSON_CreateObject();
    if (params == NULL || breakpoints == NULL || breakpoint == NULL) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "failed to allocate setBreakpoints request");
        goto cleanup;
    }
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddNumberToObject(breakpoint, "line", 3);
    cJSON_AddItemToArray(breakpoints, breakpoint);
    breakpoint = NULL;
    cJSON_AddItemToObject(params, "breakpoints", breakpoints);
    breakpoints = NULL;

    if (!cli_debug_send_request(&client, 2, "setBreakpoints", params, error, sizeof(error))) {
        params = NULL;
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    params = NULL;

    if (!cli_debug_expect_event(&client, "breakpointResolved", &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    if (!cli_debug_json_bool(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved")) {
        cli_process_terminate(&process);
        status = cli_fail(testName,
                          "expected import_basic launch breakpoint to resolve\n"
                          "Requested source: %s\n"
                          "moduleLoaded source: %s\n"
                          "entry stop source: %s\n"
                          "entry stop line: %d\n"
                          "Output so far:\n%s",
                          sourcePath,
                          runtimeModuleSource,
                          runtimeEntrySource,
                          runtimeEntryLine,
                          cli_process_output(&process));
        goto cleanup;
    }
    if (cli_debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line") != 3) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "expected resolved breakpoint line 3");
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_expect_response(&client, 2, &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    resultBreakpoints = cJSON_GetObjectItemCaseSensitive(result, "breakpoints");
    firstBreakpoint = cJSON_IsArray(resultBreakpoints) ? cJSON_GetArrayItem(resultBreakpoints, 0) : NULL;
    if (firstBreakpoint == NULL || !cli_debug_json_bool(firstBreakpoint, "verified")) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "setBreakpoints response did not report a verified breakpoint");
        goto cleanup;
    }
    if (cli_debug_json_int(firstBreakpoint, "line") != 3) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "setBreakpoints response did not preserve line 3");
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_send_request(&client, 3, "continue", cJSON_CreateObject(), error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    if (!cli_debug_expect_response(&client, 3, &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_expect_event(&client, "continued", &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_expect_event(&client, "stopped", &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s\nentry stop line: %d", error, runtimeEntryLine);
        goto cleanup;
    }
    if (strcmp(cli_debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"), "breakpoint") != 0) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "expected continue to stop on the configured breakpoint");
        goto cleanup;
    }
    if (cli_debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line") != 3) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "expected breakpoint stop on line 3");
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_send_request(&client, 4, "continue", cJSON_CreateObject(), error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    if (!cli_debug_expect_response(&client, 4, &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_expect_event(&client, "continued", &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_debug_expect_event(&client, "terminated", &message, error, sizeof(error))) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "%s", error);
        goto cleanup;
    }
    if (!cli_debug_json_bool(cJSON_GetObjectItemCaseSensitive(message, "params"), "success")) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "terminated event did not report success");
        goto cleanup;
    }
    cJSON_Delete(message);
    message = NULL;

    if (!cli_process_wait_for_exit(&process, CLI_E2E_EXIT_TIMEOUT_MS)) {
        cli_process_terminate(&process);
        status = cli_fail(testName, "timed out waiting for cli exit after breakpoint continue");
        goto cleanup;
    }
    if (process.exitCode != 0) {
        status = cli_fail(testName, "cli exited with code %d\nOutput:\n%s", process.exitCode, cli_process_output(&process));
        goto cleanup;
    }

    status = 0;

cleanup:
    if (breakpoint != NULL) {
        cJSON_Delete(breakpoint);
    }
    if (breakpoints != NULL) {
        cJSON_Delete(breakpoints);
    }
    if (params != NULL) {
        cJSON_Delete(params);
    }
    if (message != NULL) {
        cJSON_Delete(message);
    }
    if (client.isOpen) {
        ZrNetwork_StreamClose(&client);
    }
    if (!process.hasExited) {
        cli_process_terminate(&process);
    }
    cli_process_close(&process);
    return status;
}

int main(void) {
    if (test_debug_print_endpoint_without_wait() != 0) {
        return 1;
    }
    if (test_debug_wait_prints_endpoint_and_accepts_client() != 0) {
        return 1;
    }
    if (test_debug_wait_hits_import_basic_launch_breakpoint() != 0) {
        return 1;
    }
    return 0;
}

static int cli_fail(const char *testName, const char *format, ...) {
    va_list args;

    fprintf(stderr, "%s: ", testName != NULL ? testName : "cli_debug_e2e");
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

static int cli_process_append_output(ZrCliE2eProcess *process, const char *text, size_t length) {
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

    newCapacity = process->outputCapacity > 0 ? process->outputCapacity : CLI_E2E_IO_CHUNK_SIZE;
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

static const char *cli_process_output(const ZrCliE2eProcess *process) {
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
