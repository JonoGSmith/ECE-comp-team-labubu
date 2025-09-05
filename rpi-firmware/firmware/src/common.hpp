#pragma once
// Jonathan's common C++ utilities.
// --------------------------------

// Fundamental types
#include <cstdint>
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8  = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;
using f32 = float;
using f64 = double;

// Macro helpers
#define usingT template<typename... T> using
#define countof(x) (sizeof(x)/sizeof((x)[0]))
#define CONCATB(a, b) a ## b
#define CONCAT(a, b) CONCATB(a, b)
#define ANONYMOUS_VARIABLE CONCAT(CONCAT(ANON_, __COUNTER__), CONCAT(_LINE_, __LINE__))

#define PACKED [[gnu::packed]]

// Library essentials and shorthands
// ------------------------------
#include <string_view>
using sv = std::string_view;
using namespace std::string_view_literals;
#include <span>
template <typename T, std::size_t Extent = std::dynamic_extent> using span = std::span<T, Extent>;
#include <array>
template <typename T, auto N> using array = std::array<T, N>;
#include <optional>
usingT opt = std::optional<T...>;
#include <tuple>
usingT tup = std::tuple<T...>;
#include <variant>
usingT variant = std::variant<T...>;

// Those requiring a memory allocator
#include <string>
using string = std::string;
#include <vector>
usingT vec = std::vector<T...>;

// Deducing this (C++23)
#define self self                                // Intellisense highlighting
#define Self std::remove_cvref_t<decltype(self)> // Base type of current class (only usable in a self-deduced context)
#define SelfMut this auto& self                  // `self` declaration - reference (mutable ref)
#define SelfRef this auto const& self            // `self` declaration - const ref (immutable ref)
#define SelfFwd this auto&& self                 // `self` declaration - temporary (forwarding)

// Defer functionality:
template<typename F>
struct DeferHandle{
    F func;
    constexpr explicit DeferHandle(F&& function): func(std::move(function)) { } // Deletes copy/move and allows for creation
    constexpr ~DeferHandle() noexcept { func(); }
};
struct DeferBuilder{
    consteval auto operator->*(auto&& function){ return DeferHandle{std::move(function)}; }
};
#define defer_block DeferBuilder{} ->* [&]noexcept  // Syntax abuse to remove the user needing to write the capture notation
#define defer auto ANONYMOUS_VARIABLE = defer_block

// Utilities
#include <utility>

// Clamps in range (inclusive)
template<typename T>
constexpr T clamp(const T& low, const T& val, const T& high) {
    return std::min(std::max(val, low), high);
}

// Nicer reinterpret
template<class To> constexpr To ptr_cast(auto* p){ return reinterpret_cast<To>(p); }

template<class T> constexpr auto ptr_to_const(T const& target) -> T const* { return &target; }

template <typename CharT, CharT... Cs> consteval auto operator""_arr() {
    return std::array<CharT, sizeof...(Cs)>{ Cs... };
}
