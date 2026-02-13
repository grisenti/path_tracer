#pragma once

#include <memory>

#include "common.hpp"

#include <vector>
#include <optional>

struct Primitive;
class Integrator;

struct Intersection
{
  Point3f p;
  Real t;
  UNormal3f n;
};

struct Primitive
{
  Point3f center;
  Real radius;

  std::optional<Intersection> intersect(Ray const &r) const
  {
    auto const oc = r.o - center;
    auto const a = square_magnitude(r.d);
    auto const hb = dot(r.d, oc);
    auto const c = square_magnitude(oc) - pow2(radius);
    auto const delta = (pow2(hb) - a * c);
    if (delta < 0)
      return std::nullopt;

    auto const sqrt_delta = std::sqrt(delta);
    auto t = (-hb - sqrt_delta) / a;
    if (!in_range(t, r.t_min, r.t_max))
    {
      t = (-hb + sqrt_delta) / a;
      if (!in_range(t, r.t_min, r.t_max))
        return std::nullopt;
    }
    auto const p = r.at(t);
    auto const n
        = UNormal3f::normalize((p - center) / radius);

    return Intersection{ p, t, n };
  }
};

struct Scene
{
  std::vector<Primitive> primitives;
  std::unique_ptr<Integrator> integrator;

  std::optional<Intersection> intersect(Ray r) const
  {
    std::optional<Intersection> result;
    for (auto const &p : primitives)
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