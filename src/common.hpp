#pragma once

#include <source_location>
#include <format>
#include <print>

inline thread_local uint64_t global_current_sample_index
    = std::numeric_limits<uint64_t>::max();

template <typename... UserArgs>
void write_error(std::source_location location, std::string_view error_message,
    std::format_string<UserArgs...> fmt_string = "", UserArgs &&...user_args)
{
  std::string user_message
      = std::format(fmt_string, std::forward<UserArgs>(user_args)...);
  if (global_current_sample_index != std::numeric_limits<uint64_t>::max())
  {
    std::print("{}({}): {}\n{}\nsample index: {:#x}\n", location.file_name(),
        location.line(), error_message, user_message,
        global_current_sample_index);
  }
  else
  {
    std::print("{}({}): {}\n{}\n", location.file_name(), location.line(),
        error_message, user_message);
  }
}

#define PT_FATAL(...)                                                         \
  do                                                                          \
  {                                                                           \
    write_error(std::source_location::current(),                              \
        "FATAL ERROR" __VA_OPT__(, ) __VA_ARGS__);                            \
    std::abort();                                                             \
  } while (0)

#ifndef PT_DISABLE_ASSERT

#define PT_ASSERT(x, ...)                                                     \
  do                                                                          \
  {                                                                           \
    if (!(x))                                                                 \
    {                                                                         \
      write_error(std::source_location::current(),                            \
          "ASSERTION FAILED (" #x ")" __VA_OPT__(, ) __VA_ARGS__);            \
      std::abort();                                                           \
    }                                                                         \
  } while (0)

#define GMATH_DEBUG
#define GMATH_DEBUG_ASSERT_HANDLER(x) PT_ASSERT(x)

#else

#define PT_ASSERT(x, ...)

#endif

#include <gmath/gmath.hpp>

using namespace gmath;

constexpr Real EPSILON = 1e-4f;

template <std::floating_point T>
constexpr T exact_power_of_two(int const exp)
{
  T res = static_cast<T>(1.0);
  for (int i = 0; i < exp; ++i)
    res *= static_cast<T>(2.0);
  return res;
}

// Safe conversion routine between numeric types (as in it ensures that
// `val` can be represented by a value in `Out` without invoking UB). If
// both `In`and `Out` are integral types, it checks that val can be
// represented by `Out`.
// For conversion between floating-point types and between floating-point
// and integral types, we assume IEEE-754 floats. In this case, the
// conversion is not guaranteed to be precise.
template <Numeric Out, Numeric In>
Out as(In val)
{
  // check that floating point types are IEEE-754 and either 32 or 64 bits,
  // we don't support larger or smaller types and other standards.
  static_assert(std::is_integral_v<Out>
                || (std::numeric_limits<Out>::is_iec559
                    && (sizeof(Out) == 4 || sizeof(Out) == 8)));

  static_assert(std::is_integral_v<In>
                || (std::numeric_limits<In>::is_iec559
                    && (sizeof(In) == 4 || sizeof(In) == 8)));

  // for integral to floating-point conversions, the integral type should
  // be at most 64 bits
  static_assert(std::is_integral_v<Out> || sizeof(In) <= 8);
  static_assert(std::is_integral_v<In> || sizeof(Out) <= 8);

  if constexpr (std::is_integral_v<In> && std::is_integral_v<Out>)
  {
    // cases:
    // - Out is signed and In is unsigned and:
    //  - sizeof(In) < sizeof(Out): no checks, always safe
    //  - sizeof(In) > sizeof(Out): check val <= as<In>(Out::max) (since
    //  Out::max is always less than In::max as Out is signed)
    //  - sizeof(In) == sizeof(Out): same as above
    //
    // - Out is unsigned and In is signed and
    //  - sizeof(In) < sizeof(Out): check val >= 0 (In::max is smaller than
    //  Out::max)
    //  - sizeof(In) > sizeof(Out): check val >= 0 and val <= as<In>(Out::max)
    //  (since Out::max < In:max)
    //  - sizeof(In) == sizeof(Out): check val >= 0 (In::max is smaller than
    //  Out::max)
    //
    // - Out and In have the same signedness and:
    //  - sizeof(In) < sizeof(Out): always safe
    //  - sizeof(In) > sizeof(Out): val >= as<In>(Out::min) and val <=
    //  as<In>(Out::max)
    //  - sizeof(In) == sizeof(Out): same representation, no checks required
    //
    if constexpr (std::is_signed_v<Out> && std::is_unsigned_v<In>
                  && sizeof(In) >= sizeof(Out))
    {
      PT_ASSERT(val <= static_cast<In>(std::numeric_limits<Out>::max()));
    }
    else if constexpr (std::is_unsigned_v<Out> && std::is_signed_v<In>)
    {
      PT_ASSERT(val >= static_cast<In>(0));
      if constexpr (sizeof(In) > sizeof(Out))
        PT_ASSERT(val <= static_cast<In>(std::numeric_limits<Out>::max()));
    }
    else if constexpr (sizeof(In) > sizeof(Out))
    {
      PT_ASSERT(val >= static_cast<In>(std::numeric_limits<Out>::min())
                && val <= static_cast<In>(std::numeric_limits<Out>::max()));
    }
  }
  else if constexpr (std::is_floating_point_v<In>)
  {
    PT_ASSERT(!std::isnan(val));
    if constexpr (std::is_integral_v<Out>)
    {
      PT_ASSERT(!std::isinf(val));
      constexpr In upper_bound_ex
          = exact_power_of_two<In>(std::numeric_limits<Out>::digits);
      constexpr In lower_bound_inc
          = std::is_signed_v<Out> ? -upper_bound_ex : static_cast<In>(0);
      PT_ASSERT(val < upper_bound_ex);
      PT_ASSERT(val >= lower_bound_inc);
    }
  }
  return static_cast<Out>(val);
}