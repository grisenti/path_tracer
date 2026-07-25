#pragma once

#include "common.hpp"

#include <thread>

struct ProgramOptions
{
  std::string scene_file;
  // -t <n threads>
  uint32_t max_threads = std::thread::hardware_concurrency();
  // -o <file>
  std::optional<std::string> image_output = {};
  // -i
  bool interactive = false;
  // --load-scene-only
  bool load_scene_only = false;
  // --debug-start <sample index>
  std::optional<uint64_t> debug_start;
};

ProgramOptions parse_options(int argc, char **argv);