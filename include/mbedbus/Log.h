/// @file Log.h
/// @brief Minimal compile-time-disableable logging for mbedbus.

#ifndef MBEDBUS_LOG_H
#define MBEDBUS_LOG_H

#include <cstdio>

#ifdef MBEDBUS_LOGGING_ENABLED
#define MBEDBUS_LOG(...) std::fprintf(stderr, "[mbedbus] " __VA_ARGS__), std::fprintf(stderr, "\n")
#else
#define MBEDBUS_LOG(...) ((void)0)
#endif

#endif // MBEDBUS_LOG_H
