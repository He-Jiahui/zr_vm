#if !defined(_WIN32)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#else
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

typedef struct SZrPerfRunSample {
    double wallMs;
    uint64_t peakWorkingSetBytes;
    int exitCode;
} SZrPerfRunSample;

typedef struct SZrPerfSummary {
    double meanWallMs;
    double medianWallMs;
    double minWallMs;
    double maxWallMs;
    double stddevWallMs;
    double meanPeakWorkingSetBytes;
    double medianPeakWorkingSetBytes;
    uint64_t minPeakWorkingSetBytes;
    uint64_t maxPeakWorkingSetBytes;
} SZrPerfSummary;

static void zr_perf_print_usage(const char *executable) {
    fprintf(stderr,
            "Usage:\n"
            "  %s --name <case> --iterations <n> --warmup <n> --json-out <path> [--working-directory <dir>] -- <command> [args...]\n",
            executable);
}

static int zr_perf_parse_positive_int(const char *text, int *outValue) {
    long parsed;
    char *end = NULL;

    if (text == NULL || outValue == NULL || text[0] == '\0') {
        return 0;
    }

    parsed = strtol(text, &end, 10);
    if (end == text || end == NULL || *end != '\0' || parsed <= 0 || parsed > 1000000L) {
        return 0;
    }

    *outValue = (int)parsed;
    return 1;
}

static int zr_perf_compare_double(const void *left, const void *right) {
    const double leftValue = *(const double *)left;
    const double rightValue = *(const double *)right;

    if (leftValue < rightValue) {
        return -1;
    }
    if (leftValue > rightValue) {
        return 1;
    }
    return 0;
}

static int zr_perf_compare_u64(const void *left, const void *right) {
    const uint64_t leftValue = *(const uint64_t *)left;
    const uint64_t rightValue = *(const uint64_t *)right;

    if (leftValue < rightValue) {
        return -1;
    }
    if (leftValue > rightValue) {
        return 1;
    }
    return 0;
}

static double zr_perf_calculate_mean_double(const double *values, int count) {
    double sum = 0.0;
    int index;

    if (values == NULL || count <= 0) {
        return 0.0;
    }

    for (index = 0; index < count; index++) {
        sum += values[index];
    }
    return sum / (double)count;
}

static double zr_perf_calculate_mean_u64(const uint64_t *values, int count) {
    double sum = 0.0;
    int index;

    if (values == NULL || count <= 0) {
        return 0.0;
    }

    for (index = 0; index < count; index++) {
        sum += (double)values[index];
    }
    return sum / (double)count;
}

static double zr_perf_calculate_median_double(const double *values, int count) {
    double *sortedValues;
    double result;

    if (values == NULL || count <= 0) {
        return 0.0;
    }

    sortedValues = (double *)malloc((size_t)count * sizeof(*sortedValues));
    if (sortedValues == NULL) {
        return 0.0;
    }

    memcpy(sortedValues, values, (size_t)count * sizeof(*sortedValues));
    qsort(sortedValues, (size_t)count, sizeof(*sortedValues), zr_perf_compare_double);
    if ((count % 2) == 0) {
        result = (sortedValues[(count / 2) - 1] + sortedValues[count / 2]) / 2.0;
    } else {
        result = sortedValues[count / 2];
    }

    free(sortedValues);
    return result;
}

static double zr_perf_calculate_median_u64(const uint64_t *values, int count) {
    uint64_t *sortedValues;
    double result;

    if (values == NULL || count <= 0) {
        return 0.0;
    }

    sortedValues = (uint64_t *)malloc((size_t)count * sizeof(*sortedValues));
    if (sortedValues == NULL) {
        return 0.0;
    }

    memcpy(sortedValues, values, (size_t)count * sizeof(*sortedValues));
    qsort(sortedValues, (size_t)count, sizeof(*sortedValues), zr_perf_compare_u64);
    if ((count % 2) == 0) {
        result = ((double)sortedValues[(count / 2) - 1] + (double)sortedValues[count / 2]) / 2.0;
    } else {
        result = (double)sortedValues[count / 2];
    }

    free(sortedValues);
    return result;
}

