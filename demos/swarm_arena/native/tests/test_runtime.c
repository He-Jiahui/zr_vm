#define _POSIX_C_SOURCE 200809L
#include "backend.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(CONDITION) do { if (!(CONDITION)) { \
    fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #CONDITION); return false; \
} } while (0)

static bool batch_capacity(void) {
    const ZrgSprite sprite = {1, 2, 3, 4, UINT32_C(0xfedcba98)};
    zrg_begin_frame(0);
    CHECK(zrg_batch_can_append(ZRG_SPRITE_CAPACITY));
    for (uint32_t i = 0; i < ZRG_SPRITE_CAPACITY; ++i) CHECK(zrg_sprite(sprite));
    CHECK(!zrg_batch_can_append(1));
    CHECK(!zrg_batch_can_append(UINT32_MAX));
    CHECK(!zrg_sprite((ZrgSprite){9, 8, 7, 6, 0}));
    CHECK(zrg_batch()->count == ZRG_SPRITE_CAPACITY);
    CHECK(zrg_batch()->sprites[ZRG_SPRITE_CAPACITY - 1].rgba == sprite.rgba);
    CHECK(zrg_batch()->sprites[0].x == 1);
    CHECK(zrg_end_frame() == (int)ZRG_SPRITE_CAPACITY);
    CHECK(!zrg_sprite(sprite));
    CHECK(zrg_end_frame() == -1);
    return true;
}

static bool batch_reset_and_validation(void) {
    zrg_begin_frame(1);
    CHECK(zrg_batch()->count == 0 && zrg_batch()->text_count == 0);
    CHECK(!zrg_sprite((ZrgSprite){NAN, 0, 1, 0, 0}));
    CHECK(!zrg_sprite((ZrgSprite){0, 0, -1, 0, 0}));
    CHECK(!zrg_camera(INFINITY, 0));
    CHECK(zrg_batch()->count == 0);
    CHECK(zrg_sprite((ZrgSprite){-2, 3, 4, 0, 0}));
    CHECK(zrg_end_frame() == 1);
    zrg_begin_frame(2);
    CHECK(zrg_batch()->count == 0 && zrg_batch()->frame == 2);
    CHECK(zrg_end_frame() == 0);
    return true;
}

static bool null_hash_and_text(void) {
    char message[] = "hud";
    zrg_begin_frame(3);
    CHECK(zrg_camera(4, 5));
    CHECK(zrg_sprite((ZrgSprite){1, 2, 3, 0, UINT32_C(0xffffffff)}));
    CHECK(zrg_text(6, 7, message, 3));
    message[0] = 'X';
    CHECK(memcmp(zrg_batch()->texts[0].bytes, "hud", 3) == 0);
    CHECK(zrg_end_frame() == 1);
    uint32_t hash = zrg_snapshot("ignored.ppm");
    CHECK(hash == UINT32_C(299488900));
    zrg_begin_frame(4);
    CHECK(zrg_sprite((ZrgSprite){1, 2, 3, 0, UINT32_C(0xffffffff)}));
    CHECK(zrg_text(6, 7, "hud", 3));
    CHECK(zrg_end_frame() == 1);
    CHECK(zrg_snapshot("another.ppm") == hash); /* Frame/path do not affect content. */
    zrg_begin_frame(5);
    CHECK(zrg_camera(5, 5));
    CHECK(zrg_sprite((ZrgSprite){1, 2, 3, 0, UINT32_C(0xffffffff)}));
    CHECK(zrg_text(6, 7, "hud", 3));
    CHECK(zrg_end_frame() == 1);
    CHECK(zrg_snapshot("") != hash);
    CHECK(strcmp(zrg_backend_null()->name, "null") == 0);
    return true;
}

static bool text_capacity(void) {
    char large[ZRG_TEXT_BYTES] = {0};
    zrg_begin_frame(6);
    CHECK(!zrg_text(0, 0, large, sizeof(large)));
    for (uint32_t i = 0; i < ZRG_TEXT_CAPACITY; ++i) CHECK(zrg_text(0, 0, "x", 1));
    CHECK(!zrg_text(0, 0, "x", 1));
    CHECK(zrg_batch()->text_count == ZRG_TEXT_CAPACITY);
    CHECK(zrg_end_frame() == 0);
    return true;
}

