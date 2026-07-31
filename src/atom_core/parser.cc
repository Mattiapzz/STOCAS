#include <array>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

#include <atom_core/parser.hh>
#include <numerica_core/big_int.hh>
#include <numerica_core/rational.hh>
#include <symbol_table/symbol.hh>

namespace atom_core {

namespace {

enum class TokKind { Number, Ident, Plus, Minus, Star, Slash, Caret, LParen, RParen, Comma, End };

struct Token {
  TokKind kind;
  std::string text; // Number digits or Ident name; empty otherwise
  std::size_t pos;
};

[[noreturn]] void fail(std::string_view message, std::size_t pos) {
  throw std::invalid_argument(std::string(message) + " at position " + std::to_string(pos));
}

std::vector<Token> tokenize(std::string_view src) {
  std::vector<Token> tokens;
  std::size_t i = 0;
  while (i < src.size()) {
    char c = src[i];
    if (std::isspace(static_cast<unsigned char>(c)) != 0) {
      ++i;
      continue;
    }
    std::size_t start = i;
    if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
      while (i < src.size() && std::isdigit(static_cast<unsigned char>(src[i])) != 0) {
        ++i;
      }
      tokens.push_back(Token{TokKind::Number, std::string(src.substr(start, i - start)), start});
      continue;
    }
    if (std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_') {
      while (i < src.size() &&
             (std::isalnum(static_cast<unsigned char>(src[i])) != 0 || src[i] == '_')) {
        ++i;
      }
      tokens.push_back(Token{TokKind::Ident, std::string(src.substr(start, i - start)), start});
      continue;
    }
    TokKind kind;
    switch (c) {
      case '+':
        kind = TokKind::Plus;
        break;
      case '-':
        kind = TokKind::Minus;
        break;
      case '*':
        kind = TokKind::Star;
        break;
      case '/':
        kind = TokKind::Slash;
        break;
      case '^':
        kind = TokKind::Caret;
        break;
      case '(':
        kind = TokKind::LParen;
        break;
      case ')':
        kind = TokKind::RParen;
        break;
      case ',':
        kind = TokKind::Comma;
        break;
      default:
        fail("unexpected character", start);
    }
    tokens.push_back(Token{kind, {}, start});
    ++i;
  }
  tokens.push_back(Token{TokKind::End, {}, src.size()});
  return tokens;
}

class Parser {
public:
  Parser(std::vector<Token> tokens, AtomStore& store, std::string_view namespace_name)
    : tokens_(std::move(tokens)), store_(store), namespace_name_(namespace_name) {}

  Atom parse_all() {
    Atom result = parse_expression(1);
    expect(TokKind::End, "trailing input after expression");
    return result;
  }

private:
  std::vector<Token> tokens_;
  AtomStore& store_;
  std::string_view namespace_name_;
  std::size_t pos_ = 0;

  [[nodiscard]] const Token& peek() const { return tokens_[pos_]; }
  Token advance() { return tokens_[pos_++]; }

  void expect(TokKind kind, std::string_view message) {
    if (peek().kind != kind) {
      fail(message, peek().pos);
    }
    advance();
  }

  [[nodiscard]] static bool starts_primary(TokKind kind) {
    return kind == TokKind::Number || kind == TokKind::Ident || kind == TokKind::LParen;
  }

  // Precedence: 1 = +/-, 2 = */÷/implicit-mult. `^` and unary `-` bind
  // tighter still and are handled inside parse_unary()/parse_pow(), which
  // acts as the "atom" the precedence-climbing loop below combines.
  Atom parse_expression(int min_prec) {
    Atom lhs = parse_unary();
    while (true) {
      TokKind kind = peek().kind;
      if ((kind == TokKind::Plus || kind == TokKind::Minus) && min_prec <= 1) {
        advance();
        Atom rhs = parse_expression(2);
        if (kind == TokKind::Minus) {
          rhs = negate(rhs);
        }
        std::array<Atom, 2> terms{lhs, rhs};
        lhs = store_.add(terms);
      } else if ((kind == TokKind::Star || kind == TokKind::Slash) && min_prec <= 2) {
        advance();
        Atom rhs = parse_expression(3);
        if (kind == TokKind::Slash) {
          rhs = store_.pow(rhs, store_.num(numerica_core::Rational(-1)));
        }
        std::array<Atom, 2> factors{lhs, rhs};
        lhs = store_.mul(factors);
      } else if (starts_primary(kind) && min_prec <= 2) {
        // Implicit multiplication: "2x", "x(y+1)", "x y" - same precedence
        // as explicit `*`.
        Atom rhs = parse_expression(3);
        std::array<Atom, 2> factors{lhs, rhs};
        lhs = store_.mul(factors);
      } else {
        break;
      }
    }
    return lhs;
  }

