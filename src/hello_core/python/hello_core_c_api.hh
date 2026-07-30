#pragma once

#include "hello_core_export.hh"

/// @file
/// @brief Stable C ABI boundary for hello_core, per CLAUDE.md §9. No C++
/// types cross this boundary; every exported function catches all
/// exceptions internally and returns a status code.

extern "C" {

/// @brief Status codes returned by every hello_core_* C API call.
typedef enum HelloCoreStatus {
  kHelloCoreOk = 0,
  kHelloCoreErrorNullArgument = 1,
  kHelloCoreErrorUnknown = 2,
} HelloCoreStatus;

/// @brief Opaque handle to a hello_core::Greeter.
typedef struct HelloCoreGreeter HelloCoreGreeter;

/// @brief Create a Greeter for @p name.
/// @param name Null-terminated UTF-8 name string.
/// @param out_greeter Receives the owning handle on success. Caller must
///        release it with hello_core_greeter_free().
/// @return kHelloCoreOk on success, an error code otherwise.
HELLO_CORE_EXPORT HelloCoreStatus hello_core_greeter_create(const char* name,
                                                            HelloCoreGreeter** out_greeter);

/// @brief Render the greeting into a caller-owned buffer.
/// @param greeter A handle from hello_core_greeter_create().
/// @param buffer Destination buffer.
/// @param buffer_size Size of @p buffer in bytes.
/// @param out_written Receives the number of bytes written (excluding the
///        null terminator), truncated to fit @p buffer_size if necessary.
/// @return kHelloCoreOk on success, an error code otherwise.
HELLO_CORE_EXPORT HelloCoreStatus hello_core_greeter_greet(const HelloCoreGreeter* greeter,
                                                           char* buffer,
                                                           unsigned buffer_size,
                                                           unsigned* out_written);

/// @brief Release a handle obtained from hello_core_greeter_create().
/// @param greeter The handle to free. Passing nullptr is a no-op.
HELLO_CORE_EXPORT void hello_core_greeter_free(HelloCoreGreeter* greeter);
}
