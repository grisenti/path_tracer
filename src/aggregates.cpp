#include "aggregates.hpp"

#include <span>
#include <atomic>

#include "primitives.hpp"
#include "core/utils.hpp"
#include "concurrency.hpp"

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

struct BVHBuildNode
{
  Bbox3f bounds;
  std::unique_ptr<BVHBuildNode> children[2];
  uint32_t split_axis;
  uint32_t first_primitive;
  uint32_t primitive_count;
};

struct BVHBuilder
{
  std::span<Primitive> primitives = {};
  std::vector<BVHNode> nodes = {};
  std::atomic<size_t> leaf_nodes = 0;
  std::atomic<size_t> internal_nodes = 0;

  void build()
  {
    auto const root = build_subtree(primitives);
    pack_tree(root.get());
  }

  std::unique_ptr<BVHBuildNode> build_subtree(
      std::span<Primitive> primitives_in_bounds);

  std::unique_ptr<BVHBuildNode> create_leaf(
      std::span<Primitive> leaf_primitives);

  uint32_t pack_tree(BVHBuildNode *tree);
};

std::unique_ptr<BVHBuildNode> BVHBuilder::create_leaf(
    std::span<Primitive> leaf_primitives)
{
  ++leaf_nodes;
  Bbox3f bounds{};
  for (auto const prim : leaf_primitives)
  {
    bounds = Bbox3f(bounds, prim.bounds());
  }
  auto const start = leaf_primitives.data() - primitives.data();
  return std::make_unique<BVHBuildNode>(BVHBuildNode{
      .bounds = bounds,
      .children = { nullptr, nullptr },
      .split_axis = 0,
      .first_primitive = (uint32_t)start,
      .primitive_count = (uint32_t)leaf_primitives.size(),
  });
}

std::unique_ptr<BVHBuildNode> BVHBuilder::build_subtree(
    std::span<Primitive> primitives_in_bounds)
{
  if (primitives_in_bounds.size() <= 5)
  {
    return create_leaf(primitives_in_bounds);
  }

  Bbox3f bounds{};
  Point3f cluster_centroid;
  Bbox3f centroid_bounds;
  // pick a build split_axis
  auto min = splat<Point3f>(INFTY);
  auto max = splat<Point3f>(-INFTY);

  for (auto const prim : primitives_in_bounds)
  {
    auto const prim_bounds = prim.bounds();
    bounds.merge(prim_bounds);
    auto const centroid = prim_bounds.centroid();
    centroid_bounds.include(centroid);
    cluster_centroid += as_vec3(centroid) / (Real)primitives_in_bounds.size();
    min = componentwise_min(centroid, min);
    max = componentwise_max(centroid, max);
  }

  auto const diff = abs(max - min);
  uint32_t split_axis = 0;
  float max_axis = 0;
  for (size_t i = 0; i < 3; ++i)
  {
    if (diff[i] > max_axis)
    {
      max_axis = diff[i];
      split_axis = as<uint32_t>(i);
    }
  }

  struct BVHSplitBucket
  {
    Bbox3f bounds;
    uint32_t count = 0;
  };

  constexpr size_t bucket_count = 12;
  BVHSplitBucket buckets[bucket_count];

  for (auto const prim : primitives_in_bounds)
  {
    auto const offset
        = centroid_bounds.offset(prim.bounds().centroid())[split_axis];
    auto b = as<size_t>(bucket_count * offset);
    if (b == bucket_count)
      b = bucket_count - 1;
    buckets[b].count += 1;
    buckets[b].bounds.merge(prim.bounds());
  }

  constexpr auto splits_count = bucket_count - 1;
  auto split_costs = std::array<float, splits_count>{};

  Bbox3f bounds_below;
  uint32_t count_below = 0;
  for (size_t i = 0; i < splits_count; ++i)
  {
    bounds_below.merge(buckets[i].bounds);
    count_below += buckets[i].count;
    split_costs[i] = (float)count_below * bounds_below.surface_area();
  }

  Bbox3f bounds_above;
  uint32_t count_above = 0;
  for (size_t i = splits_count; i >= 1; --i)
  {
    bounds_above.merge(buckets[i].bounds);
    count_above += buckets[i].count;
    split_costs[i - 1] += (float)count_above * bounds_above.surface_area();
  }

  auto const min_it = std::min_element(split_costs.begin(), split_costs.end());
  auto const min_bucket_index = as<size_t>(min_it - split_costs.begin());
  auto const min_cost = 1.f / 2.f + *min_it / bounds.surface_area();
  auto const leaf_cost = as<Real>(primitives_in_bounds.size());

  if (primitives_in_bounds.size() >= 64 || min_cost < leaf_cost)
  {
    ++internal_nodes;
    auto mid = std::ranges::partition(primitives_in_bounds,
        [&](auto p)
        {
          auto const offset
              = centroid_bounds.offset(p.bounds().centroid())[split_axis];
          auto b = as<size_t>(bucket_count * offset);
          if (b == bucket_count)
            b = bucket_count - 1;
          return b <= min_bucket_index;
        });

    std::unique_ptr<BVHBuildNode> left;
    std::unique_ptr<BVHBuildNode> right;
    if (primitives_in_bounds.size() > 1024)
    {
      auto build_leafs = JobList::task(
          [&]
          {
            left
                = build_subtree({ primitives_in_bounds.begin(), mid.begin() });
          },
          [&]
          {
            right = build_subtree({ mid.begin(), primitives_in_bounds.end() });
          });
      ThreadPool::get_global_thread_pool()->add(build_leafs);
      build_leafs->execute_remaining_and_wait();
    }
    else
    {
      left = build_subtree({ primitives_in_bounds.begin(), mid.begin() });
      right = build_subtree({ mid.begin(), primitives_in_bounds.end() });
    }

    return std::make_unique<BVHBuildNode>(BVHBuildNode{
        .bounds = bounds,
        .children = { std::move(left), std::move(right) },
        .split_axis = split_axis,
        .first_primitive = (uint32_t)0,
        .primitive_count = (uint32_t)0,
    });
  }
  return create_leaf(primitives_in_bounds);
}

