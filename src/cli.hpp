#pragma once

#include "common.hpp"

#include <thread>

struct ProgramOptions
{
  // '-t <n>' where <n> is the number of threads
  uint32_t max_threads = std::thread::hardware_concurrency();
  // '-i'
  bool interactive = false;
  // '-o <file>'
  std::optional<std::string> image_output = {};
};

ProgramOptions parse_options(int argc, char **argv);