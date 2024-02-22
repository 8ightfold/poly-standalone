//===- Poly.hpp -----------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
//     limitations under the License.
//
//===----------------------------------------------------------------===//
//
//  This file implements stack-allocated type-checked polymorphic
//  objects. It allows you to avoid dynamic allocation.
//
//===----------------------------------------------------------------===//

#pragma once

#ifndef STANDALONE_POLY_HPP
#define STANDALONE_POLY_HPP

#if __cpp_concepts < 201907L
# error C++20 is required!
#endif

#include <cstdlib>
#include <cstdint>
#include <concepts>
#include <new>
#include <utility>

#if defined(_MSC_VER) && !defined(__clang__)
# define ALWAYS_INLINE __forceinline
# define EMPTY_BASES __declspec(empty_bases)
# define HINT_INLINE __forceinline
#elif defined(__GNUC__)
# define ALWAYS_INLINE __attribute__(( \
  always_inline, artificial)) inline
# define EMPTY_BASES
# define HINT_INLINE inline
#else // _MSC_VER?
# define ALWAYS_INLINE inline
# define EMPTY_BASES
# define HINT_INLINE inline
#endif

#if defined(__clang__) && (__clang_major__ >= 13)
# define TAIL_INLINE [[gnu::noinline]]
# define TAIL_RETURN [[clang::musttail]] return
#else
# define TAIL_INLINE ALWAYS_INLINE
# define TAIL_RETURN return
#endif

#ifndef POLY_ASSERT
# include <cassert>
# define POLY_ASSERT(...) assert(__VA_ARGS__)
#endif

#ifndef POLY_FWD
# define POLY_FWD(...) static_cast< \
  decltype(__VA_ARGS__)&&>(__VA_ARGS__)
#endif

namespace efl::H {
  template <typename T, typename...TT>
  union RecursiveUnion {
    constexpr ~RecursiveUnion() {}
    T data_;
    RecursiveUnion<TT...> next_;
  };

  template <typename T>
  union RecursiveUnion<T> {
    constexpr ~RecursiveUnion() {}
    T data_;
  };

  template <typename...TT>
  struct AlignedStorage {
    using BaseType_ = RecursiveUnion<TT...>;
    using Type = std::uint8_t[sizeof(BaseType_)];
    alignas(BaseType_) Type data;
  };

  template <>
  struct AlignedStorage<> {
    using Type = std::uint8_t[1];
    std::uint8_t data[1];
  };

  template <typename Base>
  requires(std::is_abstract_v<Base>)
  struct AbstractBase {
    using RawType = std::uint8_t[sizeof(Base)];
  public:
    Base* operator&() noexcept {
      return reinterpret_cast<Base*>(this);
    }
    const Base* operator&() const noexcept {
      return reinterpret_cast<const Base*>(this);
    }
  public:
    alignas(Base) RawType storage_;
  };

  template <typename Base, typename...Derived>
  union PolyStorage {
    using BaseType_ = AlignedStorage<Derived...>;
    using RawType = typename BaseType_::Type;
    constexpr ~PolyStorage() {}
  public:
    alignas(BaseType_) RawType raw_;
    Base base_;
  };

  template <typename Base, typename...Derived>
  requires(std::is_abstract_v<Base>)
  union PolyStorage<Base, Derived...> {
    using BaseType_ = AlignedStorage<Derived...>;
    using RawType = typename BaseType_::Type;
    constexpr ~PolyStorage() {}
  public:
    alignas(BaseType_) RawType raw_;
    AbstractBase<Base> base_;
  };
} // namespace efl::H

namespace efl::H {
  template <typename T, typename...UU>
  concept matches_any = (false || ... || std::same_as<T, UU>);

  template <typename T>
  concept copyable = std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>;

  template <typename T>
  concept movable = std::is_move_constructible_v<T> && std::is_move_assignable_v<T>;

  template <typename...TT>
  concept all_copyable = (true && ... && copyable<TT>);

  template <typename...TT>
  concept all_movable = (true && ... && movable<TT>);

  template <typename T>
  struct TyNode {
    using Type = T;
  };

  template <std::size_t I>
  using IdNode = std::integral_constant<std::size_t, I>;

  template <typename T, std::size_t I>
  struct PolyNode {
    using Type = T;
    static constexpr auto value = I;
  public:
    ALWAYS_INLINE static constexpr
     IdNode<I + 1> IGetID(TyNode<T>) noexcept
    { return {}; }

    ALWAYS_INLINE static constexpr
     TyNode<T> IGetTy(IdNode<I + 1>) noexcept
    { return {}; }
  };

  template <typename, typename...>
  struct IPoly;

  template <std::size_t...II, typename...TT>
  struct EMPTY_BASES IPoly<
    std::index_sequence<II...>, TT...> 
   : PolyNode<TT, II>... {
  private:
    using PolyNode<TT, II>::IGetID...;
    using PolyNode<TT, II>::IGetTy...;
  public:
    template <typename U>
    ALWAYS_INLINE static constexpr
     auto GetID() noexcept {
      return IGetID(TyNode<U>{});
    }

    template <std::size_t I>
    ALWAYS_INLINE static constexpr
     auto GetTy() noexcept {
      return IGetTy(IdNode<I + 1>{});
    }
  };

  template <typename...TT>
  using IPolyBase = IPoly<
    std::make_index_sequence<sizeof...(TT)>, TT...>;
  
  template <typename To, typename From>
  constexpr To* launder_cast(From* from) noexcept {
    return std::launder(reinterpret_cast<To*>(from));
  }
} // namespace efl::H

