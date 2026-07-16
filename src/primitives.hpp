#pragma once

#include "common.hpp"
#include "shapes.hpp"
#include "emitters.hpp"
#include "aggregates.hpp"
#include "bsdfs.hpp"

struct PrimitiveIntersection
{
  ShapeIntersection shape_intersection;
  Emitter emitter = Emitter::new_invalid_emitter();
  BSDF bsdf = BSDF::new_invalid_bsdf();
};

class BasicPrimitive;
class ListAggregate;

class Primitive
{
public:
  static Primitive new_basic_primitive(BasicPrimitive primitive);
  static Primitive new_list_aggregate(ListAggregate list);
  static Primitive new_bvh_aggregate(BVHAggregate bvh);

  static Primitive new_invalid_primitive();

  std::optional<PrimitiveIntersection> intersect(Ray const &r) const;

  Bbox3f bounds() const;

private:
  static std::vector<BasicPrimitive> all_basic_primitives;
  static std::vector<ListAggregate> all_list_aggregates;
  static std::vector<BVHAggregate> all_bvh_aggregates;

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
  BasicPrimitive(Shape shape, Emitter emitter, BSDF bsdf)
      : _shape(shape), _emitter(emitter), _bsdf(bsdf)
  {
  }

  std::optional<PrimitiveIntersection> intersect(Ray const &r) const
  {
    if (auto const its = _shape.intersect(r))
      return PrimitiveIntersection{ *its, _emitter, _bsdf };
    return {};
  }

  Bbox3f bounds() const
  {
    return _shape.bounds();
  }

private:
  Shape _shape;
  Emitter _emitter;
  BSDF _bsdf;
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

inline Bbox3f Primitive::bounds() const
{
  switch (_handle.type())
  {
  case PrimitiveType::Basic:
    return all_basic_primitives[_handle.index()].bounds();
  case PrimitiveType::End:
    PT_FATAL("called bounds with invalid primitive");
  default:
    PT_FATAL("bounds for this type of primitive has yet to be implemented");
  }
}