static double zr_perf_calculate_stddev(const double *values, int count, double mean) {
    double sumSquares = 0.0;
    int index;

    if (values == NULL || count <= 0) {
        return 0.0;
    }

    for (index = 0; index < count; index++) {
        const double delta = values[index] - mean;
        sumSquares += delta * delta;
    }

    return sqrt(sumSquares / (double)count);
}

static void zr_perf_compute_summary(const SZrPerfRunSample *samples, int count, SZrPerfSummary *summary) {
    double *wallValues;
    uint64_t *peakValues;
    int index;

    if (samples == NULL || count <= 0 || summary == NULL) {
        return;
    }

    wallValues = (double *)malloc((size_t)count * sizeof(*wallValues));
    peakValues = (uint64_t *)malloc((size_t)count * sizeof(*peakValues));
    if (wallValues == NULL || peakValues == NULL) {
        free(wallValues);
        free(peakValues);
        memset(summary, 0, sizeof(*summary));
        return;
    }

    summary->minWallMs = samples[0].wallMs;
    summary->maxWallMs = samples[0].wallMs;
    summary->minPeakWorkingSetBytes = samples[0].peakWorkingSetBytes;
    summary->maxPeakWorkingSetBytes = samples[0].peakWorkingSetBytes;

    for (index = 0; index < count; index++) {
        wallValues[index] = samples[index].wallMs;
        peakValues[index] = samples[index].peakWorkingSetBytes;

        if (samples[index].wallMs < summary->minWallMs) {
            summary->minWallMs = samples[index].wallMs;
        }
        if (samples[index].wallMs > summary->maxWallMs) {
            summary->maxWallMs = samples[index].wallMs;
        }
        if (samples[index].peakWorkingSetBytes < summary->minPeakWorkingSetBytes) {
            summary->minPeakWorkingSetBytes = samples[index].peakWorkingSetBytes;
        }
        if (samples[index].peakWorkingSetBytes > summary->maxPeakWorkingSetBytes) {
            summary->maxPeakWorkingSetBytes = samples[index].peakWorkingSetBytes;
        }
    }

    summary->meanWallMs = zr_perf_calculate_mean_double(wallValues, count);
    summary->medianWallMs = zr_perf_calculate_median_double(wallValues, count);
    summary->stddevWallMs = zr_perf_calculate_stddev(wallValues, count, summary->meanWallMs);
    summary->meanPeakWorkingSetBytes = zr_perf_calculate_mean_u64(peakValues, count);
    summary->medianPeakWorkingSetBytes = zr_perf_calculate_median_u64(peakValues, count);

    free(wallValues);
    free(peakValues);
}

static void zr_perf_json_write_escaped(FILE *file, const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;

    fputc('"', file);
    while (cursor != NULL && *cursor != '\0') {
        switch (*cursor) {
            case '\\':
                fputs("\\\\", file);
                break;
            case '"':
                fputs("\\\"", file);
                break;
            case '\n':
                fputs("\\n", file);
                break;
            case '\r':
                fputs("\\r", file);
                break;
            case '\t':
                fputs("\\t", file);
                break;
            default:
                if (*cursor < 0x20) {
                    fprintf(file, "\\u%04x", (unsigned int)*cursor);
                } else {
                    fputc((int)*cursor, file);
                }
                break;
        }
        cursor++;
    }
    fputc('"', file);
}

