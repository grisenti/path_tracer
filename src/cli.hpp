#pragma once

#include "common.hpp"

#include <thread>

struct ProgramOptions
{
  // '-t <n>' where <n> is the number of threads
  uint32_t max_threads = std::thread::hardware_concurrency();
  // '-s'
  std::string scene_file;
  // '-o <file>'
  std::optional<std::string> image_output = {};
  // '-i'
  bool interactive = false;
};

ProgramOptions parse_options(int argc, char **argv);