#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#define ZR_FFI_FIXTURE_EXPORT __declspec(dllexport)
#define ZR_FFI_FIXTURE_STDCALL __stdcall
#else
#include <pthread.h>
#include <unistd.h>
#define ZR_FFI_FIXTURE_EXPORT
#define ZR_FFI_FIXTURE_STDCALL
#endif

typedef struct ZrFfiFixturePoint {
    int32_t x;
    int32_t y;
} ZrFfiFixturePoint;

typedef enum ZrFfiFixtureMode {
    ZR_FFI_FIXTURE_MODE_OFF = 0,
    ZR_FFI_FIXTURE_MODE_ON = 1
} ZrFfiFixtureMode;

typedef double (*ZrFfiFixtureUnaryCallback)(double value);

static const char *kZrFfiFixtureVersion = "1.2.3-fixture";

ZR_FFI_FIXTURE_EXPORT const char *zr_ffi_version_string(void);
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_add_i32(int32_t lhs, int32_t rhs);
ZR_FFI_FIXTURE_EXPORT double zr_ffi_mul_f64(double lhs, double rhs);
ZR_FFI_FIXTURE_EXPORT size_t zr_ffi_strlen_utf8(const char *text);
ZR_FFI_FIXTURE_EXPORT ZrFfiFixturePoint zr_ffi_make_point(int32_t x, int32_t y);
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_sum_point(ZrFfiFixturePoint point);
ZR_FFI_FIXTURE_EXPORT void zr_ffi_fill_point(ZrFfiFixturePoint *outPoint, int32_t x, int32_t y);
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_increment_i32(int32_t *value);
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_sum_varargs_i32(int32_t count, ...);
ZR_FFI_FIXTURE_EXPORT int32_t ZR_FFI_FIXTURE_STDCALL zr_ffi_stdcall_add_i32(int32_t lhs, int32_t rhs);
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_fill_bytes(uint8_t *buffer, size_t length, uint8_t seed);
ZR_FFI_FIXTURE_EXPORT double zr_ffi_apply_callback(double value, ZrFfiFixtureUnaryCallback callback);
ZR_FFI_FIXTURE_EXPORT double zr_ffi_apply_callback_foreign_thread(double value,
                                                                  ZrFfiFixtureUnaryCallback callback);
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_flip_mode(int32_t modeValue);
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_tell_fd(int32_t fd);

ZR_FFI_FIXTURE_EXPORT const char *zr_ffi_version_string(void) {
    return kZrFfiFixtureVersion;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_add_i32(int32_t lhs, int32_t rhs) {
    return lhs + rhs;
}

ZR_FFI_FIXTURE_EXPORT double zr_ffi_mul_f64(double lhs, double rhs) {
    return lhs * rhs;
}

ZR_FFI_FIXTURE_EXPORT size_t zr_ffi_strlen_utf8(const char *text) {
    return text != NULL ? strlen(text) : 0u;
}

ZR_FFI_FIXTURE_EXPORT ZrFfiFixturePoint zr_ffi_make_point(int32_t x, int32_t y) {
    ZrFfiFixturePoint point;
    point.x = x;
    point.y = y;
    return point;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_sum_point(ZrFfiFixturePoint point) {
    return point.x + point.y;
}

ZR_FFI_FIXTURE_EXPORT void zr_ffi_fill_point(ZrFfiFixturePoint *outPoint, int32_t x, int32_t y) {
    if (outPoint == NULL) {
        return;
    }
    outPoint->x = x;
    outPoint->y = y;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_increment_i32(int32_t *value) {
    if (value == NULL) {
        return -1;
    }
    *value += 1;
    return *value;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_sum_varargs_i32(int32_t count, ...) {
    va_list arguments;
    int32_t total = 0;
    int32_t index = 0;

    va_start(arguments, count);
    for (index = 0; index < count; index++) {
        total += va_arg(arguments, int32_t);
    }
    va_end(arguments);

    return total;
}

ZR_FFI_FIXTURE_EXPORT int32_t ZR_FFI_FIXTURE_STDCALL zr_ffi_stdcall_add_i32(int32_t lhs, int32_t rhs) {
    return lhs + rhs;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_fill_bytes(uint8_t *buffer, size_t length, uint8_t seed) {
    size_t index;

    if (buffer == NULL) {
        return -1;
    }

    for (index = 0; index < length; index++) {
        buffer[index] = (uint8_t)(seed + (uint8_t)index);
    }

    return (int32_t)length;
}

ZR_FFI_FIXTURE_EXPORT double zr_ffi_apply_callback(double value, ZrFfiFixtureUnaryCallback callback) {
    if (callback == NULL) {
        return -1.0;
    }
    return callback(value) + 0.5;
}

#if defined(_WIN32)
typedef struct ZrFfiFixtureThreadData {
    ZrFfiFixtureUnaryCallback callback;
    double value;
    double result;
} ZrFfiFixtureThreadData;

static DWORD WINAPI zr_ffi_fixture_thread_proc(LPVOID argument) {
    ZrFfiFixtureThreadData *data = (ZrFfiFixtureThreadData *)argument;
    if (data != NULL && data->callback != NULL) {
        data->result = data->callback(data->value);
    }
    return 0;
}
#else
typedef struct ZrFfiFixtureThreadData {
    ZrFfiFixtureUnaryCallback callback;
    double value;
    double result;
} ZrFfiFixtureThreadData;

static void *zr_ffi_fixture_thread_proc(void *argument) {
    ZrFfiFixtureThreadData *data = (ZrFfiFixtureThreadData *)argument;
    if (data != NULL && data->callback != NULL) {
        data->result = data->callback(data->value);
    }
    return NULL;
}
#endif

ZR_FFI_FIXTURE_EXPORT double zr_ffi_apply_callback_foreign_thread(double value, ZrFfiFixtureUnaryCallback callback) {
    ZrFfiFixtureThreadData data;

    data.callback = callback;
    data.value = value;
    data.result = -999.0;

#if defined(_WIN32)
    {
        HANDLE threadHandle = CreateThread(NULL, 0, zr_ffi_fixture_thread_proc, &data, 0, NULL);
        if (threadHandle == NULL) {
            return -998.0;
        }
        WaitForSingleObject(threadHandle, INFINITE);
        CloseHandle(threadHandle);
    }
#else
    {
        pthread_t threadId;
        if (pthread_create(&threadId, NULL, zr_ffi_fixture_thread_proc, &data) != 0) {
            return -998.0;
        }
        pthread_join(threadId, NULL);
    }
#endif

    return data.result;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_flip_mode(int32_t modeValue) {
    return modeValue == (int32_t)ZR_FFI_FIXTURE_MODE_ON
                   ? (int32_t)ZR_FFI_FIXTURE_MODE_OFF
                   : (int32_t)ZR_FFI_FIXTURE_MODE_ON;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_tell_fd(int32_t fd) {
#if defined(_WIN32)
    return (int32_t)_lseeki64(fd, 0, SEEK_CUR);
#else
    return (int32_t)lseek(fd, 0, SEEK_CUR);
#endif
}