static int zr_perf_write_json_report(const char *jsonPath,
                                     const char *caseName,
                                     const char *workingDirectory,
                                     char *const *command,
                                     int iterations,
                                     int warmup,
                                     const SZrPerfRunSample *samples,
                                     const SZrPerfSummary *summary) {
    FILE *file;
    int index;

    if (jsonPath == NULL || caseName == NULL || command == NULL || summary == NULL) {
        return 0;
    }

    file = fopen(jsonPath, "wb");
    if (file == NULL) {
        return 0;
    }

    fputs("{\n", file);
    fputs("  \"name\": ", file);
    zr_perf_json_write_escaped(file, caseName);
    fputs(",\n", file);
    fputs("  \"working_directory\": ", file);
    zr_perf_json_write_escaped(file, workingDirectory != NULL ? workingDirectory : "");
    fputs(",\n", file);
    fprintf(file, "  \"iterations\": %d,\n", iterations);
    fprintf(file, "  \"warmup\": %d,\n", warmup);
    fputs("  \"command\": [", file);
    for (index = 0; command[index] != NULL; index++) {
        if (index > 0) {
            fputs(", ", file);
        }
        zr_perf_json_write_escaped(file, command[index]);
    }
    fputs("],\n", file);
    fputs("  \"runs\": [\n", file);
    for (index = 0; index < iterations; index++) {
        fprintf(file,
                "    {\"index\": %d, \"wall_ms\": %.3f, \"peak_working_set_bytes\": %" PRIu64 "}%s\n",
                index + 1,
                samples[index].wallMs,
                samples[index].peakWorkingSetBytes,
                (index + 1) == iterations ? "" : ",");
    }
    fputs("  ],\n", file);
    fputs("  \"summary\": {\n", file);
    fprintf(file, "    \"mean_wall_ms\": %.3f,\n", summary->meanWallMs);
    fprintf(file, "    \"median_wall_ms\": %.3f,\n", summary->medianWallMs);
    fprintf(file, "    \"min_wall_ms\": %.3f,\n", summary->minWallMs);
    fprintf(file, "    \"max_wall_ms\": %.3f,\n", summary->maxWallMs);
    fprintf(file, "    \"stddev_wall_ms\": %.3f,\n", summary->stddevWallMs);
    fprintf(file, "    \"mean_peak_working_set_bytes\": %.0f,\n", summary->meanPeakWorkingSetBytes);
    fprintf(file, "    \"median_peak_working_set_bytes\": %.0f,\n", summary->medianPeakWorkingSetBytes);
    fprintf(file, "    \"min_peak_working_set_bytes\": %" PRIu64 ",\n", summary->minPeakWorkingSetBytes);
    fprintf(file, "    \"max_peak_working_set_bytes\": %" PRIu64 "\n", summary->maxPeakWorkingSetBytes);
    fputs("  }\n", file);
    fputs("}\n", file);

    fclose(file);
    return 1;
}

#if defined(_WIN32)
static void zr_perf_format_windows_error(DWORD errorCode, char *buffer, size_t bufferSize) {
    DWORD written = 0;

    if (buffer == NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    written = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL,
                             errorCode,
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                             buffer,
                             (DWORD)bufferSize,
                             NULL);
    if (written == 0) {
        snprintf(buffer, bufferSize, "Windows error %lu", (unsigned long)errorCode);
    }
}

static int zr_perf_windows_append_char(char *buffer, size_t bufferSize, size_t *length, char value) {
    if (buffer == NULL || length == NULL || *length + 1 >= bufferSize) {
        return 0;
    }

    buffer[*length] = value;
    (*length)++;
    buffer[*length] = '\0';
    return 1;
}

static int zr_perf_windows_append_repeated(char *buffer,
                                           size_t bufferSize,
                                           size_t *length,
                                           char value,
                                           size_t repeat) {
    size_t index;

    for (index = 0; index < repeat; index++) {
        if (!zr_perf_windows_append_char(buffer, bufferSize, length, value)) {
            return 0;
        }
    }
    return 1;
}

static int zr_perf_windows_append_string(char *buffer, size_t bufferSize, size_t *length, const char *text) {
    while (text != NULL && *text != '\0') {
        if (!zr_perf_windows_append_char(buffer, bufferSize, length, *text)) {
            return 0;
        }
        text++;
    }
    return 1;
}

