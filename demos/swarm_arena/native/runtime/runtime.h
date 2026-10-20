#ifndef ZRG_RUNTIME_H
#define ZRG_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* One owner thread, one process-wide runtime shared by both descriptor plugins. */
#define ZRG_SPRITE_CAPACITY 65536u
#define ZRG_TEXT_CAPACITY 128u
#define ZRG_TEXT_BYTES 256u
#define ZRG_ZONE_CAPACITY 64u
#define ZRG_ZONE_STACK_CAPACITY 256u
#define ZRG_ZONE_REPORT_BYTES 16384u
#define ZRG_SINK_CAPACITY 16u

typedef struct ZrgSprite {
    float x, y, size, rot;
    uint32_t rgba;
} ZrgSprite;
_Static_assert(sizeof(ZrgSprite) == 20, "sprite batch instances must be 20 bytes");

typedef struct ZrgText {
    float x, y;
    uint32_t length;
    char bytes[ZRG_TEXT_BYTES];
} ZrgText;

typedef struct ZrgBatch {
    ZrgSprite sprites[ZRG_SPRITE_CAPACITY];
    ZrgText texts[ZRG_TEXT_CAPACITY];
    uint32_t count, text_count;
    float camera_x, camera_y;
    int64_t frame;
    bool active;
} ZrgBatch;

typedef struct ZrgZoneStats {
    uint64_t count, total_us, max_us;
} ZrgZoneStats;

uint64_t zrg_now_us(void);
int64_t zrg_rss_kb(void);
bool zrg_is_host_driven(void);
void zrg_set_host_driven(bool host_driven);
void zrg_zones_reset(void);
bool zrg_zone_begin_at(uint32_t id, uint64_t now);
bool zrg_zone_end_at(uint32_t id, uint64_t now);
bool zrg_zone_begin(uint32_t id);
bool zrg_zone_end(uint32_t id);
bool zrg_zone_stats(uint32_t id, ZrgZoneStats *out);
bool zrg_zone_report(char *out, size_t capacity);

const ZrgBatch *zrg_batch(void);
void zrg_begin_frame(int64_t frame);
bool zrg_camera(float x, float y);
bool zrg_batch_can_append(uint32_t count);
bool zrg_sprite(ZrgSprite sprite);
bool zrg_text(float x, float y, const char *bytes, size_t length);
int zrg_end_frame(void);
uint32_t zrg_snapshot(const char *path);

/* open returns 0 on failure; write appends a newline and flushes; close is idempotent. */
int64_t zrg_sink_open(const char *path);
bool zrg_sink_write(int64_t handle, const char *bytes, size_t length);
bool zrg_sink_close(int64_t handle);

#endif
