#pragma once

#include <memory>
#include <optional>

#include "common.hpp"
#include "primitives.hpp"

class Integrator;

struct Scene
{
  Primitive top_level_primitive;
  std::unique_ptr<Integrator> integrator;

  std::optional<PrimitiveIntersection> intersect(Ray const &r) const
  {
    return top_level_primitive.intersect(r);
  }
};