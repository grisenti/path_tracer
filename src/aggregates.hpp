#pragma once

#include <vector>

#include "common.hpp"

struct PrimitiveIntersection;
class Primitive;

class ListAggregate
{
public:
  ListAggregate(std::vector<Primitive> primitives) : _primitives(
      std::move(primitives))
  {
  }

  std::optional<PrimitiveIntersection> intersect(Ray r) const;

private:
  std::vector<Primitive> _primitives;
};