#include "backend.h"
#include <glad/gl.h>

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    GLuint program, vao, quad, instances, framebuffer, texture;
    GLint camera;
    uint32_t width, height;
    float camera_x, camera_y;
    ZrgSprite *scratch;
    size_t capacity;
} gl;

static void check_gl(ZrgBackend *backend, const char *operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        fprintf(stderr, "SWARM_ERROR gl33 %s: GL error 0x%x\n", operation, error);
        backend->failed = true;
    }
}

static GLuint shader(GLenum type, const char *source) {
    GLuint object = glCreateShader(type);
    glShaderSource(object, 1, &source, NULL);
    glCompileShader(object);
    GLint ok;
    glGetShaderiv(object, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(object, sizeof(log), NULL, log);
        fprintf(stderr, "SWARM_ERROR gl33 shader: %s\n", log);
        glDeleteShader(object);
        return 0;
    }
    return object;
}

static int init(ZrgBackend *backend, const ZrgBackendDesc *desc) {
    static const char *vertex =
        "#version 330 core\n"
        "layout(location=0) in vec2 corner;\n"
        "layout(location=1) in vec4 sprite;\n"
        "layout(location=2) in uint rgba;\n"
        "uniform vec2 camera;\n"
        "flat out vec4 color;\n"
        "void main() {\n"
        " float c=cos(sprite.w), s=sin(sprite.w);\n"
        " vec2 q=mat2(c,s,-s,c)*(corner*sprite.z);\n"
        " vec2 p=(sprite.xy+q-camera)/vec2(640.0,360.0);\n"
        " gl_Position=vec4(p.x,-p.y,0.0,1.0);\n"
        " color=vec4((rgba>>24u)&255u,(rgba>>16u)&255u,(rgba>>8u)&255u,rgba&255u)/255.0;\n"
        "}\n";
    static const char *fragment =
        "#version 330 core\n"
        "flat in vec4 color;\n"
        "out vec4 pixel;\n"
        "void main() { pixel=color; }\n";
    static const float corners[] = {-0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
    if (!GLAD_GL_VERSION_3_3) return 0;
    gl.width = desc->width; gl.height = desc->height;
    GLuint vs = shader(GL_VERTEX_SHADER, vertex);
    GLuint fs = shader(GL_FRAGMENT_SHADER, fragment);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    gl.program = glCreateProgram();
    glAttachShader(gl.program, vs); glAttachShader(gl.program, fs);
    glLinkProgram(gl.program);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint linked;
    glGetProgramiv(gl.program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[2048];
        glGetProgramInfoLog(gl.program, sizeof(log), NULL, log);
        fprintf(stderr, "SWARM_ERROR gl33 link: %s\n", log);
        return 0;
    }
    gl.camera = glGetUniformLocation(gl.program, "camera");
    glGenVertexArrays(1, &gl.vao); glBindVertexArray(gl.vao);
    glGenBuffers(1, &gl.quad); glBindBuffer(GL_ARRAY_BUFFER, gl.quad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), NULL);
    glGenBuffers(1, &gl.instances); glBindBuffer(GL_ARRAY_BUFFER, gl.instances);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ZrgSprite), NULL);
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(ZrgSprite), (void *)offsetof(ZrgSprite, rgba));
    glVertexAttribDivisor(2, 1);
    glGenTextures(1, &gl.texture); glBindTexture(GL_TEXTURE_2D, gl.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)gl.width, (GLsizei)gl.height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenFramebuffers(1, &gl.framebuffer); glBindFramebuffer(GL_FRAMEBUFFER, gl.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl.texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "SWARM_ERROR gl33 incomplete RGBA8 framebuffer\n");
        return 0;
    }
    check_gl(backend, "init");
    fprintf(stderr, "SWARM_GL vendor=%s renderer=%s version=%s\n",
        glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));
    return !backend->failed;
}

