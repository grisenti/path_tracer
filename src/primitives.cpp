#include "primitives.hpp"

#include "aggregates.hpp"

std::vector<BasicPrimitive> Primitive::all_basic_primitives;
std::vector<ListAggregate> Primitive::all_list_aggregates;

Primitive Primitive::new_basic_primitive(BasicPrimitive primitive)
{
  auto const index = static_cast<uint64_t>(all_basic_primitives.size());
  all_basic_primitives.push_back(primitive);
  return Primitive(index, PrimitiveType::Basic);
}

Primitive Primitive::new_list_aggregate(ListAggregate list)
{
  auto const index = static_cast<uint64_t>(all_list_aggregates.size());
  all_list_aggregates.emplace_back(std::move(list));
  return Primitive(index, PrimitiveType::ListAggregate);
}

Primitive Primitive::new_invalid_primitive()
{
  return Primitive(0, PrimitiveType::End);
}