uint32_t BVHBuilder::pack_tree(BVHBuildNode *tree)
{
  auto const node_index = nodes.size();
  nodes.emplace_back(BVHNode{ .bounds = tree->bounds,
      .fist_primitive = tree->first_primitive,
      .metadata
      = BVHNode::pack_metadata(tree->primitive_count, tree->split_axis) });
  if (tree->primitive_count == 0)
  {
    pack_tree(tree->children[0].get());
    nodes[node_index].right_child = pack_tree(tree->children[1].get());
  }
  PT_ASSERT(node_index <= std::numeric_limits<uint32_t>::max());
  return as<uint32_t>(node_index);
}

BVHAggregate::BVHAggregate(std::vector<Primitive> primitives)
{
  auto builder = BVHBuilder{ .primitives = primitives };
  builder.build();
  _primitives.swap(primitives);
  _nodes.swap(builder.nodes);
  _internal_nodes = builder.internal_nodes;
  _leaf_nodes = builder.leaf_nodes;
}

std::optional<PrimitiveIntersection> BVHAggregate::intersect(Ray ray) const
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
        if (ray.d[node.split_axis()] < 0)
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

void BVHAggregate::print_statistics()
{
  std::println("BVH statistics:");
  std::println("- primitives count: {}", split_thousands(_primitives.size()));
  std::println("- internal nodes: {}", split_thousands(_internal_nodes));
  std::println("- leaf nodes: {}", split_thousands(_leaf_nodes));
  std::println("- average primitive per leaf: {:.2f}",
      as<float>(_primitives.size()) / as<float>(_leaf_nodes));

  auto const node_memory = (_internal_nodes + _leaf_nodes) * sizeof(BVHNode);
  auto const triangle_memory = _primitives.size() * sizeof(Primitive);
  std::println("- memory use: {}", mem_string(node_memory + triangle_memory));
}