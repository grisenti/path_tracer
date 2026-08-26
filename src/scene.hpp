#pragma once

#include "common.hpp"

#include <memory>
#include <optional>

#include "primitives.hpp"
#include "core/ray.hpp"

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