static int zr_perf_windows_append_quoted_arg(char *buffer, size_t bufferSize, size_t *length, const char *arg) {
    size_t backslashCount = 0;
    int needsQuotes = 0;
    const char *cursor;

    if (arg == NULL) {
        return zr_perf_windows_append_string(buffer, bufferSize, length, "\"\"");
    }

    if (arg[0] == '\0') {
        needsQuotes = 1;
    } else {
        for (cursor = arg; *cursor != '\0'; cursor++) {
            if (*cursor == ' ' || *cursor == '\t' || *cursor == '"') {
                needsQuotes = 1;
                break;
            }
        }
    }

    if (!needsQuotes) {
        return zr_perf_windows_append_string(buffer, bufferSize, length, arg);
    }

    if (!zr_perf_windows_append_char(buffer, bufferSize, length, '"')) {
        return 0;
    }

    for (cursor = arg; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            backslashCount++;
            continue;
        }

        if (*cursor == '"') {
            if (!zr_perf_windows_append_repeated(buffer, bufferSize, length, '\\', backslashCount * 2 + 1)) {
                return 0;
            }
            if (!zr_perf_windows_append_char(buffer, bufferSize, length, '"')) {
                return 0;
            }
            backslashCount = 0;
            continue;
        }

        if (!zr_perf_windows_append_repeated(buffer, bufferSize, length, '\\', backslashCount)) {
            return 0;
        }
        if (!zr_perf_windows_append_char(buffer, bufferSize, length, *cursor)) {
            return 0;
        }
        backslashCount = 0;
    }

    if (!zr_perf_windows_append_repeated(buffer, bufferSize, length, '\\', backslashCount * 2)) {
        return 0;
    }
    return zr_perf_windows_append_char(buffer, bufferSize, length, '"');
}

static char *zr_perf_windows_build_command_line(char *const *command) {
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
        if (index > 0 && !zr_perf_windows_append_char(buffer, requiredLength, &length, ' ')) {
            free(buffer);
            return NULL;
        }
        if (!zr_perf_windows_append_quoted_arg(buffer, requiredLength, &length, command[index])) {
            free(buffer);
            return NULL;
        }
    }

    return buffer;
}

static int zr_perf_run_command(const char *workingDirectory,
                               char *const *command,
                               SZrPerfRunSample *sample,
                               char *errorBuffer,
                               size_t errorBufferSize) {
    LARGE_INTEGER frequency;
    LARGE_INTEGER startTime;
    LARGE_INTEGER endTime;
    STARTUPINFOA startupInfo;
    PROCESS_INFORMATION processInfo;
    PROCESS_MEMORY_COUNTERS memoryCounters;
    DWORD exitCode = 0;
    HANDLE nullHandle = INVALID_HANDLE_VALUE;
    char *commandLine = NULL;

    if (command == NULL || command[0] == NULL || sample == NULL) {
        return 0;
    }

    memset(sample, 0, sizeof(*sample));
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));
    memset(&memoryCounters, 0, sizeof(memoryCounters));

    commandLine = zr_perf_windows_build_command_line(command);
    if (commandLine == NULL) {
        snprintf(errorBuffer, errorBufferSize, "failed to allocate Windows command line");
        return 0;
    }

    nullHandle = CreateFileA("NUL",
                             GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL,
                             NULL);
    if (nullHandle == INVALID_HANDLE_VALUE) {
        zr_perf_format_windows_error(GetLastError(), errorBuffer, errorBufferSize);
        free(commandLine);
        return 0;
    }

    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startupInfo.hStdOutput = nullHandle;
    startupInfo.hStdError = nullHandle;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&startTime);
    if (!CreateProcessA(command[0],
                        commandLine,
                        NULL,
                        NULL,
                        TRUE,
                        CREATE_NO_WINDOW,
                        NULL,
                        (workingDirectory != NULL && workingDirectory[0] != '\0') ? workingDirectory : NULL,
                        &startupInfo,
                        &processInfo)) {
        zr_perf_format_windows_error(GetLastError(), errorBuffer, errorBufferSize);
        CloseHandle(nullHandle);
        free(commandLine);
        return 0;
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    QueryPerformanceCounter(&endTime);
    if (!GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
        zr_perf_format_windows_error(GetLastError(), errorBuffer, errorBufferSize);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        CloseHandle(nullHandle);
        free(commandLine);
        return 0;
    }

    memoryCounters.cb = sizeof(memoryCounters);
    if (!GetProcessMemoryInfo(processInfo.hProcess, &memoryCounters, sizeof(memoryCounters))) {
        memoryCounters.PeakWorkingSetSize = 0;
    }

    sample->wallMs =
            ((double)(endTime.QuadPart - startTime.QuadPart) * 1000.0) / (double)frequency.QuadPart;
    sample->peakWorkingSetBytes = (uint64_t)memoryCounters.PeakWorkingSetSize;
    sample->exitCode = (int)exitCode;

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    CloseHandle(nullHandle);
    free(commandLine);
    return 1;
}
#else
static double zr_perf_timespec_diff_ms(const struct timespec *startTime, const struct timespec *endTime) {
    const double seconds = (double)(endTime->tv_sec - startTime->tv_sec) * 1000.0;
    const double nanoseconds = (double)(endTime->tv_nsec - startTime->tv_nsec) / 1000000.0;
    return seconds + nanoseconds;
}

