#include "backend.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static ZrgBatch batch;
static uint32_t last_hash;
static ZrgBackend *backend;
static ZrgBackendDesc backend_desc;
static bool completed;

bool zrg_backend_open(const char *name, const ZrgBackendDesc *desc) {
    zrg_backend_close();
    ZrgBackend *next = NULL;
    if (!strcmp(name, "null")) next = zrg_backend_null();
    if (!strcmp(name, "soft")) next = zrg_backend_soft();
#ifdef ZRG_HAS_GL33
    if (!strcmp(name, "gl")) next = zrg_backend_gl33();
#endif
    if (!next || !desc || !desc->width || !desc->height || desc->width > 16384 || desc->height > 16384)
        return false;
    next->failed = false;
    if (next->init && !next->init(next, desc)) {
        if (next->shutdown) next->shutdown(next);
        return false;
    }
    backend = next;
    backend_desc = *desc;
    return true;
}

void zrg_backend_close(void) {
    if (backend && backend->shutdown) backend->shutdown(backend);
    backend = NULL;
    completed = false;
}

void zrg_backend_present(void) {
    if (backend && backend->present) backend->present(backend);
}

bool zrg_backend_ok(void) { return !backend || !backend->failed; }

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
    last_hash = zrg_batch_hash(&batch);
    if (backend && backend->begin) backend->begin(backend, batch.camera_x, batch.camera_y);
    if (backend && backend->submit)
        backend->submit(backend, batch.sprites, batch.count, batch.texts, batch.text_count);
    batch.active = false;
    completed = !backend || !backend->failed;
    if (!completed) return -1;
    return (int)batch.count;
}

uint32_t zrg_snapshot(const char *path) {
    uint32_t hash = 0;
    (void)zrg_snapshot_result(path, &hash);
    return hash;
}

bool zrg_snapshot_result(const char *path, uint32_t *hash) {
    if (!hash) return false;
    /* Preserve the null API: paths are ignored; the hash describes the batch. */
    if (!backend || !backend->read_rgba8) { *hash = last_hash; return true; }
    if (!completed) return false;
    size_t size = (size_t)backend_desc.width * backend_desc.height * 4;
    uint8_t *pixels = malloc(size);
    if (!pixels) return false;
    bool ok = backend->read_rgba8(backend, pixels, backend_desc.width, backend_desc.height);
    if (ok && path && *path) {
        size_t length = strlen(path);
        if (length >= 4 && !strcmp(path + length - 4, ".png"))
            ok = zrg_write_png(path, pixels, backend_desc.width, backend_desc.height);
        else if (length >= 4 && !strcmp(path + length - 4, ".ppm"))
            ok = zrg_write_ppm(path, pixels, backend_desc.width, backend_desc.height);
        else ok = false;
    }
    if (ok) {
        uint32_t value = UINT32_C(2166136261);
        for (size_t i = 0; i < size; ++i) value = (value ^ pixels[i]) * UINT32_C(16777619);
        *hash = value & UINT32_C(0x7fffffff);
    }
    free(pixels);
    return ok;
}

bool zrg_snapshot_write(const char *path) {
    uint32_t hash;
    return path && *path && backend && backend->read_rgba8 && zrg_snapshot_result(path, &hash);
}
