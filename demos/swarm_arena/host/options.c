#include "host.h"

#include <stdio.h>
#include <string.h>

const char *zrg_gc_policy_name(ZrgGcPolicy policy) {
    static const char *names[] = {"auto", "slack", "manual"};
    return names[policy];
}

void zrg_options_usage(void) {
    puts("usage: zr_swarm --project FILE [--scenario smoke|arena_2k|swarm_10k|bullet_storm|particle_burst] [--lane dod|c]\n"
         "  [--frames 1..scenario-limit] [--seed 1..2147483646] [--render null] [--submit bulk]\n"
         "  [--gc-policy auto|slack|manual] [--gc-budget-us 0..1000000]\n"
         "  [--gc-minor-interval 1..1800] [--warmup 0] [--out DIR]\n"
         "  [--expect-hash 0..1000000006] [--help]\n"
         "  [--host-api library|session] [--instruction-budget 0..1000000000] [--rewind]\n"
         "Defaults: scenario length (smoke 120, others 1800), seed 12345, auto GC, 500us cap, minor every 30 frames,\n"
         "          out/swarm. DOD source or C reference execution; null platform uses the game bot.\n"
         "Session: DOD only, slack GC, 100000000 instructions/frame; --rewind requires >=720 frames.\n"
         "Requires BUILD_RUST_BINDING=ON and zrgame plugins on ZR_VM_NATIVE_PATH.");
}

static bool error(const char *option, const char *message) {
    fprintf(stderr, "SWARM_ERROR %s %s\n", option, message);
    return false;
}

static bool number(const char *option, const char *text, uint32_t min,
                   uint32_t max, uint32_t *result) {
    uint32_t value = 0;
    size_t digits = 1;
    for (uint32_t bound = max; bound >= 10; bound /= 10) ++digits;
    if (!text[0] || strlen(text) > digits) return error(option, "requires a bounded decimal integer");
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9' || value > max / 10 ||
            (value == max / 10 && (uint32_t)(*p - '0') > max % 10))
            return error(option, "requires a bounded decimal integer");
        value = value * 10 + (uint32_t)(*p - '0');
    }
    if (value < min) return error(option, "is below its supported minimum");
    *result = value;
    return true;
}

bool zrg_options_parse(int argc, char **argv, ZrgOptions *options) {
    static const char *names[] = {"--project", "--out", "--scenario", "--lane", "--frames",
        "--seed", "--render", "--submit", "--gc-policy", "--gc-budget-us",
        "--gc-minor-interval", "--warmup", "--expect-hash", "--help", "--host-api",
        "--instruction-budget", "--rewind"};
    bool seen[sizeof(names) / sizeof(names[0])] = {false};
    *options = (ZrgOptions){.out = "out/swarm", .scenario = "smoke", .lane = "dod", .frames = 120, .seed = 12345,
        .gc_budget_us = 500, .minor_interval = 30, .instruction_budget = 100000000,
        .expect_hash = -1, .gc_policy = ZRG_GC_AUTO};
    for (int i = 1; i < argc; ++i) {
        size_t key = 0;
        while (key < sizeof(names) / sizeof(names[0]) && strcmp(argv[i], names[key])) ++key;
        if (key == sizeof(names) / sizeof(names[0])) return error(argv[i], "is an unknown argument");
        if (seen[key]) return error(argv[i], "is a duplicate argument");
        seen[key] = true;
        const char *option = argv[i];
        if (key == 13) { options->help = true; continue; }
        if (key == 16) { options->rewind = true; continue; }
        if (++i == argc || !strncmp(argv[i], "--", 2)) return error(option, "is missing a value");
        const char *value = argv[i];
        uint32_t parsed;
        switch (key) {
            case 0: if (!*value) return error(option, "requires a path"); options->project = value; break;
            case 1: if (!*value) return error(option, "requires a directory"); options->out = value; break;
            case 2:
                if (strcmp(value, "smoke") && strcmp(value, "arena_2k") && strcmp(value, "swarm_10k") &&
                    strcmp(value, "bullet_storm") && strcmp(value, "particle_burst"))
                    return error(option, "must be smoke, arena_2k, swarm_10k, bullet_storm or particle_burst");
                options->scenario = value;
                break;
            case 3:
                if (strcmp(value, "dod") && strcmp(value, "c")) return error(option, "must be dod or c");
                options->lane = value;
                break;
            case 4: if (!number(option, value, 1, 1800, &options->frames)) return false; break;
            case 5: if (!number(option, value, 1, 2147483646, &options->seed)) return false; break;
            case 6: if (strcmp(value, "null")) return error(option, "must be null"); break;
            case 7: if (strcmp(value, "bulk")) return error(option, "must be bulk (mode C)"); break;
            case 8:
                if (!strcmp(value, "auto")) options->gc_policy = ZRG_GC_AUTO;
                else if (!strcmp(value, "slack")) options->gc_policy = ZRG_GC_SLACK;
                else if (!strcmp(value, "manual")) options->gc_policy = ZRG_GC_MANUAL;
                else return error(option, "must be auto, slack or manual");
                break;
            case 9: if (!number(option, value, 0, 1000000, &options->gc_budget_us)) return false; break;
            case 10: if (!number(option, value, 1, 1800, &options->minor_interval)) return false; break;
            case 11: if (!number(option, value, 0, 0, &parsed)) return false; break;
            case 12:
                if (!number(option, value, 0, 1000000006, &parsed)) return false;
                options->expect_hash = parsed;
                break;
            case 14:
                if (strcmp(value, "library") && strcmp(value, "session"))
                    return error(option, "must be library or session");
                options->session_api = !strcmp(value, "session");
                break;
            case 15:
                if (!number(option, value, 0, 1000000000, &options->instruction_budget)) return false;
                break;
            default: return false;
        }
    }
    uint32_t limit = !strcmp(options->scenario, "smoke") ? 120 : 1800;
    if (!seen[4]) options->frames = limit;
    if (options->frames > limit) return error("--frames", "exceeds the scenario limit");
    if (options->session_api) {
        if (strcmp(options->lane, "dod")) return error("--host-api session", "requires --lane dod");
        if (!seen[8]) options->gc_policy = ZRG_GC_SLACK;
        if (options->gc_policy != ZRG_GC_SLACK)
            return error("--host-api session", "requires --gc-policy slack");
        if (seen[10]) return error("--gc-minor-interval", "requires --host-api library");
    } else if (seen[15] || options->rewind) {
        return error("--instruction-budget/--rewind", "requires --host-api session");
    }
    if (options->rewind && options->frames < 720)
        return error("--rewind", "requires at least 720 frames (use --scenario arena_2k)");
    return options->help || options->project || error("--project", "is required");
}