static int zr_perf_run_command(const char *workingDirectory,
                               char *const *command,
                               SZrPerfRunSample *sample,
                               char *errorBuffer,
                               size_t errorBufferSize) {
    pid_t childPid;
    int status = 0;
    struct rusage usage;
    struct timespec startTime;
    struct timespec endTime;

    if (command == NULL || command[0] == NULL || sample == NULL) {
        return 0;
    }

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
        if (workingDirectory != NULL && workingDirectory[0] != '\0' && chdir(workingDirectory) != 0) {
            _exit(127);
        }
        if (nullFd >= 0) {
            dup2(nullFd, STDOUT_FILENO);
            dup2(nullFd, STDERR_FILENO);
            if (nullFd > STDERR_FILENO) {
                close(nullFd);
            }
        }
        execvp(command[0], command);
        _exit(errno == ENOENT ? 127 : 126);
    }

    if (wait4(childPid, &status, 0, &usage) < 0) {
        snprintf(errorBuffer, errorBufferSize, "wait4 failed: %s", strerror(errno));
        return 0;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &endTime) != 0) {
        snprintf(errorBuffer, errorBufferSize, "clock_gettime failed: %s", strerror(errno));
        return 0;
    }

    sample->wallMs = zr_perf_timespec_diff_ms(&startTime, &endTime);
#if defined(__APPLE__)
    sample->peakWorkingSetBytes = (uint64_t)usage.ru_maxrss;
#else
    sample->peakWorkingSetBytes = (uint64_t)usage.ru_maxrss * 1024ULL;
#endif
    if (WIFEXITED(status)) {
        sample->exitCode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        sample->exitCode = 128 + WTERMSIG(status);
    } else {
        sample->exitCode = 1;
    }

    return 1;
}
#endif

