#include <string>

#include <atom_core/printer.hh>
#include <symbol_table/symbol.hh>

namespace atom_core {

namespace {

// Binding power, loosest to tightest: Add < Mul < Pow < (Num/Var/Fun/parens).
// Mirrors parser.cc's precedence table exactly, so print()+parse() round-trip.
int precedence(AtomTag tag) {
  switch (tag) {
    case AtomTag::Add:
      return 1;
    case AtomTag::Mul:
      return 2;
    case AtomTag::Pow:
      return 3;
    case AtomTag::Num:
    case AtomTag::Var:
    case AtomTag::Fun:
      return 4;
  }
  return 4;
}

std::string local_name(symbol_table::Symbol symbol) {
  std::string qualified = symbol_table::symbol_name(symbol);
  std::size_t sep = qualified.rfind("::");
  return sep == std::string::npos ? qualified : qualified.substr(sep + 2);
}

std::string render(AtomView atom);

std::string wrap_if_needed(AtomView atom, int min_prec) {
  std::string text = render(atom);
  return precedence(atom.tag()) < min_prec ? "(" + text + ")" : text;
}

std::string render(AtomView atom) {
  switch (atom.tag()) {
    case AtomTag::Num:
      return atom.as_num().to_string();
    case AtomTag::Var:
      return local_name(atom.as_var());
    case AtomTag::Add: {
      std::string out;
      for (std::size_t i = 0; i < atom.child_count(); ++i) {
        if (i > 0) {
          out += " + ";
        }
        out += wrap_if_needed(atom.child(i), 2);
      }
      return out;
    }
    case AtomTag::Mul: {
      std::string out;
      for (std::size_t i = 0; i < atom.child_count(); ++i) {
        if (i > 0) {
          out += " * ";
        }
        out += wrap_if_needed(atom.child(i), 3);
      }
      return out;
    }
    case AtomTag::Pow:
      return wrap_if_needed(atom.child(0), 4) + "^" + wrap_if_needed(atom.child(1), 3);
    case AtomTag::Fun: {
      std::string out = local_name(atom.function_head()) + "(";
      for (std::size_t i = 0; i < atom.child_count(); ++i) {
        if (i > 0) {
          out += ", ";
        }
        out += wrap_if_needed(atom.child(i), 1);
      }
      out += ")";
      return out;
    }
  }
  return {};
}

} // namespace

std::string print(AtomView atom) {
  return render(atom);
}

} // namespace atom_core
