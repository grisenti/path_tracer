#pragma once

#include <cassert>
#include <gmath/geometry.hpp>
#include <gmath/constants.hpp>

#include "gmath/vec.hpp"


#define PT_ASSERT(x) assert(x)

using namespace gmath;

struct Ray
{
  Ray(Point3f o, UVec3f d) : o(o), d(d), t_min(0.f), t_max(inf)
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

struct Extent2u
{
  uint32_t x;
  uint32_t y;
};

struct Extent2f
{
  float x;
  float y;
};


