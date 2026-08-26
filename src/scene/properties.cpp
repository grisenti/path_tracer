#include "properties.hpp"

#include "scene.hpp"
#include "scene/toml_helpers.hpp"

void Properties::specify_valid_properties(
    std::initializer_list<std::string_view> valid_properties) const
{
  for (auto const &prop : *_table)
  {
    // we always expect 'type', and it's not relevant to the user
    if (prop.first.str() == "type")
      continue;
    if (!std::ranges::contains(valid_properties, prop.first.str()))
    {
      // TODO: probably replace with proper logging
      std::println(
          "WARNING: table starting at line {} contains unused property '{}'",
          _table->source().begin.line, prop.first.str());
    }
  }
}

Point3f Properties::get_point3(std::string_view prop_name) const
{
  return get_float_array<Point3f>(*_table, prop_name);
}

Point3f Properties::get_point3(
    std::string_view prop_name, Point3f const &default_value) const
{
  return get_float_array_or_default<Point3f>(
      *_table, prop_name, default_value);
}

Vec3f Properties::get_vec3(std::string_view prop_name) const
{
  return get_float_array<Vec3f>(*_table, prop_name);
}

Vec3f Properties::get_vec3(
    std::string_view prop_name, Vec3f const &default_value) const
{
  return get_float_array_or_default<Vec3f>(*_table, prop_name, default_value);
}

Color3f Properties::get_color(std::string_view prop_name) const
{
  return get_float_array<Color3f>(*_table, prop_name);
}

Color3f Properties::get_color(
    std::string_view prop_name, Color3f const &default_value) const
{
  return get_float_array_or_default<Color3f>(
      *_table, prop_name, default_value);
}

Real Properties::get_float(std::string_view prop_name) const
{
  if (auto const node = _table->get(prop_name))
    return to_float(*node);
  throw SceneParseException("missing property '{}' at line {}", prop_name,
      _table->source().begin.line);
}

Real Properties::get_float(
    std::string_view prop_name, Real default_value) const
{
  if (auto const node = _table->get(prop_name))
    return to_float(*node);
  return default_value;
}

int64_t Properties::get_int(std::string_view prop_name) const
{
  return get_as<int64_t>(*_table, prop_name);
}

int64_t Properties::get_int(
    std::string_view prop_name, int64_t default_value) const
{
  if (auto const node = _table->get(prop_name))
  {
    if (!node->is_integer())
    {
      throw SceneParseException(
          "expected integer at line {}", prop_name, node->source().begin.line);
    }
    return node->as_integer()->get();
  }
  return default_value;
}

bool Properties::get_bool(std::string_view prop_name) const
{
  return get_as<bool>(*_table, prop_name);
}

bool Properties::get_bool(std::string_view prop_name, bool default_value) const
{
  if (auto const node = _table->get(prop_name))
  {
    if (!node->is_boolean())
    {
      throw SceneParseException(
          "expected bool at line {}", prop_name, node->source().begin.line);
    }
    return node->as_boolean()->get();
  }
  return default_value;
}