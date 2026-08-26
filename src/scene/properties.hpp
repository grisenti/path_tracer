#pragma once

#include "common.hpp"

#include <toml++/toml.hpp>

#include "core/color.hpp"

class Properties
{
public:
  Properties(toml::table const *table) : _table(table)
  {
    PT_ASSERT(table);
  }

  // prints a warning if the dictionary contains properties different from the
  // ones in the list
  void specify_valid_properties(
      std::initializer_list<std::string_view> valid_properties) const;

  Point3f get_point3(std::string_view prop_name) const;
  Point3f get_point3(
      std::string_view prop_name, Point3f const &default_value) const;

  Vec3f get_vec3(std::string_view prop_name) const;
  Vec3f get_vec3(std::string_view prop_name, Vec3f const &default_value) const;

  Color3f get_color(std::string_view prop_name) const;
  Color3f get_color(
      std::string_view prop_name, Color3f const &default_value) const;

  Real get_float(std::string_view prop_name) const;
  Real get_float(std::string_view prop_name, Real default_value) const;

  int64_t get_int(std::string_view prop_name) const;
  int64_t get_int(std::string_view prop_name, int64_t default_value) const;

  bool get_bool(std::string_view prop_name) const;
  bool get_bool(std::string_view prop_name, bool default_value) const;

private:
  toml::table const *_table;
};
