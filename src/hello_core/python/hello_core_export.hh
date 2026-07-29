#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(HELLO_CORE_BUILDING_SHARED)
#define HELLO_CORE_EXPORT __declspec(dllexport)
#elif defined(HELLO_CORE_SHARED)
#define HELLO_CORE_EXPORT __declspec(dllimport)
#else
#define HELLO_CORE_EXPORT
#endif
#else
#define HELLO_CORE_EXPORT __attribute__((visibility("default")))
#endif
