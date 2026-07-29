#pragma once

#include <string>

/// @file
/// @brief Placeholder module validating the standard src/<module> shape
/// (CLAUDE.md §1) before it is cloned for real modules.
namespace hello_core {

/// @brief Produces a canonical greeting string. Exists purely to exercise
/// the module template (headers/impl/tests/python bindings/CMake/CI),
/// not as a real algebraic component.
class Greeter {
public:
  /// @brief Construct a greeter that addresses @p name.
  /// @param name The name to greet.
  explicit Greeter(std::string name);

  /// @brief Build the greeting.
  /// @return A string of the form "Hello, <name>!".
  [[nodiscard]] std::string greet() const;

private:
  std::string name_;
};

} // namespace hello_core
