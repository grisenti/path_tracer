#pragma once

#include "common.hpp"
#include "shapes.hpp"
#include "emitters.hpp"
#include "bsdfs.hpp"
#include "aggregates/bvh.hpp"
#include "aggregates/bvh_8.hpp"

struct PrimitiveIntersection
{
  ShapeIntersection shape_intersection;
  Emitter emitter = Emitter::new_invalid_emitter();
  BSDF bsdf = BSDF::new_invalid_bsdf();
};

class BasicPrimitive;

class Primitive
{
public:
  static Primitive new_basic_primitive(BasicPrimitive primitive);
  static Primitive new_bvh_aggregate(Bvh bvh);
  static Primitive new_bvh8_aggregate(Bvh8 bvh);

  static Primitive new_invalid_primitive();

  std::optional<PrimitiveIntersection> intersect(Ray const &r) const;

  bool is_hit_by(Ray const &r) const;

  Bbox3f bounds() const;

private:
  static std::vector<BasicPrimitive> all_basic_primitives;
  static std::vector<Bvh> all_bvh_aggregates;
  static std::vector<Bvh8> all_bvh8_aggregates;

  enum class PrimitiveType : uint64_t
  {
    Basic,
    BVHAggregate,
    BVH8Aggregate,
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

  bool is_hit_by(Ray const &r) const
  {
    return _shape.is_hit_by(r);
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
  case PrimitiveType::BVHAggregate:
    return all_bvh_aggregates[_handle.index()].intersect(r);
  case PrimitiveType::BVH8Aggregate:
    return all_bvh8_aggregates[_handle.index()].intersect(r);
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
  case PrimitiveType::BVHAggregate:
    return all_bvh_aggregates[_handle.index()].bounds();
  case PrimitiveType::BVH8Aggregate:
    return all_bvh8_aggregates[_handle.index()].bounds();
  case PrimitiveType::End:
    PT_FATAL("called bounds with invalid primitive");
  default:
    PT_FATAL("bounds for this type of primitive has yet to be implemented");
  }
}

inline bool Primitive::is_hit_by(Ray const &r) const
{
  switch (_handle.type())
  {
  case PrimitiveType::Basic:
    return all_basic_primitives[_handle.index()].is_hit_by(r);
  case PrimitiveType::BVHAggregate:
    return all_bvh_aggregates[_handle.index()].is_hit_by(r);
  case PrimitiveType::BVH8Aggregate:
    return all_bvh8_aggregates[_handle.index()].is_hit_by(r);
  case PrimitiveType::End:
    PT_FATAL("called is_hit_by with invalid primitive");
  default:
    PT_FATAL("is_hit_by for this type of primitive has yet to be implemented");
  }
}
