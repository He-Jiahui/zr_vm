#pragma once

#if defined(_WIN32) && defined(MINIZ_SHARED)
#if defined(MINIZ_EXPORTS)
#define MINIZ_EXPORT __declspec(dllexport)
#else
#define MINIZ_EXPORT __declspec(dllimport)
#endif
#else
#define MINIZ_EXPORT
#endif

