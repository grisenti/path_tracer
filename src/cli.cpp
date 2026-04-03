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
      "path_tracer -i <file> [-t <n>] [-o <file>]\n"
      "options:\n"
      "  -h: print this message\n"
      "  -t <n>: specify the maximum number of threads (<n>) the application "
      "will use\n"
      "  -i <file>: specify the scene file to render\n"
      "  -o <file>: specify file in which the output image will be stored\n"
      "  --help: same as -h\n";

std::optional<uint32_t> string_to_u32(std::string_view s)
{
  uint32_t value = 0;
  if (std::from_chars(s.data(), s.data() + s.size(), value).ec == std::errc{})
    return value;
  return std::nullopt;
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
  while (auto const next = options.next())
  {
    if (*next == "-h" || *next == "--help")
    {
      std::println(CLI_HELP);
      std::exit(EXIT_SUCCESS);
    }
    if (*next == "-t")
    {
      if (auto const n_opt = options.next())
      {
        if (auto const n = string_to_u32(*n_opt))
          result.max_threads = *n;
        else
          invalid_usage("invalid number after -t");
      }
      else
      {
        invalid_usage("-t requires a number");
      }
    }
    else if (*next == "-i")
    {
      if (auto const filename = options.next())
      {
        result.scene_file = *filename;
      }
      else
      {
        invalid_usage("-i requires a filename");
      }
    }
    else if (*next == "-o")
    {
      if (auto const filename = options.next())
        result.image_output = *filename;
      else
        invalid_usage("-o requires a filename");
    }
  }
  if (result.scene_file.empty())
    invalid_usage("missing scene file");
  return result;
}