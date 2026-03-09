#include "utils.hpp"

#include <cmath>

std::string mem_string(size_t bytes)
{
  auto const exp = std::log2(bytes);
  if (exp < 10)
    return std::format("{} B", bytes);
  if (exp < 20)
    return std::format("{:.2f} KiB", static_cast<double>(bytes) / (1 << 10));
  if (exp < 30)
    return std::format("{:.2f} MiB", static_cast<double>(bytes) / (1 << 20));
  if (exp < 40)
    return std::format("{:.2f} GiB", static_cast<double>(bytes) / (1 << 30));
  // its probably never going to go past a pebibyte
  return std::format("{:.2f} TiB", static_cast<double>(bytes) / (1 << 40));
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