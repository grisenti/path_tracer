#include "bvh_build.hpp"

#include "concurrency.hpp"
#include "primitives.hpp"

namespace
{

struct BVHBuilderSAH
{
  static size_t constexpr BUILD_LEAF_THRESHOLD = 5;

  BVHBuilderSAH(
      std::span<Primitive> primitives, uint32_t max_primitives_per_leaf)
      : primitives(primitives),
        max_primitives_per_leaf(max_primitives_per_leaf)
  {
    PT_ASSERT(BUILD_LEAF_THRESHOLD <= max_primitives_per_leaf);
  }

  std::unique_ptr<BvhBuildNode> build()
  {
    return build_subtree(primitives);
  }

  std::unique_ptr<BvhBuildNode> build_subtree(
      std::span<Primitive> primitives_in_bounds);

  std::unique_ptr<BvhBuildNode> create_leaf(
      std::span<Primitive> leaf_primitives);

  std::span<Primitive> primitives;
  std::atomic<size_t> leaf_nodes = 0;
  std::atomic<size_t> internal_nodes = 0;
  uint32_t max_primitives_per_leaf;
};

std::unique_ptr<BvhBuildNode> BVHBuilderSAH::create_leaf(
    std::span<Primitive> leaf_primitives)
{
  ++leaf_nodes;
  Bbox3f bounds{};
  for (auto const prim : leaf_primitives)
  {
    bounds = Bbox3f(bounds, prim.bounds());
  }
  auto const start = leaf_primitives.data() - primitives.data();
  return std::make_unique<BvhBuildNode>(BvhBuildNode{
      .bounds = bounds,
      .children = { nullptr, nullptr },
      .split_axis = 0,
      .first_primitive = as<uint32_t>(start),
      .primitive_count = as<uint32_t>(leaf_primitives.size()),
  });
}

std::unique_ptr<BvhBuildNode> BVHBuilderSAH::build_subtree(
    std::span<Primitive> primitives_in_bounds)
{
  if (primitives_in_bounds.size() <= BUILD_LEAF_THRESHOLD)
    return create_leaf(primitives_in_bounds);

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

  if (primitives_in_bounds.size() > max_primitives_per_leaf
      || min_cost < leaf_cost)
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

    PT_ASSERT(mid.begin() != primitives_in_bounds.begin()
              && mid.begin() != primitives_in_bounds.end());

    std::unique_ptr<BvhBuildNode> left;
    std::unique_ptr<BvhBuildNode> right;
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

    return std::make_unique<BvhBuildNode>(BvhBuildNode{
        .bounds = bounds,
        .children = { std::move(left), std::move(right) },
        .split_axis = split_axis,
        .first_primitive = 0u,
        .primitive_count = 0u,
    });
  }
  return create_leaf(primitives_in_bounds);
}

struct MortonPrimitive
{
  uint32_t morton_index = 0;
  size_t primitive_index = 0;
};

struct Treelet
{
  size_t start;
  size_t count;
};

uint32_t space_bits_by_two(uint32_t x)
{
  if (x == 1 << 10)
    x -= 1;
  x = (x | (x << 10)) & 0b00'00000'00000'11111'00000'00000'11111;
  x = (x | (x << 4)) & 0b00'00000'01110'00011'00000'01110'00011;
  x = (x | (x << 2)) & 0b00'00001'10010'01001'00001'10010'01001;
  x = (x | (x << 2)) & 0b00'00100'10010'01001'00100'10010'01001;
  return x;
}

uint32_t morton_code(Point3f const &p)
{
  return space_bits_by_two(as<uint32_t>(p.x))
         | space_bits_by_two(as<uint32_t>(p.y)) << 1
         | space_bits_by_two(as<uint32_t>(p.z)) << 2;
}

struct HLBVHBuilder
{
  static size_t constexpr BUILD_LEAF_THRESHOLD = 5;

