#include "host.h"

#include <stdio.h>
#include <string.h>

const char *zrg_gc_policy_name(ZrgGcPolicy policy) {
    static const char *names[] = {"auto", "slack", "manual"};
    return names[policy];
}

void zrg_options_usage(void) {
    puts("usage: zr_swarm --project FILE [--scenario smoke|arena_2k|swarm_10k|bullet_storm|particle_burst] [--lane dod|c]\n"
         "  [--frames 1..scenario-limit] [--seed 1..2147483646] [--render null|soft|gl] [--submit bulk]\n"
         "  [--headless] [--interactive] [--snapshot FRAME[,FRAME...]|FILE.ppm|FILE.png]\n"
         "  [--gc-policy auto|slack|manual] [--gc-budget-us 0..1000000]\n"
         "  [--gc-minor-interval 1..1800] [--warmup 0] [--out DIR]\n"
         "  [--expect-hash 0..1000000006] [--help]\n"
         "Defaults: scenario length (smoke 120, others 1800), seed 12345, auto GC, 500us cap, minor every 30 frames,\n"
         "          out/swarm. GL opens a window unless --headless; WASD requires windowed GL.\n"
         "          Headless and noninteractive runs use the deterministic game bot.");
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

static bool snapshots(const char *text, ZrgOptions *options) {
    options->snapshot = text;
    size_t length = strlen(text);
    if (length >= 5 && (!strcmp(text + length - 4, ".ppm") || !strcmp(text + length - 4, ".png"))) return true;
    options->snapshot_sequence = true;
    const char *p = text;
    do {
        const char *end = strchr(p, ',');
        size_t count = end ? (size_t)(end - p) : strlen(p);
        char token[5];
        if (!count || count >= sizeof(token))
            return error("--snapshot", "requires frame numbers or a .ppm/.png path");
        memcpy(token, p, count); token[count] = '\0';
        uint32_t frame;
        if (!number("--snapshot", token, 1, 1800, &frame)) return false;
        if (options->snapshot_frames[frame - 1]) return error("--snapshot", "has duplicate frames");
        options->snapshot_frames[frame - 1] = true;
        if (!end) break;
        p = end + 1;
    } while (true);
    return true;
}

bool zrg_options_parse(int argc, char **argv, ZrgOptions *options) {
    static const char *names[] = {"--project", "--out", "--scenario", "--lane", "--frames",
        "--seed", "--render", "--submit", "--gc-policy", "--gc-budget-us",
        "--gc-minor-interval", "--warmup", "--expect-hash", "--help", "--headless", "--interactive", "--snapshot"};
    bool seen[sizeof(names) / sizeof(names[0])] = {false};
    *options = (ZrgOptions){.out = "out/swarm", .scenario = "smoke", .lane = "dod", .render = "null", .frames = 120, .seed = 12345,
        .gc_budget_us = 500, .minor_interval = 30, .expect_hash = -1, .gc_policy = ZRG_GC_AUTO};
    for (int i = 1; i < argc; ++i) {
        size_t key = 0;
        while (key < sizeof(names) / sizeof(names[0]) && strcmp(argv[i], names[key])) ++key;
        if (key == sizeof(names) / sizeof(names[0])) return error(argv[i], "is an unknown argument");
        if (seen[key]) return error(argv[i], "is a duplicate argument");
        seen[key] = true;
        const char *option = argv[i];
        if (key == 13) { options->help = true; continue; }
        if (key == 14) { options->headless = true; continue; }
        if (key == 15) { options->interactive = true; continue; }
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
            case 6:
                if (strcmp(value, "null") && strcmp(value, "soft") && strcmp(value, "gl"))
                    return error(option, "must be null, soft or gl");
                options->render = value;
                break;
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
            case 16:
                if (!snapshots(value, options)) return false;
                break;
            default: return false;
        }
    }
    uint32_t limit = !strcmp(options->scenario, "smoke") ? 120 : 1800;
    if (!seen[4]) options->frames = limit;
    if (options->frames > limit) return error("--frames", "exceeds the scenario limit");
    if (options->interactive && (options->headless || strcmp(options->render, "gl")))
        return error("--interactive", "requires windowed --render gl (omit --headless)");
    if (options->snapshot && !strcmp(options->render, "null"))
        return error("--snapshot", "requires --render soft or gl");
    for (uint32_t f = options->frames; f < 1800; ++f)
        if (options->snapshot_frames[f]) return error("--snapshot", "exceeds the configured frame count");
#ifndef ZRG_HAS_GL33
    if (!strcmp(options->render, "gl")) return error("--render", "GL support unavailable in this build");
#endif
    return options->help || options->project || error("--project", "is required");
}
