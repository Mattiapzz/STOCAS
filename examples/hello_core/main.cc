#include <iostream>

#include <hello_core/greeter.hh>

int main() {
  const hello_core::Greeter greeter("StoCAS");
  std::cout << greeter.greet() << '\n';
  return 0;
}