static void begin(ZrgBackend *backend, float camera_x, float camera_y) {
    (void)backend;
    gl.camera_x = camera_x; gl.camera_y = camera_y;
    glBindFramebuffer(GL_FRAMEBUFFER, gl.framebuffer);
    glViewport(0, 0, (GLsizei)gl.width, (GLsizei)gl.height);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DITHER); glDisable(GL_MULTISAMPLE); glDisable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(16.0f / 255.0f, 20.0f / 255.0f, 28.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

/* Original 5x7 row masks. HUD coordinates are screen pixels; lowercase uses
 * the uppercase glyph. Text pixels join the same instance stream as sprites. */
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

static void submit(ZrgBackend *backend, const ZrgSprite *sprites, uint32_t count,
                   const ZrgText *texts, uint32_t ntext) {
    size_t maximum = count;
    for (uint32_t i = 0; i < ntext; ++i) maximum += (size_t)texts[i].length * 35;
    const ZrgSprite *data = sprites;
    if (ntext) {
        if (maximum > gl.capacity) {
            ZrgSprite *next = realloc(gl.scratch, maximum * sizeof(*next));
            if (!next) { backend->failed = true; return; }
            gl.scratch = next; gl.capacity = maximum;
        }
        if (count) memcpy(gl.scratch, sprites, (size_t)count * sizeof(*sprites));
        for (uint32_t i = 0; i < ntext; ++i) {
            float x = texts[i].x, y = texts[i].y;
            for (uint32_t j = 0; j < texts[i].length; ++j) {
                unsigned char c = (unsigned char)texts[i].bytes[j];
                if (c == '\n') { x = texts[i].x; y += 8; continue; }
                if (c != ' ') {
                    const uint8_t *rows = glyph(c);
                    for (unsigned row = 0; row < 7; ++row)
                        for (unsigned col = 0; col < 5; ++col)
                            if (rows[row] & (1u << (4 - col)))
                                gl.scratch[count++] = (ZrgSprite){
                                    gl.camera_x + (x + col + 0.5f) * 1280.0f / gl.width - 640.0f,
                                    gl.camera_y + (y + row + 0.5f) * 720.0f / gl.height - 360.0f,
                                    1280.0f / gl.width, 0.0f, UINT32_C(0xe8edf5ff)};
                }
                x += 6;
            }
        }
        data = gl.scratch;
    }
    glUseProgram(gl.program);
    glUniform2f(gl.camera, gl.camera_x, gl.camera_y);
    glBindVertexArray(gl.vao);
    glBindBuffer(GL_ARRAY_BUFFER, gl.instances);
    /* Orphan the streaming store, then upload once and draw the entire frame. */
    GLsizeiptr bytes = (GLsizeiptr)((size_t)count * sizeof(*data));
    glBufferData(GL_ARRAY_BUFFER, bytes, NULL, GL_STREAM_DRAW);
    if (count) glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, data);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)count);
    check_gl(backend, "submit");
}

static void present(ZrgBackend *backend) {
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport); /* Platform supplies framebuffer size. */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gl.framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, (GLint)gl.width, (GLint)gl.height,
                      0, 0, viewport[2], viewport[3], GL_COLOR_BUFFER_BIT, GL_NEAREST);
    check_gl(backend, "present");
}

static int read_rgba8(ZrgBackend *backend, uint8_t *out, uint32_t width, uint32_t height) {
    if (backend->failed || !out || width != gl.width || height != gl.height) return 0;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gl.framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, (GLsizei)width, (GLsizei)height, GL_RGBA, GL_UNSIGNED_BYTE, out);
    /* glReadPixels is bottom-to-top; the shared image contract is top-to-bottom. */
    size_t stride = (size_t)width * 4;
    for (uint32_t y = 0; y < height / 2; ++y) {
        uint8_t *top = out + (size_t)y * stride;
        uint8_t *bottom = out + (size_t)(height - y - 1) * stride;
        for (size_t x = 0; x < stride; ++x) {
            uint8_t byte = top[x]; top[x] = bottom[x]; bottom[x] = byte;
        }
    }
    check_gl(backend, "read_rgba8");
    return !backend->failed;
}

static void shutdown(ZrgBackend *backend) {
    (void)backend;
    glDeleteFramebuffers(1, &gl.framebuffer); glDeleteTextures(1, &gl.texture);
    glDeleteBuffers(1, &gl.instances); glDeleteBuffers(1, &gl.quad);
    glDeleteVertexArrays(1, &gl.vao);
    if (gl.program) glDeleteProgram(gl.program);
    free(gl.scratch);
    memset(&gl, 0, sizeof(gl));
}

ZrgBackend *zrg_backend_gl33(void) {
    static ZrgBackend backend = {.name = "gl33", .init = init, .begin = begin,
        .submit = submit, .present = present, .read_rgba8 = read_rgba8, .shutdown = shutdown};
    return &backend;
}
