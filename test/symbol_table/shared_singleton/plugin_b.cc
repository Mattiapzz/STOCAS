#include <cstddef>

#include <symbol_table/symbol.hh>

// M2-T5: minimal exported surface for the shared-library singleton identity
// test. See test_singleton_identity.cc for what this proves.
extern "C" {

std::size_t plugin_b_lookup(const char* name) {
  return symbol_table::get_symbol("m2t5_shared_singleton", name).id();
}

} // extern "C"
