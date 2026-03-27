#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char* executable) {
    printf("ZR-VM test runner is a thin wrapper around ctest.\n");
    printf("ctest is the only source of truth for discovery and execution.\n\n");
    printf("Usage:\n");
    printf("  %s --help\n", executable);
    printf("  %s --ctest [ctest arguments...]\n\n", executable);
    printf("Examples:\n");
    printf("  %s --ctest --output-on-failure\n", executable);
    printf("  %s --ctest --output-on-failure -R scripts_tests\n", executable);
}

int main(int argc, char* argv[]) {
    if (argc == 1 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "--ctest") != 0) {
        fprintf(stderr, "Unknown argument: %s\n\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }

    {
        char command[4096];
        int offset = snprintf(command, sizeof(command), "ctest");

        if (offset < 0 || (size_t)offset >= sizeof(command)) {
            return 1;
        }

        for (int i = 2; i < argc; ++i) {
            int written = snprintf(command + offset, sizeof(command) - (size_t)offset, " %s", argv[i]);
            if (written < 0 || (size_t)written >= sizeof(command) - (size_t)offset) {
                fprintf(stderr, "ctest command line too long.\n");
                return 1;
            }
            offset += written;
        }

        return system(command);
    }
}
