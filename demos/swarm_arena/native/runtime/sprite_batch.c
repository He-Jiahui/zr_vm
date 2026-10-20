#include "backend.h"

#include <math.h>
#include <string.h>

static ZrgBatch batch;
static uint32_t last_hash;

const ZrgBatch *zrg_batch(void) { return &batch; }

void zrg_begin_frame(int64_t frame) {
    batch.count = batch.text_count = 0;
    batch.frame = frame;
    batch.active = true;
}

bool zrg_camera(float x, float y) {
    if (!batch.active || !isfinite(x) || !isfinite(y)) return false;
    batch.camera_x = x;
    batch.camera_y = y;
    return true;
}

bool zrg_batch_can_append(uint32_t count) {
    return batch.active && count <= ZRG_SPRITE_CAPACITY - batch.count;
}

bool zrg_sprite(ZrgSprite sprite) {
    if (!zrg_batch_can_append(1) || !isfinite(sprite.x) || !isfinite(sprite.y) ||
        !isfinite(sprite.size) || sprite.size < 0.0f || !isfinite(sprite.rot)) return false;
    batch.sprites[batch.count++] = sprite;
    return true;
}

bool zrg_text(float x, float y, const char *bytes, size_t length) {
    if (!batch.active || !isfinite(x) || !isfinite(y) || !bytes ||
        batch.text_count == ZRG_TEXT_CAPACITY || length >= ZRG_TEXT_BYTES) return false;
    ZrgText *text = &batch.texts[batch.text_count++];
    text->x = x;
    text->y = y;
    text->length = (uint32_t)length;
    memcpy(text->bytes, bytes, length);
    text->bytes[length] = '\0';
    return true;
}

int zrg_end_frame(void) {
    if (!batch.active) return -1;
    last_hash = zrg_backend_null()->submit(&batch);
    batch.active = false;
    return (int)batch.count;
}

uint32_t zrg_snapshot(const char *path) {
    (void)path; /* Null backend: return the completed frame hash, create no image. */
    return last_hash;
}
