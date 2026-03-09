#pragma once

#include "aggregates.hpp"
#include "common.hpp"
#include "shapes.hpp"

struct PrimitiveIntersection
{
  // should contain material properties and other stuff
  // in the future
  ShapeIntersection shape_intersection;
};

class BasicPrimitive;
class ListAggregate;

class Primitive
{
public:
  static std::vector<BasicPrimitive> all_basic_primitives;
  static Primitive new_basic_primitive(BasicPrimitive primitive);
  static std::vector<ListAggregate> all_list_aggregates;
  static Primitive new_list_aggregate(ListAggregate list);
  static std::vector<BVHAggregate> all_bvh_aggregates;
  static Primitive new_bvh_aggregate(BVHAggregate bvh);

  static Primitive new_invalid_primitive();

  std::optional<PrimitiveIntersection> intersect(Ray const &r) const;

  BoundingBox3f bounds() const;

private:
  enum class PrimitiveType : uint64_t
  {
    Basic,
    ListAggregate,
    BVHAggregate,
    End
  };

  Primitive(uint64_t index, PrimitiveType type) : _handle(index, type)
  {
  }

  Handle<PrimitiveType> _handle;
};

class BasicPrimitive
{
public:
  BasicPrimitive(Shape s) : _shape(s)
  {
  }

  std::optional<PrimitiveIntersection> intersect(Ray const &r) const
  {
    auto its = _shape.intersect(r);
    if (its)
      return PrimitiveIntersection{ *its };
    return {};
  }

  BoundingBox3f bounds() const
  {
    return _shape.bounds();
  }

private:
  Shape _shape;
};

inline std::optional<PrimitiveIntersection> Primitive::intersect(
    Ray const &r) const
{
  switch (_handle.type())
  {
  case PrimitiveType::Basic:
    return all_basic_primitives[_handle.index()].intersect(r);
  case PrimitiveType::ListAggregate:
    return all_list_aggregates[_handle.index()].intersect(r);
  case PrimitiveType::BVHAggregate:
    return all_bvh_aggregates[_handle.index()].intersect(r);
  case PrimitiveType::End:
    PT_FATAL("called intersect with invalid primitive");
  default:
    PT_FATAL("intersect for this type of primitive has yet to be implemented");
  }
}

inline BoundingBox3f Primitive::bounds() const
{
  switch (_handle.type())
  {
  case PrimitiveType::Basic:
    return all_basic_primitives[_handle.index()].bounds();
  case PrimitiveType::End:
    PT_FATAL("called intersect with invalid primitive");
  default:
    PT_FATAL("intersect for this type of primitive has yet to be implemented");
  }
}