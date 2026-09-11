#pragma once

#include "common.hpp"

#include <vector>

#include "core/simd.hpp"
#include "core/ray.hpp"
#include "core/bbox.hpp"

class Primitive;
struct PrimitiveIntersection;
struct BvhBuildNode;

struct Bvh8Node
{
  v8f corners[2][3];
  v8i children;
  v8i permutations;
};

struct Bvh8ChildIndex
{
  static constexpr uint32_t INDEX_MASK = 0x0FFFFFFF;
  static constexpr uint32_t PRIMITIVE_COUNT_MASK = 0xF0000000;
  static constexpr uint32_t PRIMITIVE_COUNT_SHIFT = 28;

  uint32_t value = 0;

  Bvh8ChildIndex() = default;

  Bvh8ChildIndex(uint32_t index, uint32_t primitive_count)
  {
    value = primitive_count << PRIMITIVE_COUNT_SHIFT;
    value |= index;
  }

  static uint32_t primitive_count(uint32_t index)
  {
    return (index & PRIMITIVE_COUNT_MASK) >> PRIMITIVE_COUNT_SHIFT;
  }

  static uint32_t primitive_starting_index(uint32_t index)
  {
    return index & INDEX_MASK;
  }

  static uint32_t index(uint32_t index)
  {
    return index & INDEX_MASK;
  }

  static bool is_leaf(uint32_t index)
  {
    return index & PRIMITIVE_COUNT_MASK;
  }
};

class Bvh8
{
public:
  Bvh8(std::vector<Primitive> primitives);

  std::optional<PrimitiveIntersection> intersect(Ray ray) const;

  Bbox3f bounds() const;

  void print_statistics() const;

private:
  Bvh8ChildIndex pack_tree(BvhBuildNode *build_node);

  std::vector<Primitive> _primitives;
  std::vector<Bvh8Node> _nodes;
  Bbox3f _bounds;
  size_t _leaf_nodes_count;
  size_t _internal_nodes_count;
};