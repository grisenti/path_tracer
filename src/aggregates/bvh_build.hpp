#pragma once

#include "common.hpp"

#include <memory>
#include <atomic>
#include <span>

#include "core/bbox.hpp"

class Primitive;

struct BvhBuildNode
{
  Bbox3f bounds;
  std::unique_ptr<BvhBuildNode> children[2];
  uint32_t split_axis;
  uint32_t first_primitive;
  uint32_t primitive_count;
};

struct BvhBuildResult
{
  std::unique_ptr<BvhBuildNode> root;
  size_t leaf_nodes;
  size_t internal_nodes;
};

BvhBuildResult build_bvh(std::span<Primitive> primitives);