#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#endif

static void fixture_sleep_ms(unsigned int milliseconds) {
#if defined(_WIN32)
    Sleep(milliseconds);
#else
    struct timespec delay;
    delay.tv_sec = (time_t)(milliseconds / 1000U);
    delay.tv_nsec = (long)(milliseconds % 1000U) * 1000000L;
    nanosleep(&delay, NULL);
#endif
}

static int fixture_write_pid(const char *path, uint64_t processId) {
    FILE *file;

    if (path == NULL) {
        return 0;
    }
    file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }
    fprintf(file, "%llu\n", (unsigned long long)processId);
    return fclose(file) == 0;
}

static int fixture_process_is_alive(uint64_t processId) {
#if defined(_WIN32)
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)processId);
    int isAlive;
    if (process == NULL) {
        return 0;
    }
    isAlive = WaitForSingleObject(process, 0U) == WAIT_TIMEOUT;
    CloseHandle(process);
    return isAlive;
#else
    if (processId == 0U || processId > (uint64_t)INT32_MAX) {
        return 0;
    }
    return kill((pid_t)processId, 0) == 0 || errno == EPERM;
#endif
}

static int fixture_wait_for_process_death(uint64_t processId) {
    unsigned int attempt;
    for (attempt = 0U; attempt < 40U; attempt++) {
        if (!fixture_process_is_alive(processId)) {
            return 1;
        }
        fixture_sleep_ms(50U);
    }
    return !fixture_process_is_alive(processId);
}

static int fixture_run_hanging_process(const char *executable, const char *pidFile) {
#if defined(_WIN32)
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    char commandLine[1024];

    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);
    if (snprintf(commandLine,
                 sizeof(commandLine),
                 "\"%s\" --fixture-descendant-child --descendant-pid-file \"%s\"",
                 executable,
                 pidFile) < 0 ||
        !CreateProcessA(executable, commandLine, NULL, NULL, FALSE, 0U, NULL, NULL, &startup, &process)) {
        return 0;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
#else
    pid_t child = fork();
    (void)executable;
    if (child < 0) {
        return 0;
    }
    if (child == 0) {
        if (!fixture_write_pid(pidFile, (uint64_t)getpid())) {
            _exit(31);
        }
        fixture_sleep_ms(30000U);
        _exit(0);
    }
#endif
    fixture_sleep_ms(30000U);
    return 0;
}

static int fixture_parse_positive_u64(const char *text, uint64_t maximum, uint64_t *value) {
    uint64_t parsed = 0U;
    const unsigned char *cursor = (const unsigned char *)text;

    if (cursor == NULL || cursor[0] < '1' || cursor[0] > '9' || value == NULL) {
        return 0;
    }
    while (*cursor >= '0' && *cursor <= '9') {
        const uint64_t digit = (uint64_t)(*cursor - '0');
        if (parsed > (maximum - digit) / 10U) {
            return 0;
        }
        parsed = parsed * 10U + digit;
        cursor++;
    }
    if (*cursor != '\0') {
        return 0;
    }
    *value = parsed;
    return 1;
}

static int fixture_parse_request(const char *line,
                                 char *kind,
                                 size_t kindSize,
                                 int *index,
                                 uint64_t *repetitions) {
    char trailing = '\0';
    char canonical[96];
    char indexText[32];
    char repetitionText[32];
    uint64_t parsedIndex;

    if (line == NULL || kind == NULL || kindSize < 7U || index == NULL || repetitions == NULL) {
        return 0;
    }
    if (sscanf(line, "%6s %31s %31s %c", kind, indexText, repetitionText, &trailing) != 3 ||
        (strcmp(kind, "WARMUP") != 0 && strcmp(kind, "RUN") != 0) ||
        !fixture_parse_positive_u64(indexText, 2147483647U, &parsedIndex) ||
        !fixture_parse_positive_u64(repetitionText, 1048576U, repetitions)) {
        return 0;
    }
    snprintf(canonical,
             sizeof(canonical),
             "%s %llu %llu",
             kind,
             (unsigned long long)parsedIndex,
             (unsigned long long)*repetitions);
    if (strcmp(line, canonical) != 0) {
        return 0;
    }
    *index = (int)parsedIndex;
    return 1;
}

