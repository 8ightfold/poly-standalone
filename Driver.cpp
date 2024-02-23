#include <Poly/Poly.hpp>
#include <optional>
#include <cstdio>
#include <expected>

struct MyBase {
  virtual ~MyBase() {}
  virtual void saySomething() = 0;
};

struct Meower : public MyBase {
  void saySomething() override { std::printf("Meow!\n"); }
};

struct Woofer : public MyBase {
  void saySomething() override { std::printf("Woof!\n"); }
};

using MyPoly = efl::Poly<MyBase, Meower, Woofer>;

int main() {
  MyPoly x;
  assert(x.isEmpty());
  x = Meower();
  x->saySomething();
  x = Woofer();
  x->saySomething();
  assert(x.holdsType<Woofer>());
  
  MyPoly y = x;
  assert(y.holdsAny());
  Meower M {};
  y = M;
  assert(y.holdsType<Meower>());
  y->saySomething();

  MyPoly z = std::move(x);
  assert(z.holdsType<Woofer>() && x.isEmpty());
  z->saySomething();

  std::optional<MyPoly> O;
  assert(!O.has_value());
  O = Meower();
  (*O)->saySomething();

  x = O->take();
  x->saySomething();
  z.swap(x);
  x->saySomething();

  x = z.take();
  assert(z.isEmpty());
  assert(x.holdsType<Meower>());
  z = Woofer();
  std::swap(x, z);
  assert(x.holdsType<Woofer>());
}