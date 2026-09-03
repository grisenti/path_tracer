#include "cli.hpp"

#include <span>
#include <charconv>

class Options
{
public:
  Options(int argc, char **argv)
      : _cli_options(argv, as<size_t>(argc)), _last_opt(1)
  {
    PT_ASSERT(argc >= 1);
  }

  std::optional<std::string_view> next()
  {
    if (_last_opt < _cli_options.size())
      return _cli_options[_last_opt++];
    return std::nullopt;
  }

private:
  std::span<char *> _cli_options;
  size_t _last_opt;
};

constexpr char const *CLI_HELP
    = "usage:\n"
      "path_tracer <scene_file> [options]\n"
      "options:\n"
      "  -t <n>: specify the maximum number of threads (n) the application "
      "will use (n should be at least one)\n"
      "  -i: display the rendering process in a separate window (uses up one "
      "of the available threads)\n"
      "  -o <file>: specify file in which the output image will be stored\n"
      "  --load-scene-only: only load and generate the acceleration "
      "structures for the given scene\n";

template <std::integral T>
std::optional<T> string_to_integer(std::string_view s)
{
  T value = 0;
  std::from_chars_result parse_result = {};
  if (s.starts_with("0x") || s.starts_with("0X"))
  {
    s.remove_prefix(2);
    parse_result = std::from_chars(s.data(), s.data() + s.size(), value, 16);
  }
  else
  {
    parse_result = std::from_chars(s.data(), s.data() + s.size(), value);
  }

  if (parse_result.ec != std::errc{}
      || parse_result.ptr != s.data() + s.size())
    return std::nullopt;
  return value;
}

void invalid_usage(std::string_view error)
{
  std::println("{}", error);
  std::println("for help use path_tracer -h");
  std::exit(EXIT_FAILURE);
}

ProgramOptions parse_options(int argc, char **argv)
{
  Options options{ argc, argv };
  ProgramOptions result;
  auto const first = options.next();
  if (!first)
    invalid_usage("missing scene file");
  if (*first == "-h" || *first == "--help")
  {
    std::println(CLI_HELP);
    std::exit(EXIT_SUCCESS);
  }
  result.scene_file = *first;
  while (auto const next = options.next())
  {
    if (*next == "-t")
    {
      if (auto const n_opt = options.next())
      {
        if (auto const n = string_to_integer<uint32_t>(*n_opt))
        {
          if (n == 0)
            invalid_usage("the number of threads should be > 0");
          result.max_threads = *n;
        }
        else
        {
          invalid_usage("invalid number after -t");
        }
      }
      else
      {
        invalid_usage("-t requires a number");
      }
    }
    else if (*next == "-o")
    {
      if (auto const filename = options.next())
        result.image_output = *filename;
      else
        invalid_usage("-o requires a filename");
    }
    else if (*next == "-i")
    {
      result.interactive = true;
    }
    else if (*next == "--load-scene-only")
    {
      result.load_scene_only = true;
    }
    else if (*next == "--debug-start")
    {
      if (auto const n_opt = options.next())
      {
        if (auto const n = string_to_integer<uint64_t>(*n_opt))
          result.debug_start = *n;
        else
          invalid_usage("invalid number after --debug-start");
      }
      else
      {
        invalid_usage("--debug-start requires a number");
      }
    }
    else
    {
      invalid_usage(std::format("{} is not a valid option", *next));
    }
  }
  if (!result.image_output && !result.interactive && !result.load_scene_only
      && !result.debug_start)
  {
    std::println(
        "WARNING: the rendered result is not displayed or saved anywhere");
  }
  if (result.load_scene_only && (result.interactive || result.image_output))
  {
    std::println(
        "WARNING: an output method was specified, but no rendering is done");
  }
  if (result.interactive && result.max_threads == 1)
  {
    invalid_usage(
        "with interactive mode active the program needs at least 2 threads");
  }
  if (result.debug_start && (result.interactive || result.image_output))
  {
    invalid_usage("WARNING: --debug-start does not produce an image");
  }
  return result;
}
