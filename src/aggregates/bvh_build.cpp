#include "bvh_build.hpp"

#include "concurrency.hpp"
#include "primitives.hpp"

namespace
{

struct BVHBuilder
{
  BVHBuilder(std::span<Primitive> primitives) : primitives(primitives)
  {
  }

  std::unique_ptr<BVHBuildNode> build()
  {
    return build_subtree(primitives);
  }

  std::unique_ptr<BVHBuildNode> build_subtree(
      std::span<Primitive> primitives_in_bounds);

  std::unique_ptr<BVHBuildNode> create_leaf(
      std::span<Primitive> leaf_primitives);

  std::span<Primitive> primitives;
  std::atomic<size_t> leaf_nodes = 0;
  std::atomic<size_t> internal_nodes = 0;
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
      .first_primitive = as<uint32_t>(start),
      .primitive_count = as<uint32_t>(leaf_primitives.size()),
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

  for (auto const prim : primitives_in_bounds)
  {
    auto const prim_bounds = prim.bounds();
    bounds.merge(prim_bounds);
    auto const centroid = prim_bounds.centroid();
    centroid_bounds.include(centroid);
    cluster_centroid
        += as_vec3(centroid) / as<Real>(primitives_in_bounds.size());
  }

  auto const diff = abs(centroid_bounds.max - centroid_bounds.min);
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
  auto split_costs = std::array<Real, splits_count>{};

  Bbox3f bounds_below;
  uint32_t count_below = 0;
  for (size_t i = 0; i < splits_count; ++i)
  {
    bounds_below.merge(buckets[i].bounds);
    count_below += buckets[i].count;
    split_costs[i] = as<Real>(count_below) * bounds_below.surface_area();
  }

  Bbox3f bounds_above;
  uint32_t count_above = 0;
  for (size_t i = splits_count; i >= 1; --i)
  {
    bounds_above.merge(buckets[i].bounds);
    count_above += buckets[i].count;
    split_costs[i - 1] += as<Real>(count_above) * bounds_above.surface_area();
  }

  auto const min_it = std::ranges::min_element(split_costs);
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
        .first_primitive = 0u,
        .primitive_count = 0u,
    });
  }
  return create_leaf(primitives_in_bounds);
}

}

BVHBuildResult build_bvh(std::span<Primitive> primitives)
{
  BVHBuilder builder(primitives);
  auto root = builder.build();
  return {
    .root = std::move(root),
    .leaf_nodes = builder.leaf_nodes,
    .internal_nodes = builder.internal_nodes,
  };
}