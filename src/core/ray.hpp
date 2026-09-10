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
