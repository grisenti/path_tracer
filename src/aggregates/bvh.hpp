#pragma once

#include "common.hpp"
#include "core/bbox.hpp"
#include "core/ray.hpp"
#include "bvh_build.hpp"

struct PrimitiveIntersection;
class Primitive;

struct BvhNode
{
  static uint32_t constexpr MAX_PRIMITIVE_COUNT = 0x3FFFFFFF;

  static uint32_t pack_metadata(uint32_t primitive_count, uint32_t split_axis)
  {
    PT_ASSERT(primitive_count < MAX_PRIMITIVE_COUNT);
    PT_ASSERT(split_axis < 3);
    return primitive_count | (split_axis << 30);
  }

  Bbox3f bounds;

  union
  {
    uint32_t fist_primitive;
    uint32_t right_child;
  };

  uint32_t metadata;

  uint32_t primitive_count() const
  {
    return metadata & 0x3FFFFFFF;
  }

  uint32_t split_axis() const
  {
    return metadata >> 30;
  }
};

class Bvh
{
public:
  Bvh(std::vector<Primitive> primitives, BVHBuildMethod build_method);

  std::optional<PrimitiveIntersection> intersect(Ray ray) const;

  bool is_hit_by(Ray ray) const;

  Bbox3f bounds() const;

  void print_statistics() const;

private:
  uint32_t pack_tree(BvhBuildNode *node);

  std::vector<Primitive> _primitives;
  std::vector<BvhNode> _nodes;
  size_t _leaf_nodes;
  size_t _internal_nodes;
};