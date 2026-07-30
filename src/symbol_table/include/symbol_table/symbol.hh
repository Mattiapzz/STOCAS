#pragma once

#include <cstddef>
#include <source_location>
#include <span>
#include <string>
#include <string_view>

/// @file
/// @brief Process-wide, append-only symbol table: interned `Symbol` handles,
/// namespaced by module. Single-threaded in this milestone (M2-T3) - the
/// writer-lock/lock-free-read concurrency contract is added in M2-T4 and
/// must not be assumed yet.
namespace symbol_table {

/// @brief Properties a symbol can be registered with, mirroring the
/// attribute set of the original Symbolica design (used operationally by
/// pattern_core's matching rules in a later milestone; carried here so
/// registration conflicts can be detected now).
enum class SymbolAttribute { Symmetric, Antisymmetric, Linear };

/// @brief An interned handle to a registered symbol. Never default
/// constructed - only obtainable via get_symbol().
class Symbol {
public:
  Symbol() = delete;

  [[nodiscard]] bool operator==(const Symbol& other) const noexcept = default;

  /// @return The table-internal id backing this handle (stable for the
  ///         lifetime of the process, never reused).
  [[nodiscard]] std::size_t id() const noexcept { return id_; }

private:
  friend Symbol get_symbol(std::string_view namespace_name,
                           std::string_view name,
                           std::span<const SymbolAttribute> attributes,
                           std::source_location location);

  explicit Symbol(std::size_t id) noexcept : id_(id) {}

  std::size_t id_;
};

/// @brief Registers a symbol in the given namespace, or returns the handle
/// of an existing registration with the same namespace, name, and
/// attribute set (registration is idempotent when attributes match).
///
/// First definition wins: registering the same namespace+name again with a
/// *different* attribute set is a hard error, per the "first definition
/// wins, error shows original site" rule this mirrors from the original
/// design - the exception message includes the source location of the
/// original registration.
///
/// @param namespace_name The owning module's namespace (e.g. "atom_core").
///        See SYMBOL_TABLE_SYMBOL() for the CMake-injected-define
///        convenience form of passing this.
/// @param name The symbol's name within that namespace.
/// @param attributes Symbol properties; order does not matter for
///        conflict detection.
/// @throws std::logic_error if @p namespace_name + @p name was already
///         registered with a different attribute set.
[[nodiscard]] Symbol get_symbol(std::string_view namespace_name,
                                std::string_view name,
                                std::span<const SymbolAttribute> attributes = {},
                                std::source_location location = std::source_location::current());

/// @return The fully-qualified `"namespace::name"` string a symbol was
///         registered under.
[[nodiscard]] std::string symbol_name(Symbol symbol);

} // namespace symbol_table

#ifdef CORE_NAMESPACE
namespace symbol_table {
/// @brief The consuming module's namespace, injected at compile time via
/// `target_compile_definitions(<module> PRIVATE CORE_NAMESPACE="<module>")`
/// in that module's CMakeLists.txt - captured once into a constexpr rather
/// than re-expanding the macro at every call site.
inline constexpr std::string_view kCoreNamespace = CORE_NAMESPACE;
} // namespace symbol_table

/// @brief Registers/looks up @p name in the CORE_NAMESPACE injected into
/// this translation unit's target. Requires CORE_NAMESPACE to be defined
/// (see symbol_table::kCoreNamespace).
#define SYMBOL_TABLE_SYMBOL(name) ::symbol_table::get_symbol(::symbol_table::kCoreNamespace, name)
#endif
