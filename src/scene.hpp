#pragma once

#include <memory>
#include <optional>

#include <toml++/impl/forward_declarations.hpp>

#include "common.hpp"
#include "primitives.hpp"
#include "color.hpp"

class Integrator;
class Camera;
class Sampler;

class SceneParseException : public std::runtime_error
{
public:
  template <typename... Args>
  SceneParseException(std::format_string<Args...> fmt_string, Args &&...args)
      : std::runtime_error(
            std::format(fmt_string, std::forward<Args>(args)...))
  {
  }
};

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

struct Scene
{
  static Scene read_from_file(std::string_view path);

  Primitive top_level_primitive = Primitive::new_invalid_primitive();
  std::unique_ptr<Integrator> integrator;
  std::unique_ptr<Camera> camera;
  std::unique_ptr<Sampler> sampler;
  std::vector<Emitter> lights;
  uint32_t image_width;
  uint32_t image_height;
  uint32_t spp;

  std::optional<PrimitiveIntersection> intersect(Ray const &r) const
  {
    return top_level_primitive.intersect(r);
  }
};