int main(int argc, char **argv) {
    const char *caseName = NULL;
    const char *jsonPath = NULL;
    const char *workingDirectory = "";
    int iterations = 0;
    int warmup = 0;
    int commandIndex = -1;
    int index;
    SZrPerfRunSample *samples = NULL;
    SZrPerfSummary summary;
    char errorBuffer[512];

    memset(&summary, 0, sizeof(summary));
    memset(errorBuffer, 0, sizeof(errorBuffer));

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--") == 0) {
            commandIndex = index + 1;
            break;
        }
        if (strcmp(argv[index], "--name") == 0 && index + 1 < argc) {
            caseName = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--iterations") == 0 && index + 1 < argc) {
            if (!zr_perf_parse_positive_int(argv[++index], &iterations)) {
                fprintf(stderr, "Invalid --iterations value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--warmup") == 0 && index + 1 < argc) {
            if (!zr_perf_parse_positive_int(argv[++index], &warmup)) {
                fprintf(stderr, "Invalid --warmup value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--json-out") == 0 && index + 1 < argc) {
            jsonPath = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--working-directory") == 0 && index + 1 < argc) {
            workingDirectory = argv[++index];
            continue;
        }

        zr_perf_print_usage(argv[0]);
        fprintf(stderr, "Unknown or incomplete option: %s\n", argv[index]);
        return 1;
    }

    if (caseName == NULL || jsonPath == NULL || iterations <= 0 || warmup <= 0 || commandIndex <= 0 ||
        commandIndex >= argc) {
        zr_perf_print_usage(argv[0]);
        return 1;
    }

    samples = (SZrPerfRunSample *)calloc((size_t)iterations, sizeof(*samples));
    if (samples == NULL) {
        fprintf(stderr, "Failed to allocate performance samples.\n");
        return 1;
    }

    for (index = 0; index < warmup; index++) {
        SZrPerfRunSample warmupSample;
        if (!zr_perf_run_command(workingDirectory, &argv[commandIndex], &warmupSample, errorBuffer, sizeof(errorBuffer))) {
            fprintf(stderr, "Warmup run failed for %s: %s\n", caseName, errorBuffer[0] != '\0' ? errorBuffer : "unknown error");
            free(samples);
            return 1;
        }
        if (warmupSample.exitCode != 0) {
            fprintf(stderr, "Warmup run failed for %s with exit code %d.\n", caseName, warmupSample.exitCode);
            free(samples);
            return 1;
        }
    }

    for (index = 0; index < iterations; index++) {
        if (!zr_perf_run_command(workingDirectory, &argv[commandIndex], &samples[index], errorBuffer, sizeof(errorBuffer))) {
            fprintf(stderr, "Measured run failed for %s: %s\n", caseName, errorBuffer[0] != '\0' ? errorBuffer : "unknown error");
            free(samples);
            return 1;
        }
        if (samples[index].exitCode != 0) {
            fprintf(stderr, "Measured run %d failed for %s with exit code %d.\n",
                    index + 1,
                    caseName,
                    samples[index].exitCode);
            free(samples);
            return 1;
        }
    }

    zr_perf_compute_summary(samples, iterations, &summary);
    if (!zr_perf_write_json_report(jsonPath,
                                   caseName,
                                   workingDirectory,
                                   &argv[commandIndex],
                                   iterations,
                                   warmup,
                                   samples,
                                   &summary)) {
        fprintf(stderr, "Failed to write JSON report: %s\n", jsonPath);
        free(samples);
        return 1;
    }

    printf("PERF_SUMMARY case=%s iterations=%d warmup=%d mean_wall_ms=%.3f median_wall_ms=%.3f min_wall_ms=%.3f "
           "max_wall_ms=%.3f stddev_wall_ms=%.3f mean_peak_bytes=%.0f median_peak_bytes=%.0f min_peak_bytes=%" PRIu64
           " max_peak_bytes=%" PRIu64 " mean_peak_mib=%.3f max_peak_mib=%.3f\n",
           caseName,
           iterations,
           warmup,
           summary.meanWallMs,
           summary.medianWallMs,
           summary.minWallMs,
           summary.maxWallMs,
           summary.stddevWallMs,
           summary.meanPeakWorkingSetBytes,
           summary.medianPeakWorkingSetBytes,
           summary.minPeakWorkingSetBytes,
           summary.maxPeakWorkingSetBytes,
           summary.meanPeakWorkingSetBytes / (1024.0 * 1024.0),
           (double)summary.maxPeakWorkingSetBytes / (1024.0 * 1024.0));

    free(samples);
    return 0;
}
