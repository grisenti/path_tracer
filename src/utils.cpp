#include "utils.hpp"

#include <cmath>
#include <ranges>

std::string mem_string(size_t bytes)
{
  auto const exp = std::log2(bytes);
  if (exp < 10)
    return std::format("{} B", bytes);
  if (exp < 20)
    return std::format(
        "{:.2f} KiB", as<double>(bytes) / (1ull << 10));
  if (exp < 30)
    return std::format(
        "{:.2f} MiB", as<double>(bytes) / (1ull << 20));
  if (exp < 40)
    return std::format(
        "{:.2f} GiB", as<double>(bytes) / (1ull << 30));
  // its probably never going to go past a pebibyte
  return std::format("{:.2f} TiB", as<double>(bytes) / (1ull << 40));
}

std::string split_thousands(uint64_t value)
{
  std::string result;
  uint64_t n_digits = 1;
  while (value > 0)
  {
    auto const d = value % 10;
    result.push_back('0' + d);
    value /= 10;
    if (value != 0 && n_digits % 3 == 0)
      result.push_back('\'');
    ++n_digits;
  }
  std::ranges::reverse(result);
  return result;
}

std::string duration_string(std::chrono::duration<double> d)
{
  using namespace std::chrono_literals;
  namespace ch = std::chrono;
  if (d < 1us)
  {
    return std::format("{:.2f}ns",
        ch::duration_cast<ch::duration<double, std::nano>>(d).count());
  }
  if (d < 1ms)
  {
    return std::format("{:.2f}us",
        ch::duration_cast<ch::duration<double, std::micro>>(d).count());
  }
  if (d < 1s)
  {
    return std::format("{:.2f}ms",
        ch::duration_cast<ch::duration<double, std::milli>>(d).count());
  }
  if (d < 1min)
  {
    return std::format("{:.2f}s", d.count());
  }
  if (d < 1h)
  {
    return std::format("{:%M:%S}s", d);
  }
  return std::format("{:%H:%M:%S}s", d);
}