  HLBVHBuilder(
      std::span<Primitive> primitives, uint32_t max_primitives_per_leaf)
      : primitives(primitives), morton_primitives(primitives.size()),
        max_primitives_per_leaf(max_primitives_per_leaf)
  {
    PT_ASSERT(BUILD_LEAF_THRESHOLD <= max_primitives_per_leaf);
  }

  std::unique_ptr<BvhBuildNode> build();

  std::unique_ptr<BvhBuildNode> build_subtree(
      std::span<MortonPrimitive> subtree_primitives,
      int32_t split_bit_index) const;

  std::unique_ptr<BvhBuildNode> create_leaf(
      std::span<MortonPrimitive> leaf_primitives) const;

  std::unique_ptr<BvhBuildNode> build_upper_sah(
      std::span<std::unique_ptr<BvhBuildNode>> treelet_nodes);

  std::unique_ptr<BvhBuildNode> build_lower_median_split(
      std::span<MortonPrimitive> subtree_primitives) const;

  std::span<Primitive> primitives;
  std::vector<MortonPrimitive> morton_primitives;
  mutable std::atomic<size_t> leaf_nodes = 0;
  mutable std::atomic<size_t> internal_nodes = 0;
  uint32_t max_primitives_per_leaf;
};

std::unique_ptr<BvhBuildNode> HLBVHBuilder::build()
{
  Bbox3f centroid_bounds;
  for (auto const prim : primitives)
    centroid_bounds.include(prim.bounds().centroid());

  for (size_t i = 0; i < primitives.size(); ++i)
  {
    auto const prim = primitives[i];
    auto const centroid = prim.bounds().centroid();
    auto const offset = centroid_bounds.offset(centroid);
    auto const scaled_offset = scale(offset, exact_power_of_two<Real>(10));
    auto const morton_index = morton_code(scaled_offset);
    morton_primitives[i] = MortonPrimitive{
      .morton_index = morton_index,
      .primitive_index = i,
    };
  }

  std::ranges::sort(morton_primitives,
      [](MortonPrimitive const &lhs, MortonPrimitive const &rhs)
      { return lhs.morton_index < rhs.morton_index; });

  constexpr size_t N_TREELETS = 1 << 12;
  constexpr uint32_t TREELET_BITS_MASK = ((1 << 12) - 1) << 18;
  std::vector<Treelet> treelets;
  treelets.reserve(N_TREELETS);
  size_t current_treelet_start = 0;
  for (size_t i = 0; i < N_TREELETS; ++i)
  {
    uint32_t const treelet_bits = as<uint32_t>(i) << 18;
    for (size_t j = current_treelet_start; j < morton_primitives.size(); ++j)
    {
      auto const [morton_index, _] = morton_primitives[j];
      if ((morton_index & TREELET_BITS_MASK) != treelet_bits)
      {
        if (j != current_treelet_start)
        {
          treelets.push_back(Treelet{
              .start = current_treelet_start,
              .count = j - current_treelet_start,
          });
        }
        current_treelet_start = j;
        break;
      }
    }
  }

  if (current_treelet_start < morton_primitives.size())
  {
    treelets.push_back(Treelet{
        .start = current_treelet_start,
        .count = morton_primitives.size() - current_treelet_start,
    });
  }

  std::vector<std::unique_ptr<BvhBuildNode>> treelet_nodes(treelets.size());
  const auto subtree_build_task = RepeatedJob::task(treelets.size(),
      [&](size_t i)
      {
        auto const [start, count] = treelets[i];
        auto treelet_node
            = build_subtree({ morton_primitives.data() + start, count }, 17);
        treelet_nodes[i].swap(treelet_node);
      });
  ThreadPool::get_global_thread_pool()->add(subtree_build_task);
  subtree_build_task->execute_remaining_and_wait();

  auto root = build_upper_sah(treelet_nodes);

  // TODO: an algorithm that uses O(1) space exists, use that here
  // reorder primitives using the permutation in morton_primitives
  std::vector<Primitive> reordered_primitives;
  reordered_primitives.reserve(primitives.size());
  for (auto const [_, prim_idx] : morton_primitives)
    reordered_primitives.push_back(primitives[prim_idx]);
  for (size_t i = 0; i < primitives.size(); ++i)
    primitives[i] = reordered_primitives[i];

  return root;
}

