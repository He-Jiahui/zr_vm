#include "backend.h"

#include <math.h>
#include <string.h>

/* The public 20-byte instance has no shape tag. Zero rotation selects a
 * circle; nonzero rotation selects a square. Size is diameter/side in world
 * units. Both use pixel-center coverage, without filtering or antialiasing.
 * Keep float contraction disabled for this translation unit. */
static struct {
    uint8_t pixels[ZRG_RENDER_WIDTH * ZRG_RENDER_HEIGHT * 4];
    double camera_x, camera_y;
} soft;

static int init(ZrgBackend *backend, const ZrgBackendDesc *desc) {
    (void)backend;
    return desc->width == ZRG_RENDER_WIDTH && desc->height == ZRG_RENDER_HEIGHT;
}

static void begin(ZrgBackend *backend, float camera_x, float camera_y) {
    (void)backend;
    soft.camera_x = camera_x; soft.camera_y = camera_y;
    for (size_t i = 0; i < sizeof(soft.pixels); i += 4) {
        soft.pixels[i] = 16; soft.pixels[i + 1] = 20;
        soft.pixels[i + 2] = 28; soft.pixels[i + 3] = 255;
    }
}

static void blend(unsigned x, unsigned y, uint32_t rgba) {
    uint8_t *pixel = soft.pixels + ((size_t)y * ZRG_RENDER_WIDTH + x) * 4;
    unsigned alpha = rgba & 255u;
    for (unsigned channel = 0; channel < 3; ++channel) {
        unsigned source = (rgba >> (24 - channel * 8)) & 255u;
        /* Integer source-over, round to nearest. The background is opaque. */
        pixel[channel] = (uint8_t)((source * alpha + pixel[channel] * (255 - alpha) + 127) / 255);
    }
}

/* Clamp in floating point before converting to an integer: all finite
 * float32 world coordinates/sizes are valid, including offscreen extremes. */
static unsigned bound(double value, unsigned limit) {
    if (value <= 0) return 0;
    if (value >= limit) return limit;
    return (unsigned)value;
}

static void sprite(const ZrgSprite *instance) {
    if (instance->size == 0 || !(instance->rgba & 255u)) return;
    double cx = ((double)instance->x - soft.camera_x) * 0.5 + ZRG_RENDER_WIDTH * 0.5;
    double cy = ((double)instance->y - soft.camera_y) * 0.5 + ZRG_RENDER_HEIGHT * 0.5;
    double half = (double)instance->size * 0.25;
    bool circle = instance->rot == 0;
    double c = circle ? 1 : cos((double)instance->rot);
    double s = circle ? 0 : sin((double)instance->rot);
    double extent = circle ? half : half * (fabs(c) + fabs(s));
    unsigned x0 = bound(floor(cx - extent), ZRG_RENDER_WIDTH);
    unsigned x1 = bound(ceil(cx + extent), ZRG_RENDER_WIDTH);
    unsigned y0 = bound(floor(cy - extent), ZRG_RENDER_HEIGHT);
    unsigned y1 = bound(ceil(cy + extent), ZRG_RENDER_HEIGHT);
    for (unsigned y = y0; y < y1; ++y) {
        double dy = y + 0.5 - cy;
        for (unsigned x = x0; x < x1; ++x) {
            double dx = x + 0.5 - cx;
            bool covered;
            if (circle) covered = dx * dx + dy * dy <= half * half;
            else {
                double u = dx * c + dy * s, v = -dx * s + dy * c;
                covered = u >= -half && u < half && v >= -half && v < half;
            }
            if (covered) blend(x, y, instance->rgba);
        }
    }
}

/* The same original 5x7 row masks as backend_gl33.c, with no external font
 * dependency. Lowercase maps to uppercase; unsupported bytes draw '?'. */
static const uint8_t *glyph(unsigned char c) {
    static const uint8_t rows[][7] = {
        {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
        {14,17,17,15,1,1,14},
        {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
        {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
        {7,2,2,2,2,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
        {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
        {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
        {14,17,1,2,4,0,4}, {0,0,4,0,4,0,0}, {0,0,0,31,0,0,0}, {0,0,0,0,0,0,4}
    };
    if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');
    if (c >= '0' && c <= '9') return rows[c - '0'];
    if (c >= 'A' && c <= 'Z') return rows[10 + c - 'A'];
    if (c == ':') return rows[37];
    if (c == '-') return rows[38];
    if (c == '.') return rows[39];
    return rows[36];
}

static void text(const ZrgText *message) {
    double x = floor((double)message->x + 0.5), y = floor((double)message->y + 0.5);
    double start = x;
    for (uint32_t i = 0; i < message->length; ++i) {
        unsigned char byte = (unsigned char)message->bytes[i];
        if (byte == '\n') { x = start; y += 8; continue; }
        if (byte != ' ') {
            const uint8_t *rows = glyph(byte);
            for (unsigned row = 0; row < 7; ++row)
                for (unsigned col = 0; col < 5; ++col) {
                    double px = x + col, py = y + row;
                    if ((rows[row] & (1u << (4 - col))) && px >= 0 && py >= 0 &&
                        px < ZRG_RENDER_WIDTH && py < ZRG_RENDER_HEIGHT)
                        blend((unsigned)px, (unsigned)py, UINT32_C(0xe8edf5ff));
                }
        }
        x += 6;
    }
}

static void submit(ZrgBackend *backend, const ZrgSprite *sprites, uint32_t count,
                   const ZrgText *texts, uint32_t ntext) {
    (void)backend;
    for (uint32_t i = 0; i < count; ++i) sprite(&sprites[i]);
    for (uint32_t i = 0; i < ntext; ++i) text(&texts[i]);
}

static int read_rgba8(ZrgBackend *backend, uint8_t *out, uint32_t width, uint32_t height) {
    if (backend->failed || !out || width != ZRG_RENDER_WIDTH || height != ZRG_RENDER_HEIGHT) return 0;
    memcpy(out, soft.pixels, sizeof(soft.pixels));
    return 1;
}

ZrgBackend *zrg_backend_soft(void) {
    static ZrgBackend backend = {.name = "soft", .init = init, .begin = begin,
        .submit = submit, .read_rgba8 = read_rgba8};
    return &backend;
}
