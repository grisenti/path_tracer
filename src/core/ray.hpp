#pragma once

#include "common.hpp"

struct Ray
{
  Ray(Point3f o, UVec3f d, Real t_min, Real t_max)
      : o(o), d(d), t_min(t_min), t_max(t_max)
  {
  }

  Ray(Point3f o, UVec3f d) : Ray(o, d, 0._r, INFTY)
  {
  }

  Point3f at(Real const t) const
  {
    return o + d * t;
  }

  Point3f o;
  UVec3f d;

  float t_min;
  float t_max;
};
