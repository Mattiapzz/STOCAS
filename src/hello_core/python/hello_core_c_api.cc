#include "hello_core_c_api.hh"

#include <cstring>
#include <new>

#include <hello_core/greeter.hh>

namespace {

// Not exported: this is the exception boundary translation helper, an
// implementation detail of the C API (CLAUDE.md §11 - internal .cc-only
// functions don't require Doxygen).
template <typename Fn> HelloCoreStatus catch_all(Fn&& fn) {
  try {
    fn();
    return kHelloCoreOk;
  } catch (...) {
    return kHelloCoreErrorUnknown;
  }
}

} // namespace

struct HelloCoreGreeter {
  hello_core::Greeter impl;
};

extern "C" {

HelloCoreStatus hello_core_greeter_create(const char* name, HelloCoreGreeter** out_greeter) {
  if (name == nullptr || out_greeter == nullptr) {
    return kHelloCoreErrorNullArgument;
  }
  return catch_all([&] { *out_greeter = new HelloCoreGreeter{hello_core::Greeter(name)}; });
}

HelloCoreStatus hello_core_greeter_greet(const HelloCoreGreeter* greeter,
                                         char* buffer,
                                         unsigned buffer_size,
                                         unsigned* out_written) {
  if (greeter == nullptr || buffer == nullptr || out_written == nullptr) {
    return kHelloCoreErrorNullArgument;
  }
  return catch_all([&] {
    const std::string greeting = greeter->impl.greet();
    const unsigned to_copy = static_cast<unsigned>(greeting.size()) < buffer_size
                                 ? static_cast<unsigned>(greeting.size())
                                 : (buffer_size > 0 ? buffer_size - 1 : 0);
    if (buffer_size > 0) {
      std::memcpy(buffer, greeting.data(), to_copy);
      buffer[to_copy] = '\0';
    }
    *out_written = to_copy;
  });
}

void hello_core_greeter_free(HelloCoreGreeter* greeter) {
  delete greeter;
}
}
