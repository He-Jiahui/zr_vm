#include "backend.h"

#include <string.h>

static uint32_t hash_byte(uint32_t hash, uint8_t byte) {
    return (hash ^ byte) * UINT32_C(16777619);
}

static uint32_t hash_word(uint32_t hash, uint32_t word) {
    for (unsigned i = 0; i < 4; ++i) hash = hash_byte(hash, (uint8_t)(word >> (i * 8)));
    return hash;
}

static uint32_t hash_float(uint32_t hash, float value) {
    uint32_t bits;
    /* Canonicalize signed zero and encode each word in little endian order. */
    if (value == 0.0f) value = 0.0f;
    memcpy(&bits, &value, sizeof(bits));
    return hash_word(hash, bits);
}

static uint32_t submit(const ZrgBatch *batch) {
    uint32_t hash = UINT32_C(2166136261);
    hash = hash_float(hash, batch->camera_x);
    hash = hash_float(hash, batch->camera_y);
    hash = hash_word(hash, batch->count);
    for (uint32_t i = 0; i < batch->count; ++i) {
        const ZrgSprite *sprite = &batch->sprites[i];
        hash = hash_float(hash, sprite->x);
        hash = hash_float(hash, sprite->y);
        hash = hash_float(hash, sprite->size);
        hash = hash_float(hash, sprite->rot);
        hash = hash_word(hash, sprite->rgba);
    }
    hash = hash_word(hash, batch->text_count);
    for (uint32_t i = 0; i < batch->text_count; ++i) {
        const ZrgText *text = &batch->texts[i];
        hash = hash_float(hash, text->x);
        hash = hash_float(hash, text->y);
        hash = hash_word(hash, text->length);
        for (uint32_t j = 0; j < text->length; ++j) hash = hash_byte(hash, (uint8_t)text->bytes[j]);
    }
    return hash & UINT32_C(0x7fffffff);
}

const ZrgBackend *zrg_backend_null(void) {
    static const ZrgBackend backend = {.name = "null", .submit = submit};
    return &backend;
}
