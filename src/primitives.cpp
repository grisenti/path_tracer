#include "primitives.hpp"

#include "aggregates/bvh.hpp"

std::vector<BasicPrimitive> Primitive::all_basic_primitives;
std::vector<BVH> Primitive::all_bvh_aggregates;

Primitive Primitive::new_basic_primitive(BasicPrimitive primitive)
{
  auto const index = as<uint64_t>(all_basic_primitives.size());
  all_basic_primitives.push_back(primitive);
  return Primitive(index, PrimitiveType::Basic);
}

Primitive Primitive::new_bvh_aggregate(BVH bvh)
{
  auto const index = as<uint64_t>(all_bvh_aggregates.size());
  all_bvh_aggregates.emplace_back(std::move(bvh));
  return Primitive(index, PrimitiveType::BVHAggregate);
}

Primitive Primitive::new_invalid_primitive()
{
  return Primitive(0, PrimitiveType::End);
}