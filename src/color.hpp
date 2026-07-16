#pragma once
#include "common.hpp"

template <typename T>
struct Color3 : VectorBase<3, T>
{
  using TypeClass = ColumnVectorTag;

  constexpr Color3() = default;

  constexpr Color3(T r, T g, T b) : r(r), g(g), b(b)
  {
  }

  constexpr Color3(T v) : r(v), g(v), b(v)
  {
  }

  GMATH_ARRAY_MEMBER_ACCESS_FOR_STANDARD_LAYOUT(Color3<T>, (&r), T)

  T r;
  T g;
  T b;
};

using Color3f = Color3<Real>;

template <Array A>
  requires(A::SIZE >= 3)
Color3<ComponentT<A>> as_color3(A const &a)
{
  return { a[0], a[1], a[2] };
}

inline Real luminance(Color3f const &c)
{
  return c.r * 0.212671f + c.g * 0.715160f + c.b * 0.072169f;
}

// from https://www.shadertoy.com/view/4dBcD1
inline Color3f jodie_reinhard(Color3f const &c)
{
  auto const l = luminance(c);
  auto const a = c / (l + 1.f);
  auto const b = componentwise_div(c, c + Color3f(1.f));
  Color3f result;
  for (size_t i = 0; i < 3; ++i)
    result[i] = std::lerp(a[i], b[i], b[i]);
  return result;
}

inline Color3f to_srgb(Color3f const &c)
{
  Color3f result = jodie_reinhard(c);

  // from https://en.wikipedia.org/wiki/SRGB
  for (size_t i = 0; i < 3; ++i)
  {
    auto const R = result[i];
    if (R <= 0.0031308_r)
      result[i] = 12.92_r * R;
    else
      result[i] = 1.055_r * std::pow(R, 1.0_r / 2.4_r) - 0.055_r;
  }

  return result;
}

inline uint32_t as_32bit_value(Color3f const &c, uint8_t alpha = 0xff)
{
  auto const r = as<uint32_t>(clamp(c.r * 255.f, 0.f, 255.f));
  auto const g = as<uint32_t>(clamp(c.g * 255.f, 0.f, 255.f));
  auto const b = as<uint32_t>(clamp(c.b * 255.f, 0.f, 255.f));
  return r | g << 8 | b << 16 | as<uint32_t>(alpha) << 24;
}

constexpr Color3f color_from_web_hex(uint32_t value)
{
  auto const r = as<Real>((value & 0xff0000) >> 16) / 255.f;
  auto const g = as<Real>((value & 0x00ff00) >> 8) / 255.f;
  auto const b = as<Real>(value & 0x0000ff) / 255.f;
  return { r, g, b };
}

template <typename T>
struct Color4 : VectorBase<4, T>
{
  using TypeClass = ColumnVectorTag;

  constexpr Color4() = default;

  constexpr Color4(T r, T g, T b, T a) : r(r), g(g), b(b), a(a)
  {
  }

  constexpr Color4(T v, T a) : r(v), g(v), b(v), a(a)
  {
  }

  constexpr Color4(Color3<T> c, T a) : r(c.r), g(c.g), b(c.b), a(a)
  {
  }

  GMATH_ARRAY_MEMBER_ACCESS_FOR_STANDARD_LAYOUT(Color4<T>, (&r), T)

  T r;
  T g;
  T b;
  T a;
};

using Color4f = Color4<Real>;