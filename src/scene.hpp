#pragma once

#include <memory>
#include <vector>
#include <optional>

#include "common.hpp"
#include "shapes.hpp"

class Integrator;

struct Scene
{
  std::vector<Shape> shapes;
  std::unique_ptr<Integrator> integrator;

  std::optional<ShapeIntersection> intersect(Ray r) const
  {
    std::optional<ShapeIntersection> result;
    for (auto const &p : shapes)
    {
      auto its = p.intersect(r);
      if (its)
      {
        r.t_max = its->t;
        result = its;
      }
    }
    return result;
  }
};