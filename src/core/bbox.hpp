#pragma once

#include "common.hpp"
#include "ray.hpp"

template <typename T>
struct Bbox2
{
  Point2<T> min;
  Point2<T> max;

  Bbox2() : min(splat<Point2<T>>(INFTY)), max(splat<Point2<T>>(-INFTY))
  {
  }

  Bbox2(Point2<T> const &min, Point2<T> const &max) : min(min), max(max)
  {
  }
};

using Bbox2u = Bbox2<uint32_t>;

struct Bbox3f
{
  Point3f min;
  Point3f max;

  Bbox3f() : min(splat<Point3f>(INFTY)), max(splat<Point3f>(-INFTY))
  {
  }

  explicit Bbox3f(Point3f const &p) : min(p), max(p)
  {
  }

  Bbox3f(Point3f const &min, Point3f const &max) : min(min), max(max)
  {
  }

  template <typename... Ps>
  Bbox3f(Point3f const &p, Ps... ps) : Bbox3f(p)
  {
    (include(ps), ...);
  }

  Bbox3f(Bbox3f const &a, Bbox3f const &b)
  {
    min = componentwise_min(a.min, b.min);
    max = componentwise_max(a.max, b.max);
  }

  Bbox3f(Point3f const &a, Point3f const &b, Point3f const &c)
  {
    for (size_t i = 0; i < 3; ++i)
    {
      min[i] = std::min({ a[i], b[i], c[i] });
      max[i] = std::max({ a[i], b[i], c[i] });
    }
  }

  void include(Point3f const &p)
  {
    min = componentwise_min(min, p);
    max = componentwise_max(max, p);
  }

  void merge(Bbox3f const &bounds)
  {
    min = componentwise_min(min, bounds.min);
    max = componentwise_max(max, bounds.max);
  }

  // compute the position of p relative to the
  // corners, meaning that if p is
  // inside the bounding box, the result is
  // inside [0, 1]^n
  Point3f offset(const Point3f &p) const
  {
    Vec3f o = p - min;
    for (size_t i = 0; i < 3; ++i)
    {
      if (max[i] != min[i]) // prevent division by 0
        o[i] /= max[i] - min[i];
    }
    return as_point3(o);
  }

  bool intersect(Ray const &r) const
  {
    // slab method, from https://tavianator.com/2022/ray_box_boundary.html
    Real t_min = 0._r;
    Real t_max = INFTY;
    for (size_t i = 0; i < 3; ++i)
    {
      // TODO: maybe should precalculate 1/d
      auto const t1 = (min[i] - r.o[i]) / r.d[i];
      auto const t2 = (max[i] - r.o[i]) / r.d[i];

      t_min = std::min(std::max(t1, t_min), std::max(t2, t_min));
      t_max = std::max(std::min(t1, t_max), std::min(t2, t_max));
    }
    return t_min <= t_max;
  }

  Real surface_area() const
  {
    auto const diff = max - min;
    return 2 * dot(diff, Vec3f{ diff.y, diff.z, diff.x });
  }

  Point3f centroid() const
  {
    return min + (max - min) * 0.5;
  }

  bool operator==(const Bbox3f &) const = default;
};