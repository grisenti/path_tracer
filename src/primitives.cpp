#include "primitives.hpp"

#include "aggregates/bvh.hpp"

std::vector<BasicPrimitive> Primitive::all_basic_primitives;
std::vector<Bvh> Primitive::all_bvh_aggregates;
std::vector<Bvh8> Primitive::all_bvh8_aggregates;

Primitive Primitive::new_basic_primitive(BasicPrimitive primitive)
{
  auto const index = as<uint64_t>(all_basic_primitives.size());
  all_basic_primitives.push_back(primitive);
  return Primitive(index, PrimitiveType::Basic);
}

Primitive Primitive::new_bvh_aggregate(Bvh bvh)
{
  auto const index = as<uint64_t>(all_bvh_aggregates.size());
  all_bvh_aggregates.emplace_back(std::move(bvh));
  return Primitive(index, PrimitiveType::BVHAggregate);
}

Primitive Primitive::new_bvh8_aggregate(Bvh8 bvh)
{
  auto const index = as<uint64_t>(all_bvh8_aggregates.size());
  all_bvh8_aggregates.emplace_back(std::move(bvh));
  return Primitive(index, PrimitiveType::BVH8Aggregate);
}

Primitive Primitive::new_invalid_primitive()
{
  return Primitive(0, PrimitiveType::End);
}