namespace efl {
  template <typename Base,
    std::derived_from<Base>...Derived>
  struct Poly : protected H::IPolyBase<Base, Derived...> {
    using BaseType = H::IPolyBase<Base, Derived...>;
    using SelfType = Poly<Base, Derived...>;
    using StorageType = H::PolyStorage<Base, Derived...>;
  private:
    template <typename T>
    static constexpr std::size_t ID
      = BaseType::template GetID<T>().value;
  public:
    constexpr Poly() = default;

    Poly(const Poly& p)
      requires H::all_copyable<Derived...>
     : id_(p.id_) {
      p.visit([this] <typename T> (const T* P) {
        if constexpr (!std::is_abstract_v<T>)
          (void) new (this->data_.raw_) T{*P};
      });
    }

    Poly(Poly&& p) noexcept
      requires H::all_movable<Derived...>
     : id_(p.id_) {
      p.visit([this] <typename T> (T* P) {
        if constexpr (!std::is_abstract_v<T>)
          (void) new (this->data_.raw_) T{std::move(*P)};
      });
      p.destroySelf();
    }

    template <typename U>
    requires(H::matches_any<U, Base, Derived...> && H::copyable<U>)
    Poly(const U& u) : id_(ID<U>) {
      static_assert(!std::is_abstract_v<U>);
      (void) new (data_.raw_) U{u};
    }

    template <typename U>
    requires(H::matches_any<U, Base, Derived...> && H::movable<U>)
    Poly(U&& u) noexcept : id_(ID<U>) {
      static_assert(!std::is_abstract_v<U>);
      (void) new (data_.raw_) U{std::move(u)};
    }

    ~Poly() { destroySelf(); }

    template <typename = void>
    requires H::all_copyable<Derived...>
    Poly& operator=(const Poly& p) {
      destroySelf();
      this->id_ = p.id_;
      p.visit([this] <typename T> (const T* P) {
        (void) new (this->data_.raw_) T{*P};
      });
      return *this;
    }

    template <typename = void>
    requires H::all_movable<Derived...>
    Poly& operator=(Poly&& p) noexcept {
      destroySelf();
      this->id_ = p.id_;
      p.visit([this] <typename T> (T* P) {
        (void) new (this->data_.raw_) T{std::move(*P)};
      });
      p.destroySelf();
      return *this;
    }

    template <H::copyable U>
    requires H::matches_any<U, Base, Derived...>
    Poly& operator=(const U& u) {
      destroySelf();
      this->id_ = ID<U>;
      (void) new (data_.raw_) U{u};
      return *this;
    }

    template <H::movable U>
    requires H::matches_any<U, Base, Derived...>
    Poly& operator=(U&& u) noexcept {
      destroySelf();
      this->id_ = ID<U>;
      (void) new (data_.raw_) U{std::move(u)};
      return *this;
    }

    //=== Mutators ===//

    ALWAYS_INLINE void visit(auto&& F) {
      if (this->isEmpty()) {
        return;
      } else {
        TAIL_RETURN visit_<Base, Derived...>(
          POLY_FWD(F));
      }
    }

    ALWAYS_INLINE void visit(auto&& F) const {
      if (this->isEmpty()) {
        return;
      } else {
        TAIL_RETURN visit_<Base, Derived...>(
          POLY_FWD(F));
      }
    }

    constexpr Base* operator->() {
      POLY_ASSERT(holdsAny());
      return getPtr();
    }

    constexpr const Base* operator->() const {
      POLY_ASSERT(holdsAny());
      return getPtr();
    }

    void erase() noexcept {
      destroySelf();
    }

    //=== Observers ===//

    template <typename T>
    requires H::matches_any<T, Base, Derived...>
    constexpr bool holdsType() const noexcept {
      return this->id_ == ID<T>;
    }

    constexpr bool holdsAny() const noexcept {
      return this->id_ != 0U;
    }

    constexpr bool isEmpty() const noexcept {
      return this->id_ == 0U;
    }
  
  protected:
    void destroySelf() noexcept {
      this->visit([] <typename T>
        (T* P) { P->~T(); });
      this->id_ = 0U;
    }
  
  private:
    static constexpr std::size_t Size() noexcept {
      return sizeof...(Derived) + 1;
    }

    constexpr const Base* getPtr() const noexcept {
      if (id_ == 0) [[unlikely]] {
        return nullptr;
      } else [[likely]] {
        return &data_.base_;
      }
    }

    constexpr Base* getPtr() noexcept {
      if (id_ == 0) [[unlikely]] {
        return nullptr;
      } else [[likely]] {
        return &data_.base_;
      }
    }

    template <typename...Next>
    requires(sizeof...(Next) == 0)
    ALWAYS_INLINE void visit_(auto&& F) const noexcept {
      (void) F;
      return;
    }

    template <typename T, typename...Next>
    TAIL_INLINE void visit_(auto&& F) {
      if (ID<T> != id_) {
        TAIL_RETURN visit_<Next...>(POLY_FWD(F));
      } else {
        (void) POLY_FWD(F)(
          H::launder_cast<T>(getPtr()));
      }
    }

    template <typename T, typename...Next>
    TAIL_INLINE void visit_(auto&& F) const {
      if (ID<T> != id_) {
        TAIL_RETURN visit_<Next...>(POLY_FWD(F));
      } else {
        (void) POLY_FWD(F)(
          H::launder_cast<const T>(getPtr()));
      }
    }

  private:
    StorageType data_ {};
    std::size_t id_ = 0;
  };
} // namespace efl

#undef ALWAYS_INLINE
#undef EMPTY_BASES
#undef HINT_INLINE
#undef POLY_ASSERT
#undef POLY_FWD
#undef TAIL_INLINE
#undef TAIL_RETURN

#endif // STANDALONE_POLY_HPP