static int fixture_run_server(const char *behavior, unsigned int sleepMs) {
    char line[256];

    if (strcmp(behavior, "ready_timeout") == 0) {
        fixture_sleep_ms(1500U);
    }
    if (strcmp(behavior, "early_exit_before_ready") == 0) {
        return 7;
    }
    if (strcmp(behavior, "malformed_ready") == 0) {
        puts("BROKEN_READY");
        fflush(stdout);
        return 8;
    }
    if (strcmp(behavior, "ready_contract_mismatch") == 0) {
        puts("READY wrong-contract");
    } else {
        puts("READY fixture-contract-v1");
    }
    fflush(stdout);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        char kind[8];
        int requestIndex = 0;
        uint64_t repetitions = 0U;
        uint64_t repetition;
        uint64_t checksum = UINT64_C(424242);

        if (strcmp(line, "STOP\n") == 0 || strcmp(line, "STOP\r\n") == 0) {
            if (strcmp(behavior, "stop_timeout") == 0) {
                fixture_sleep_ms(1500U);
            }
            return strcmp(behavior, "stop_nonzero") == 0 ? 9 : 0;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (!fixture_parse_request(line, kind, sizeof(kind), &requestIndex, &repetitions)) {
            puts("ERROR 0 malformed-request");
            fflush(stdout);
            return 10;
        }
        if (strcmp(behavior, "early_exit_after_ready") == 0) {
            return 11;
        }
        if (strcmp(behavior, "request_timeout") == 0) {
            fixture_sleep_ms(1500U);
            continue;
        }
        if (strcmp(behavior, "error_response") == 0) {
            printf("ERROR %d fixture-error\n", requestIndex);
            fflush(stdout);
            return 12;
        }
        if (strcmp(behavior, "malformed_response") == 0) {
            puts("NOT_DONE");
            fflush(stdout);
            return 13;
        }
        if (strcmp(behavior, "overlong_response") == 0) {
            int characterIndex;
            for (characterIndex = 0; characterIndex < 600; characterIndex++) {
                fputc('A', stdout);
            }
            fputc('\n', stdout);
            fflush(stdout);
            return 14;
        }
        for (repetition = 0U; repetition < repetitions; repetition++) {
            const uint64_t currentChecksum = strcmp(behavior, "repetition_checksum_mismatch") == 0 && repetition == 1U
                                                     ? UINT64_C(424243)
                                                     : UINT64_C(424242);
            if (strcmp(behavior, "unstable_timing") == 0 && strcmp(kind, "RUN") == 0) {
                fixture_sleep_ms((requestIndex % 2) == 0 ? 30U : 5U);
            } else if (sleepMs > 0U) {
                fixture_sleep_ms(sleepMs);
            }
            if (repetition > 0U && currentChecksum != checksum) {
                printf("ERROR %d repetition-checksum-mismatch\n", requestIndex);
                fflush(stdout);
                return 16;
            }
            checksum = currentChecksum;
        }
        if (strcmp(behavior, "wrong_index") == 0) {
            printf("DONE %d 424242\n", requestIndex + 1);
        } else if (strcmp(behavior, "done_checksum_mismatch") == 0) {
            printf("DONE %d 999999\n", requestIndex);
        } else if (strcmp(behavior, "leading_space_response") == 0) {
            printf(" DONE %d 424242\n", requestIndex);
        } else if (strcmp(behavior, "tab_response") == 0) {
            printf("DONE\t%d\t424242\n", requestIndex);
        } else if (strcmp(behavior, "double_space_response") == 0) {
            printf("DONE  %d 424242\n", requestIndex);
        } else if (strcmp(behavior, "plus_index_response") == 0) {
            printf("DONE +%d 424242\n", requestIndex);
        } else if (strcmp(behavior, "leading_zero_index_response") == 0) {
            printf("DONE 0%d 424242\n", requestIndex);
        } else {
            printf("DONE %d %llu\n", requestIndex, (unsigned long long)checksum);
        }
        fflush(stdout);
    }

    return 15;
}

int main(int argc, char **argv) {
    const char *behavior = "normal";
    const char *parseRequest = NULL;
    unsigned int sleepMs = 0U;
    unsigned int processSleepMs = 0U;
    const char *descendantPidFile = NULL;
    int processHang = 0;
    int descendantChild = 0;
    uint64_t checkDeadProcessId = 0U;
    int index;

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--fixture-behavior") == 0 && index + 1 < argc) {
            behavior = argv[++index];
        } else if (strcmp(argv[index], "--fixture-sleep-ms") == 0 && index + 1 < argc) {
            sleepMs = (unsigned int)strtoul(argv[++index], NULL, 10);
        } else if (strcmp(argv[index], "--fixture-process-ms") == 0 && index + 1 < argc) {
            processSleepMs = (unsigned int)strtoul(argv[++index], NULL, 10);
        } else if (strcmp(argv[index], "--fixture-process-hang") == 0) {
            processHang = 1;
        } else if (strcmp(argv[index], "--fixture-descendant-child") == 0) {
            descendantChild = 1;
        } else if (strcmp(argv[index], "--descendant-pid-file") == 0 && index + 1 < argc) {
            descendantPidFile = argv[++index];
        } else if (strcmp(argv[index], "--check-process-dead") == 0 && index + 1 < argc) {
            if (!fixture_parse_positive_u64(argv[++index], UINT64_MAX, &checkDeadProcessId)) {
                return 22;
            }
        } else if (strcmp(argv[index], "--parse-request") == 0 && index + 1 < argc) {
            parseRequest = argv[++index];
        } else {
            return 20;
        }
    }
    if (checkDeadProcessId != 0U) {
        return fixture_wait_for_process_death(checkDeadProcessId) ? 0 : 23;
    }
    if (descendantChild) {
#if defined(_WIN32)
        if (!fixture_write_pid(descendantPidFile, (uint64_t)GetCurrentProcessId())) {
            return 24;
        }
        fixture_sleep_ms(30000U);
        return 0;
#else
        return 25;
#endif
    }
    if (parseRequest != NULL) {
        char kind[8];
        int requestIndex = 0;
        uint64_t repetitions = 0U;
        return fixture_parse_request(parseRequest, kind, sizeof(kind), &requestIndex, &repetitions) ? 0 : 21;
    }
    if (processSleepMs > 0U) {
        fixture_sleep_ms(processSleepMs);
        return 0;
    }
    if (processHang) {
        return fixture_run_hanging_process(argv[0], descendantPidFile);
    }
    return fixture_run_server(behavior, sleepMs);
}
