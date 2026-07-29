#include <utility>

#include <hello_core/greeter.hh>

namespace hello_core {

Greeter::Greeter(std::string name) : name_(std::move(name)) {}

std::string Greeter::greet() const {
  return "Hello, " + name_ + "!";
}

} // namespace hello_core
