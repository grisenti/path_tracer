#pragma once

#include "common.hpp"

#include <toml++/toml.hpp>
#include <filesystem>

#include "scene.hpp"

// parses the toml file or throws SceneParseException
toml::table parse_toml_file(std::filesystem::path const &path);

template <typename T>
T const &get_as(toml::table const &table, std::string_view node_name)
{
  if (auto const val_ptr = table.get_as<T>(node_name))
  {
    if constexpr (std::derived_from<T, toml::node>)
      return *val_ptr;
    else
      return val_ptr->get();
  }
  throw SceneParseException(
      "attribute {} is missing or incorrect (table at line {})", node_name,
      table.source().begin.line);
}

template <typename T>
std::optional<T const *> get_as_opt(
    toml::table const &table, std::string_view node_name)
{
  auto const node_ptr = table.get(node_name);
  if (!node_ptr)
    return std::nullopt;
  if (auto const val_ptr = node_ptr->as<T>())
  {
    if constexpr (std::derived_from<T, toml::node>)
      return val_ptr;
    else
      return val_ptr->get();
  }
  throw SceneParseException(
      "attribute {} has incorrect type (table at line {})", node_name,
      table.source().begin.line);
}

// works with both double nodes and integer nodes to remove some of the
// friction when writing scene files manually
Real to_float(toml::node const &n);

std::string_view get_type(toml::table const &table);

template <ModifiableArray A>
  requires(std::is_same_v<ComponentT<A>, Real>)
A get_float_array(toml::table const &table, std::string_view attribute_name)
{
  auto const &list = get_as<toml::array>(table, attribute_name);
  if (list.size() != A::SIZE)
  {
    throw SceneParseException("expected {} component array at line {}",
        A::SIZE, list.source().begin.line);
  }
  A result;
  for (size_t i = 0; i < A::SIZE; ++i)
    result[i] = to_float(list[i]);
  return result;
}

template <ModifiableArray A>
  requires(std::is_same_v<ComponentT<A>, Real>)
A get_float_array_or_default(toml::table const &table,
    std::string_view attribute_name, A const &default_value)
{
  if (!table.contains(attribute_name))
    return default_value;
  return get_float_array<A>(table, attribute_name);
}

Transform3D read_view_transform(
    toml::table const &table, std::string_view attribute_name);

std::optional<Transform3D> read_object_transform_opt(
    toml::table const &table, std::string_view attribute_name);