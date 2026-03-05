#pragma once

#include <memory>
#include <optional>

#include <toml++/impl/forward_declarations.hpp>

#include "common.hpp"
#include "primitives.hpp"

class Integrator;

class Properties
{
public:
  Properties(toml::table const *table) : _table(table)
  {
  }

  Point3f get_point(std::string_view prop_name) const;
  Point3f get_point(std::string_view prop_name, Point3f default_value) const;

  Real get_float(std::string_view prop_name) const;
  Real get_float(std::string_view prop_name, Real default_value) const;

private:
  toml::table const *_table;
};

struct Scene
{
  static Scene read_from_file(std::string_view path);

  Primitive top_level_primitive;
  std::unique_ptr<Integrator> integrator;

  std::optional<PrimitiveIntersection> intersect(Ray const &r) const
  {
    return top_level_primitive.intersect(r);
  }
};