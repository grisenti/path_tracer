#pragma once

#include "common.hpp"
#include "core/bbox.hpp"
#include "core/ray.hpp"

struct PrimitiveIntersection;
class Primitive;
struct BVHBuildNode;

struct BVHNode
{
  static uint32_t pack_metadata(uint32_t primitive_count, uint32_t split_axis)
  {
    PT_ASSERT(primitive_count < 0x3FFFFFFF);
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

class BVH
{
public:
  BVH(std::vector<Primitive> primitives);

  std::optional<PrimitiveIntersection> intersect(Ray ray) const;

  Bbox3f bounds() const;

  void print_statistics() const;

private:
  uint32_t pack_tree(BVHBuildNode *node);

  std::vector<Primitive> _primitives;
  std::vector<BVHNode> _nodes;
  size_t _leaf_nodes;
  size_t _internal_nodes;
};