  Atom negate(Atom value) {
    std::array<Atom, 2> factors{store_.num(numerica_core::Rational(-1)), value};
    return store_.mul(factors);
  }

  Atom parse_unary() {
    if (peek().kind == TokKind::Minus) {
      // A '-' immediately followed by a numeric literal builds a raw
      // negative Num leaf directly, rather than negate()'s Mul(-1, N):
      // this is what the printer (M3-T3) needs to round-trip - a negative
      // Num leaf (e.g. the exponent `/` desugars to, store_.num(-1)) would
      // otherwise be unreconstructable, since re-parsing its printed form
      // "-1" would always go through this same negate() path and produce a
      // *different* tree shape (Mul, not a raw Num) than the original.
      if (tokens_[pos_ + 1].kind == TokKind::Number) {
        advance();
        Token number = advance();
        if (number.text.empty()) {
          fail("empty numeric literal", number.pos);
        }
        Atom base = store_.num(numerica_core::Rational(numerica_core::BigInt("-" + number.text)));
        // The negative-literal short-circuit above skips parse_pow(), so
        // '^' must be handled here too (same as parse_pow() below) -
        // otherwise "-19^2" would build just Num(-19) and leave "^2"
        // unconsumed instead of Pow(Num(-19), Num(2)), which is exactly
        // what the printer emits for that tree and must be able to
        // reparse.
        if (peek().kind == TokKind::Caret) {
          advance();
          Atom exponent = parse_unary();
          return store_.pow(base, exponent);
        }
        return base;
      }
      advance();
      return negate(parse_unary());
    }
    if (peek().kind == TokKind::Plus) {
      advance();
      return parse_unary();
    }
    return parse_pow();
  }

  Atom parse_pow() {
    Atom base = parse_primary();
    if (peek().kind == TokKind::Caret) {
      advance();
      Atom exponent = parse_unary(); // right-associative, allows x^-1
      return store_.pow(base, exponent);
    }
    return base;
  }

  Atom parse_primary() {
    const Token& tok = peek();
    switch (tok.kind) {
      case TokKind::Number: {
        advance();
        if (tok.text.empty()) {
          fail("empty numeric literal", tok.pos);
        }
        return store_.num(numerica_core::Rational(numerica_core::BigInt(tok.text)));
      }
      case TokKind::Ident: {
        advance();
        symbol_table::Symbol symbol = symbol_table::get_symbol(namespace_name_, tok.text);
        if (peek().kind == TokKind::LParen) {
          advance();
          std::vector<Atom> args;
          if (peek().kind != TokKind::RParen) {
            args.push_back(parse_expression(1));
            while (peek().kind == TokKind::Comma) {
              advance();
              args.push_back(parse_expression(1));
            }
          }
          expect(TokKind::RParen, "expected ')' to close function call");
          return store_.fun(symbol, args);
        }
        return store_.var(symbol);
      }
      case TokKind::LParen: {
        advance();
        Atom inner = parse_expression(1);
        expect(TokKind::RParen, "expected ')' to close parenthesized expression");
        return inner;
      }
      default:
        fail("expected a number, identifier, or '('", tok.pos);
    }
  }
};

} // namespace

Atom parse(std::string_view expression, AtomStore& store, std::string_view namespace_name) {
  std::vector<Token> tokens = tokenize(expression);
  if (tokens.size() == 1) { // just End
    fail("empty expression", 0);
  }
  Parser parser(std::move(tokens), store, namespace_name);
  return parser.parse_all();
}

} // namespace atom_core
