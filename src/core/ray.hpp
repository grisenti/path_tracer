#pragma once

#include "common.hpp"

class Ray
{
public:
  Ray(Point3f o, UVec3f d, Real t_min, Real t_max)
      : o(o), t_min(t_min), t_max(t_max), _d(d)
  {
    _inv_d = Vec3f(1._r / d->x, 1._r / d->y, 1._r / d->z);
  }

  Ray(Point3f o, UVec3f d) : Ray(o, d, 0._r, INFTY)
  {
  }

  Point3f at(Real const t) const
  {
    return o + _d * t;
  }

  UVec3f const &d() const
  {
    return _d;
  }

  Vec3f const &inv_d() const
  {
    return _inv_d;
  }

  Point3f o;

  float t_min;
  float t_max;

private:
  UVec3f _d;
  Vec3f _inv_d;
};

// from "Ray Tracing Gems" by E. Haines, T. Akenine-Möller
inline Point3f offset_intersection(Point3f const &p, UNormal3f const &n)
{
  // this function only works if Real is float
  static_assert(std::is_same_v<Real, float>);

  auto constexpr ORIGIN = 1._r / 32._r;
  // not sure if this is the same with double
  auto constexpr FLOAT_SCALE = 1._r / 65536._r;
  auto constexpr INT_SCALE = 256._r;

  Vec3i const of_i{
    as<int32_t>(INT_SCALE * n->x),
    as<int32_t>(INT_SCALE * n->y),
    as<int32_t>(INT_SCALE * n->z),
  };

  Vec3f const p_i{
    std::bit_cast<Real>(
        std::bit_cast<int32_t>(p.x) + (p.x < 0 ? -of_i.x : of_i.x)),
    std::bit_cast<Real>(
        std::bit_cast<int32_t>(p.y) + (p.y < 0 ? -of_i.y : of_i.y)),
    std::bit_cast<Real>(
        std::bit_cast<int32_t>(p.z) + (p.z < 0 ? -of_i.z : of_i.z)),
  };

  return Point3f{
    std::abs(p.x) < ORIGIN ? p.x + FLOAT_SCALE * n->x : p_i.x,
    std::abs(p.y) < ORIGIN ? p.y + FLOAT_SCALE * n->y : p_i.y,
    std::abs(p.z) < ORIGIN ? p.z + FLOAT_SCALE * n->z : p_i.z,
  };
}