std::unique_ptr<BvhBuildNode> HLBVHBuilder::create_leaf(
    std::span<MortonPrimitive> leaf_primitives) const
{
  PT_ASSERT(!leaf_primitives.empty());
  Bbox3f bounds;
  for (auto const &prim : leaf_primitives)
    bounds.merge(primitives[prim.primitive_index].bounds());
  auto const first
      = as<uint32_t>(leaf_primitives.data() - morton_primitives.data());
  auto const count = as<uint32_t>(leaf_primitives.size());
  PT_ASSERT(count <= max_primitives_per_leaf);
  ++leaf_nodes;
  return std::make_unique<BvhBuildNode>(BvhBuildNode{
      .bounds = bounds,
      .children = { nullptr, nullptr },
      .split_axis = 0u,
      .first_primitive = first,
      .primitive_count = count,
  });
}

// TODO: shares a lot with BVHBuilderSAH, factor out common stuff
std::unique_ptr<BvhBuildNode> HLBVHBuilder::build_upper_sah(
    std::span<std::unique_ptr<BvhBuildNode>> treelet_nodes)
{
  if (treelet_nodes.size() == 1)
    return std::move(treelet_nodes[0]);

  Bbox3f bounds{};
  Bbox3f centroid_bounds;

  for (auto const &node : treelet_nodes)
  {
    auto const node_bounds = node->bounds;
    bounds.merge(node_bounds);
    auto const centroid = node_bounds.centroid();
    centroid_bounds.include(centroid);
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

  constexpr size_t BUCKET_COUNT = 12;
  BVHSplitBucket buckets[BUCKET_COUNT];

  for (auto const &node : treelet_nodes)
  {
    auto const node_bounds = node->bounds;
    auto const offset
        = centroid_bounds.offset(node_bounds.centroid())[split_axis];
    auto b = as<size_t>(BUCKET_COUNT * offset);
    if (b == BUCKET_COUNT)
      b = BUCKET_COUNT - 1;
    buckets[b].count += 1;
    buckets[b].bounds.merge(node_bounds);
  }

  constexpr auto splits_count = BUCKET_COUNT - 1;
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

  auto const mid = std::ranges::partition(treelet_nodes,
      [&](auto const &node)
      {
        auto const offset
            = centroid_bounds.offset(node->bounds.centroid())[split_axis];
        auto b = as<size_t>(BUCKET_COUNT * offset);
        if (b == BUCKET_COUNT)
          b = BUCKET_COUNT - 1;
        return b <= min_bucket_index;
      });

  PT_ASSERT(mid.begin() != treelet_nodes.begin()
            && mid.begin() != treelet_nodes.end());

  auto left = build_upper_sah({ treelet_nodes.begin(), mid.begin() });
  auto right = build_upper_sah({ mid.begin(), treelet_nodes.end() });

  ++internal_nodes;
  return std::make_unique<BvhBuildNode>(BvhBuildNode{
      .bounds = bounds,
      .children = { std::move(left), std::move(right) },
      .split_axis = split_axis,
      .first_primitive = 0u,
      .primitive_count = 0u,
  });
}

std::unique_ptr<BvhBuildNode> HLBVHBuilder::build_lower_median_split(
    std::span<MortonPrimitive> subtree_primitives) const
{
  if (subtree_primitives.size() <= BUILD_LEAF_THRESHOLD)
    return create_leaf(subtree_primitives);

  Bbox3f centroid_bounds;

  for (auto const &node : subtree_primitives)
  {
    auto const prim = primitives[node.primitive_index];
    auto const node_bounds = prim.bounds();
    auto const centroid = node_bounds.centroid();
    centroid_bounds.include(centroid);
  }
  auto const split_axis = centroid_bounds.max_dim();
  auto const mid_point = centroid_bounds.centroid()[split_axis];
  auto const right_range = std::ranges::partition(subtree_primitives,
      [&](MortonPrimitive const &prim)
      {
        return primitives[prim.primitive_index].bounds().centroid()[split_axis]
               < mid_point;
      });

  std::unique_ptr<BvhBuildNode> left;
  std::unique_ptr<BvhBuildNode> right;
  if (right_range.empty() || right_range.begin() == subtree_primitives.begin())
  {
    left = build_lower_median_split(
        subtree_primitives.subspan(0, subtree_primitives.size() / 2));
    right = build_lower_median_split(
        subtree_primitives.subspan(subtree_primitives.size() / 2));
  }
  else
  {
    left = build_lower_median_split(
        { subtree_primitives.begin(), right_range.begin() });
    right
        = build_lower_median_split({ right_range.begin(), right_range.end() });
  }

  ++internal_nodes;
  return std::make_unique<BvhBuildNode>(BvhBuildNode{
      .bounds = Bbox3f(left->bounds, right->bounds),
      .children = { std::move(left), std::move(right) },
      .split_axis = as<uint32_t>(split_axis),
      .first_primitive = 0u,
      .primitive_count = 0u,
  });
}

std::unique_ptr<BvhBuildNode> HLBVHBuilder::build_subtree(
    std::span<MortonPrimitive> subtree_primitives,
    int32_t split_bit_index) const
{
  if (subtree_primitives.size() <= 5)
  {
    PT_ASSERT(subtree_primitives.size() <= max_primitives_per_leaf);
    return create_leaf(subtree_primitives);
  }

  if (split_bit_index < 0)
    return build_lower_median_split(subtree_primitives);

  auto const split_bit = as<uint32_t>(1 << split_bit_index);
  // primitives are sorted wrt the split_bit so we can use binary search
  auto const mid = std::ranges::partition_point(subtree_primitives,
      [split_bit](MortonPrimitive const &prim)
      { return !(prim.morton_index & split_bit); });

  if (subtree_primitives.begin() == mid || mid == subtree_primitives.end())
  {
    return build_subtree(subtree_primitives, split_bit_index - 1);
  }

  auto left = build_subtree(
      { subtree_primitives.begin(), mid }, split_bit_index - 1);
  auto right
      = build_subtree({ mid, subtree_primitives.end() }, split_bit_index - 1);

  auto const split_axis = as<uint32_t>(split_bit_index % 3);
  ++internal_nodes;
  return std::make_unique<BvhBuildNode>(BvhBuildNode{
      .bounds = Bbox3f(left->bounds, right->bounds),
      .children = { std::move(left), std::move(right) },
      .split_axis = split_axis,
      .first_primitive = 0u,
      .primitive_count = 0u,
  });
}

}

template <typename Builder>
BvhBuildResult build_bvh(
    std::span<Primitive> primitives, uint32_t max_primitives_per_leaf)
{
  Builder builder(primitives, max_primitives_per_leaf);
  auto root = builder.build();
  return {
    .root = std::move(root),
    .leaf_nodes = builder.leaf_nodes,
    .internal_nodes = builder.internal_nodes,
  };
}

BvhBuildResult build_bvh(std::span<Primitive> primitives,
    uint32_t max_primitives_per_leaf, BVHBuildMethod build_method)
{
  if (build_method == BVHBuildMethod::SAH)
    return build_bvh<BVHBuilderSAH>(primitives, max_primitives_per_leaf);
  if (build_method == BVHBuildMethod::HLBVH)
    return build_bvh<HLBVHBuilder>(primitives, max_primitives_per_leaf);
  PT_FATAL("invalid bvh build method");
}