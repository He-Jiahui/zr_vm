//
// Created by HeJiahui on 2025/6/20.
//

#ifndef ZR_LOG_CONF_H
#define ZR_LOG_CONF_H

enum EZrLogLevel {
    ZR_LOG_LEVEL_DEBUG,
    ZR_LOG_LEVEL_VERBOSE,
    ZR_LOG_LEVEL_INFO,
    ZR_LOG_LEVEL_WARNING,
    ZR_LOG_LEVEL_ERROR,
    ZR_LOG_LEVEL_EXCEPTION,
    ZR_LOG_LEVEL_FATAL,
};

typedef enum EZrLogLevel EZrLogLevel;
#endif //ZR_LOG_CONF_H
