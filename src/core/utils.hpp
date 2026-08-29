#pragma once

#include "common.hpp"
#include <chrono>

class Timer
{
public:
  Timer() : _start(std::chrono::steady_clock::now())
  {
  }

  std::chrono::duration<double> check() const
  {
    return std::chrono::steady_clock::now() - _start;
  }

  void reset()
  {
    _start = std::chrono::steady_clock::now();
  }

private:
  std::chrono::time_point<std::chrono::steady_clock> _start;
};

std::string mem_string(size_t bytes);

std::string split_thousands(uint64_t value);

std::string duration_string(std::chrono::duration<double> d);