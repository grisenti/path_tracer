#pragma once

#include <source_location>
#include <format>
#include <print>

#include <gmath/geometry.hpp>
#include <gmath/constants.hpp>
#include <gmath/vec.hpp>

template <typename... UserArgs>
void write_error(std::source_location location, std::string_view error_message,
    UserArgs... user_args)
{
  std::string user_message;
  if constexpr (sizeof...(user_args) == 1)
  {
    user_message = std::format("{}", user_args...);
  }
  else if constexpr (sizeof...(user_args) > 1)
  {
    user_message = std::format(user_args...);
  }
  std::print("{}({}): {}\n{}", location.file_name(),
      location.line(), error_message, user_message);
}

#define PT_FATAL(...) do { write_error(std::source_location::current(), "FATAL ERROR", __VA_ARGS__); std::abort(); } while(0)

#define PT_ASSERT(x, ...) do { if (!(x)) {write_error(std::source_location::current(), "ASSERTION FAILED (" #x  ")", __VA_ARGS__); std::abort(); }} while(0)

using namespace gmath;

struct Ray
{
  Ray(Point3f o, UVec3f d) : o(o), d(d), t_min(0.f), t_max(inf)
  {
  }

  Point3f at(Real const t) const
  {
    return o + d * t;
  }

  Point3f o;
  UVec3f d;

  float t_min;
  float t_max;
};

struct Extent2u
{
  uint32_t x;
  uint32_t y;
};

struct Extent2f
{
  float x;
  float y;
};