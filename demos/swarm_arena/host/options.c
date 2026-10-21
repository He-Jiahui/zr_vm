#include "host.h"

#include <stdio.h>
#include <string.h>

const char *zrg_gc_policy_name(ZrgGcPolicy policy) {
    static const char *names[] = {"auto", "slack", "manual"};
    return names[policy];
}

void zrg_options_usage(void) {
    puts("usage: zr_swarm --project FILE [--scenario smoke] [--lane dod]\n"
         "  [--frames 1..120] [--seed 1..2147483646] [--render null] [--submit bulk]\n"
         "  [--gc-policy auto|slack|manual] [--gc-budget-us 0..1000000]\n"
         "  [--gc-minor-interval 1..120] [--warmup 0] [--out DIR]\n"
         "  [--expect-hash 0..1000000006] [--help]\n"
         "Defaults: 120 frames, seed 12345, auto GC, 500us cap, minor every 30 frames,\n"
         "          out/swarm. Source execution; null platform uses the game bot.");
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
        "--gc-minor-interval", "--warmup", "--expect-hash", "--help"};
    bool seen[sizeof(names) / sizeof(names[0])] = {false};
    *options = (ZrgOptions){.out = "out/swarm", .frames = 120, .seed = 12345,
        .gc_budget_us = 500, .minor_interval = 30, .expect_hash = -1, .gc_policy = ZRG_GC_AUTO};
    for (int i = 1; i < argc; ++i) {
        size_t key = 0;
        while (key < sizeof(names) / sizeof(names[0]) && strcmp(argv[i], names[key])) ++key;
        if (key == sizeof(names) / sizeof(names[0])) return error(argv[i], "is an unknown argument");
        if (seen[key]) return error(argv[i], "is a duplicate argument");
        seen[key] = true;
        const char *option = argv[i];
        if (key == 13) { options->help = true; continue; }
        if (++i == argc || !strncmp(argv[i], "--", 2)) return error(option, "is missing a value");
        const char *value = argv[i];
        uint32_t parsed;
        switch (key) {
            case 0: if (!*value) return error(option, "requires a path"); options->project = value; break;
            case 1: if (!*value) return error(option, "requires a directory"); options->out = value; break;
            case 2: if (strcmp(value, "smoke")) return error(option, "must be smoke"); break;
            case 3: if (strcmp(value, "dod")) return error(option, "must be dod"); break;
            case 4: if (!number(option, value, 1, 120, &options->frames)) return false; break;
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
            case 10: if (!number(option, value, 1, 120, &options->minor_interval)) return false; break;
            case 11: if (!number(option, value, 0, 0, &parsed)) return false; break;
            case 12:
                if (!number(option, value, 0, 1000000006, &parsed)) return false;
                options->expect_hash = parsed;
                break;
            default: return false;
        }
    }
    return options->help || options->project || error("--project", "is required");
}
