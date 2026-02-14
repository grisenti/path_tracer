#pragma once

#include <optional>
#include <vector>

#include "common.hpp"

struct Intersection
{
  Point3f p;
  Real t;
  UNormal3f n;
};

class Sphere
{
public:
  Sphere(Point3f center, Real radius) : _center(center), _radius(radius)
  {
  }

  std::optional<Intersection> intersect(Ray r) const
  {
    auto const oc = r.o - _center;
    auto const a = square_magnitude(r.d);
    auto const hb = dot(r.d, oc);
    auto const c = square_magnitude(oc) - pow2(_radius);
    auto const delta = pow2(hb) - a * c;
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
        = UNormal3f::normalize((p - _center) / _radius);

    return Intersection{ p, t, n };
  }

private:
  Point3f _center;
  Real _radius;
};

enum class ShapeType : uint64_t
{
  Sphere,
  End,
};

struct ShapeHandle
{
  static constexpr uint64_t SHAPE_TYPE_MASK = 0xf000000000000000;
  static constexpr uint64_t SHAPE_TYPE_SHIFT = 56;

  ShapeHandle() : ShapeHandle(0, ShapeType::End)
  {
  }

  ShapeHandle(uint64_t index, ShapeType type)
  {
    PT_ASSERT((index & SHAPE_TYPE_MASK) == 0);
    handle = index | static_cast<uint64_t>(type) << SHAPE_TYPE_SHIFT;
  }

  uint64_t index() const
  {
    return handle & ~SHAPE_TYPE_MASK;
  }

  ShapeType type() const
  {
    return static_cast<ShapeType>(
      (handle & SHAPE_TYPE_MASK) >> SHAPE_TYPE_SHIFT);
  }

  bool is_valid() const
  {
    return type() != ShapeType::End;
  }

  uint64_t handle;
};

class Shape
{
public:
  static std::vector<Sphere> spheres;

  Shape(ShapeHandle handle) : _handle(handle)
  {
  }

  std::optional<Intersection> intersect(Ray r) const
  {
    PT_ASSERT(handle.is_valid());
    switch (_handle.type())
    {
    case ShapeType::Sphere:
      return spheres[_handle.index()].intersect(r);
    default:
      return std::nullopt;
    }
  }

private:
  ShapeHandle _handle;
};

