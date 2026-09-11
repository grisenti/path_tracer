#include "bvh.hpp"

#include "bvh_build.hpp"

#include <span>
#include <atomic>
#include <memory>

#include "primitives.hpp"
#include "core/utils.hpp"
#include "concurrency.hpp"

Bvh::Bvh(std::vector<Primitive> primitives)
    : _primitives(std::move(primitives))
{
  auto ir = build_bvh(_primitives);
  pack_tree(ir.root.get());
  _internal_nodes = ir.internal_nodes;
  _leaf_nodes = ir.leaf_nodes;
}

std::optional<PrimitiveIntersection> Bvh::intersect(Ray ray) const
{
  std::optional<PrimitiveIntersection> intersection;

  auto node_stack = std::array<uint32_t, 64>{};
  uint32_t current_node = 0;
  size_t stack_top = 0;
  while (true)
  {
    auto const &node = _nodes[current_node];
    if (node.bounds.intersect(ray))
    {
      if (node.primitive_count() != 0)
      {
        auto primitives = std::span{ _primitives.begin() + node.fist_primitive,
          node.primitive_count() };
        for (auto const prim : primitives)
        {
          if (auto const its = prim.intersect(ray))
          {
            ray.t_max = its->shape_intersection.t;
            intersection = its;
          }
        }
        if (stack_top == 0)
          break;
        current_node = node_stack[--stack_top];
      }
      else
      {
        // if the direction in the split axis is negative, the
        // ray goes from right to left meaning that the ray intersects
        // the objects in the right node first
        if (ray.d()[node.split_axis()] < 0)
        {
          node_stack[stack_top++] = current_node + 1;
          current_node = node.right_child;
        }
        else
        {
          node_stack[stack_top++] = node.right_child;
          current_node = current_node + 1;
        }
      }
    }
    else
    {
      if (stack_top == 0)
        break;
      current_node = node_stack[--stack_top];
    }
  }
  return intersection;
}

Bbox3f Bvh::bounds() const
{
  PT_ASSERT(!_nodes.empty(), "trying to get bounds on an empty BVH");
  return _nodes[0].bounds;
}

void Bvh::print_statistics() const
{
  std::println("BVH statistics:");
  std::println("- primitives count: {}", split_thousands(_primitives.size()));
  std::println("- internal nodes: {}", split_thousands(_internal_nodes));
  std::println("- leaf nodes: {}", split_thousands(_leaf_nodes));
  std::println("- average primitive per leaf: {:.2f}",
      as<Real>(_primitives.size()) / as<Real>(_leaf_nodes));

  auto const node_memory = _nodes.size() * sizeof(BvhNode);
  auto const primitives_memory = _primitives.size() * sizeof(Primitive);
  std::println(
      "- memory use: {}", mem_string(node_memory + primitives_memory));
}

uint32_t Bvh::pack_tree(BvhBuildNode *node)
{
  auto const node_index = _nodes.size();
  _nodes.emplace_back(BvhNode{ .bounds = node->bounds,
      .fist_primitive = node->first_primitive,
      .metadata
      = BvhNode::pack_metadata(node->primitive_count, node->split_axis) });
  if (node->primitive_count == 0)
  {
    pack_tree(node->children[0].get());
    _nodes[node_index].right_child = pack_tree(node->children[1].get());
  }
  PT_ASSERT(node_index <= std::numeric_limits<uint32_t>::max());
  return as<uint32_t>(node_index);
}