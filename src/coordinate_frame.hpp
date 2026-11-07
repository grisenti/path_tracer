#pragma once

#include "common.hpp"

class CoordinateFrame
{
public:
  // computes dot(n, v), where n is the normal of the local frame and
  // v is in local space
  static Real cos_theta(UVec3f const &v)
  {
    return v->y;
  }

  explicit CoordinateFrame(UNormal3f const &normal) : n(normal)
  {
    // from "Building an Orthonormal Basis, Revisited" by Duff et al.
    auto const sign = std::copysign(1._r, n->z);
    auto const a = -1._r / (sign + n->z);
    auto const b = n->x * n->y * a;
    b1 = Vec3f{ 1._r + sign * pow2(n->x) * a, sign * b, -sign * n->x };
    b2 = Vec3f{ b, sign + pow2(n->y) * a, -n->y };
  }

  Vec3f to_world(Vec3f const &v) const
  {
    return b2 * v.x + as_vec3(n) * v.y + b1 * v.z;
  }

  Vec3f to_local(Vec3f const &v) const
  {
    return Vec3f(dot(v, b2), dot(v, n), dot(v, b1));
  }

  UVec3f to_world(UVec3f const &v) const
  {
    // The matrix is orthonormal (no scaling), so in theory we could use the
    // unchecked version, but compounding errors can lead to the result being
    // slightly over/under unitary length
    return UVec3f::normalize(to_world(v.unwrap()));
  }

  UVec3f to_local(UVec3f const &v) const
  {
    return UVec3f::normalize(to_local(v.unwrap()));
  }

  UNormal3f normal() const
  {
    return n;
  }

private:
  UNormal3f n;
  Vec3f b1;
  Vec3f b2;
};