static bool zone_aggregation(void) {
    ZrgZoneStats stats;
    char report[ZRG_ZONE_REPORT_BYTES];
    zrg_zones_reset();
    CHECK(zrg_zone_begin_at(2, 100));
    CHECK(zrg_zone_begin_at(3, 110));
    CHECK(zrg_zone_end_at(3, 125));
    CHECK(zrg_zone_end_at(2, 150));
    CHECK(zrg_zone_begin_at(2, 200));
    CHECK(zrg_zone_begin_at(2, 205)); /* Recursive zones measure inclusive time. */
    CHECK(zrg_zone_end_at(2, 210));
    CHECK(zrg_zone_end_at(2, 230));
    CHECK(zrg_zone_stats(2, &stats));
    CHECK(stats.count == 3 && stats.total_us == 85 && stats.max_us == 50);
    CHECK(zrg_zone_stats(3, &stats));
    CHECK(stats.count == 1 && stats.total_us == 15 && stats.max_us == 15);
    CHECK(zrg_zone_report(report, sizeof(report)));
    CHECK(strcmp(report, "{\"zones\":[{\"id\":2,\"count\":3,\"total_us\":85,\"max_us\":50},"
                         "{\"id\":3,\"count\":1,\"total_us\":15,\"max_us\":15}]}") == 0);
    CHECK(!zrg_zone_report(report, 12));
    return true;
}

static bool zone_errors_and_capacity(void) {
    ZrgZoneStats stats;
    char report[ZRG_ZONE_REPORT_BYTES];
    zrg_zones_reset();
    CHECK(!zrg_zone_end_at(0, 1));
    CHECK(!zrg_zone_begin_at(ZRG_ZONE_CAPACITY, 1));
    CHECK(zrg_zone_begin_at(1, 100));
    CHECK(!zrg_zone_end_at(0, 200));
    CHECK(!zrg_zone_end_at(1, 99));
    CHECK(zrg_zone_end_at(1, 105));
    CHECK(zrg_zone_stats(1, &stats) && stats.count == 1 && stats.total_us == 5);
    for (uint32_t i = 0; i < ZRG_ZONE_STACK_CAPACITY; ++i) CHECK(zrg_zone_begin_at(0, 100));
    CHECK(!zrg_zone_begin_at(0, 100));
    for (uint32_t i = 0; i < ZRG_ZONE_STACK_CAPACITY; ++i) CHECK(zrg_zone_end_at(0, 110));
    CHECK(zrg_zone_stats(0, &stats) && stats.count == ZRG_ZONE_STACK_CAPACITY && stats.total_us == 2560);
    zrg_zones_reset();
    for (uint32_t i = 0; i < ZRG_ZONE_CAPACITY; ++i) {
        CHECK(zrg_zone_begin_at(i, 0) && zrg_zone_end_at(i, UINT64_MAX));
    }
    CHECK(zrg_zone_report(report, sizeof(report))); /* Largest one-sample report fits. */
    return true;
}

static bool sinks(void) {
    char path[] = "/tmp/zr-swarm-gd-c2-sink-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0);
    close(fd);
    int64_t handle = zrg_sink_open(path);
    CHECK(handle > 0);
    CHECK(zrg_sink_write(handle, "one", 3));
    CHECK(zrg_sink_write(handle, "a\0b", 3));
    CHECK(zrg_sink_close(handle) && zrg_sink_close(handle));
    CHECK(!zrg_sink_write(handle, "stale", 5));
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    char bytes[16];
    CHECK(fread(bytes, 1, sizeof(bytes), file) == 8);
    CHECK(memcmp(bytes, "one\na\0b\n", 8) == 0);
    fclose(file);
    int64_t handles[ZRG_SINK_CAPACITY];
    for (uint32_t i = 0; i < ZRG_SINK_CAPACITY; ++i) {
        handles[i] = zrg_sink_open(path);
        CHECK(handles[i] > handle);
    }
    CHECK(zrg_sink_open(path) == 0);
    for (uint32_t i = 0; i < ZRG_SINK_CAPACITY; ++i) CHECK(zrg_sink_close(handles[i]));
    unlink(path);
    CHECK(zrg_sink_open("") == 0);
    return true;
}

static bool clock_and_drive(void) {
    uint64_t first = zrg_now_us();
    CHECK(first > 0 && zrg_now_us() >= first);
    CHECK(zrg_rss_kb() > 0);
    CHECK(!zrg_is_host_driven());
    zrg_set_host_driven(true);
    CHECK(zrg_is_host_driven());
    zrg_set_host_driven(false);
    return true;
}

int main(void) {
    bool (*const tests[])(void) = {batch_capacity, batch_reset_and_validation, null_hash_and_text,
        text_capacity, zone_aggregation, zone_errors_and_capacity, sinks, clock_and_drive};
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (!tests[i]()) { fprintf(stderr, "native test %zu failed\n", i + 1); return 1; }
    }
    puts("ZRG_NATIVE_OK tests=8");
    return 0;
}
