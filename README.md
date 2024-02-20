# poly-standalone

Standalone port of ``efl::Poly`` (yw nikki).

Moving a ``Poly`` instance erases the original.

```cpp
template <typename Base, typename...Derived>
struct Poly : /* Implementation */ {
  constexpr Poly() = default;
  Poly(const Poly& p);
  Poly(Poly&& p) noexcept;

  template </* copyable */ U> 
  Poly(const U& u);

  template </* movable */ U> 
  Poly(U&& u) noexcept;

  Poly& operator=(const Poly& p);
  Poly& operator=(Poly&& p) noexcept;

  template </* copyable */ U>
  Poly& operator=(const U& u);

  template </* movable */ U>
  Poly& operator=(U&& u) noexcept;

  //=== Mutators ===//

  void visit(auto&& F);
  void visit(auto&& F) const;
  constexpr Base* operator->();
  constexpr const Base* operator->() const;
  void erase() noexcept;

  //=== Observers ===//

  template <typename T>
  constexpr bool holdsType() const noexcept;
  constexpr bool holdsAny() const noexcept;
  constexpr bool isEmpty() const noexcept;
};
```
