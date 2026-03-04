#pragma once

#include "common.hpp"

template <typename T> concept HandleType = requires(T type)
{
  static_cast<uint64_t>(type);
  static_cast<T>(uint64_t{});
};

template <HandleType T>
struct Handle
{
  static constexpr uint64_t TYPE_MASK = 0xff00000000000000;
  static constexpr uint64_t TYPE_SHIFT = 56;
  static constexpr uint64_t TYPE_MAX_VALUE = TYPE_MASK >> TYPE_SHIFT;

  Handle(uint64_t index, T type)
  {
    PT_ASSERT((index & TYPE_MASK) == 0);
    auto const type64 = static_cast<uint64_t>(type);
    PT_ASSERT(type64 <= TYPE_MAX_VALUE);
    handle = index | type64 << TYPE_SHIFT;
  }

  T type() const
  {
    return static_cast<T>((handle & TYPE_MASK) >> TYPE_SHIFT);
  }

  uint64_t index() const
  {
    return handle & ~TYPE_MASK;
  }

  uint64_t handle;
};