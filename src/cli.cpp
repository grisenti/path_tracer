#include "cli.hpp"

#include <span>
#include <charconv>
#include <map>
#include <expected>

namespace
{

enum class NumberError
{
  MissingParameter,
  InvalidNumber,
};

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

  std::expected<uint64_t, NumberError> next_unsigned();

private:
  std::span<char *> _cli_options;
  size_t _last_opt;
};

class OptionsParser
{
public:
  OptionsParser();

  ProgramOptions parse(Options &options);

private:
  static void handle_t(ProgramOptions &result, Options &options);
  static void handle_o(ProgramOptions &result, Options &options);
  static void handle_i(ProgramOptions &result, Options &options);
  static void handle_spp_override(ProgramOptions &result, Options &options);
  static void handle_tlas_override(ProgramOptions &result, Options &options);
  static void handle_load_scene_only(ProgramOptions &result, Options &options);
  static void handle_debug_start(ProgramOptions &result, Options &options);

  void handle_option(
      std::string_view opt, ProgramOptions &result, Options &options) const;
  static void validate_result(ProgramOptions const &result);
  static uint64_t next_unsigned(
      Options &options, std::string_view command_name);

  using OptionHandler = void (*)(ProgramOptions &, Options &options);
  std::map<std::string_view, OptionHandler> _handlers;
  std::string _help_string;
};

}

ProgramOptions parse_options(int argc, char **argv)
{
  Options options{ argc, argv };
  OptionsParser parser;
  return parser.parse(options);
}

namespace
{

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

[[noreturn]]
void invalid_usage(std::string_view error)
{
  std::println("{}", error);
  std::println("for help use path_tracer -h");
  std::exit(EXIT_FAILURE);
}

std::expected<uint64_t, NumberError> Options::next_unsigned()
{
  if (auto const n_opt = next())
  {
    if (auto const n = string_to_integer<uint64_t>(*n_opt))
      return *n;
    return std::unexpected(NumberError::InvalidNumber);
  }
  return std::unexpected(NumberError::MissingParameter);
}

OptionsParser::OptionsParser()
{
  _help_string = "usage:\n"
                 "path_tracer <scene_file> [options]\n"
                 "options:\n";

  _handlers["-t"] = &OptionsParser::handle_t;
  _help_string += "  -t <n>: specify the maximum number of threads (n) the "
                  "application will use (n should be at least one)\n";

  _handlers["-o"] = &OptionsParser::handle_o;
  _help_string += "  -o <file>: specify file in which the output image will "
                  "be stored\n";

  _handlers["-i"] = &OptionsParser::handle_i;
  _help_string += "  -i: display the rendering process in a separate window "
                  "(uses up one of the available threads)\n";

  _handlers["--spp"] = &OptionsParser::handle_spp_override;
  _help_string += "  --spp <n>: override spp value for the given scene\n";

  _handlers["--tlas"] = &OptionsParser::handle_tlas_override;
  _help_string
      += "  --tlas <type>: override tlas parameter for the given scene\n";

  _handlers["--load-scene-only"] = &OptionsParser::handle_load_scene_only;
  _help_string
      += "  --load-scene-only: only load and generate the acceleration "
         "structures for the given scene\n";

  _handlers["--debug-start"] = &OptionsParser::handle_debug_start;
  _help_string
      += "  --debug-start <sample index>: render only the specified sample "
         "(usually given by an error message)\n";
}

ProgramOptions OptionsParser::parse(Options &options)
{
  ProgramOptions result;
  auto const first = options.next();
  if (!first)
    invalid_usage("missing scene file");
  if (*first == "-h" || *first == "--help")
  {
    std::println("{}", _help_string);
    std::exit(EXIT_SUCCESS);
  }
  result.scene_file = *first;
  while (auto const next = options.next())
    handle_option(*next, result, options);
  validate_result(result);
  return result;
}

void OptionsParser::handle_t(ProgramOptions &result, Options &options)
{
  auto const n = next_unsigned(options, "-t");
  if (n == 0)
    invalid_usage("the number of threads should be > 0");
  if (n > std::numeric_limits<uint32_t>::max())
    invalid_usage("the number of threads is to large");
  result.max_threads = as<uint32_t>(n);
}

void OptionsParser::handle_o(ProgramOptions &result, Options &options)
{
  if (auto const filename = options.next())
    result.image_output = *filename;
  else
    invalid_usage("-o requires a filename");
}

void OptionsParser::handle_i(ProgramOptions &result, Options &)
{
  result.interactive = true;
}

void OptionsParser::handle_spp_override(
    ProgramOptions &result, Options &options)
{
  auto const spp = next_unsigned(options, "--spp");
  if (spp == 0)
  {
    invalid_usage("spp value cannot be 0. To parse the scene without "
                  "rendering use --load-scene-only");
  }
  if (spp > std::numeric_limits<uint32_t>::max())
    invalid_usage("spp value too large");
  result.scene_overrides.spp = as<uint32_t>(spp);
}

void OptionsParser::handle_tlas_override(
    ProgramOptions &result, Options &options)
{
  auto const tlas_opt = options.next();
  if (!tlas_opt)
    invalid_usage("missing value for --tlas option");
  if (!std::ranges::contains(Scene::tlas_types(), tlas_opt))
    invalid_usage(std::format("'{}' is not a valid tlas type", *tlas_opt));
  result.scene_overrides.tlas = *tlas_opt;
}

void OptionsParser::handle_load_scene_only(ProgramOptions &result, Options &)
{
  result.load_scene_only = true;
}

void OptionsParser::handle_debug_start(
    ProgramOptions &result, Options &options)
{
  result.debug_start = next_unsigned(options, "--debug-start");
}

void OptionsParser::handle_option(
    std::string_view opt, ProgramOptions &result, Options &options) const
{
  auto const it = _handlers.find(opt);
  if (it == _handlers.end())
    invalid_usage(std::format("{} is not a valid option", opt));
  auto const handler = it->second;
  (*handler)(result, options);
}

void OptionsParser::validate_result(ProgramOptions const &result)
{
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
}

uint64_t OptionsParser::next_unsigned(
    Options &options, std::string_view command_name)
{
  if (auto const n = options.next_unsigned())
    return *n;
  else if (n.error() == NumberError::InvalidNumber)
    invalid_usage(std::format("invalid number after {}", command_name));
  else if (n.error() == NumberError::MissingParameter)
    invalid_usage(std::format("{} requires a number", command_name));
  PT_FATAL();
}

}
