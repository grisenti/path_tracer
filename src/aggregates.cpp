#include "aggregates.hpp"

#include "primitives.hpp"

std::optional<PrimitiveIntersection> ListAggregate::intersect(Ray r) const
{
  std::optional<PrimitiveIntersection> result;
  for (auto const &p : _primitives)
  {
    if (auto its = p.intersect(r))
    {
      r.t_max = its->shape_intersection.t;
      result = its;
    }
  }
  return result;
}