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
    auto const b = n->z * n->y * a;
    b1 = Vec3f{ 1._r + sign * pow2(n->x) * a, sign * b, -sign * n->x };
    b2 = Vec3f{ b, sign + pow2(n->y) * a, n->y };
  }

  Vec3f to_world(Vec3f const &v) const
  {
    auto const r1 = Vec3f(b2.x, n->x, b1.x);
    auto const r2 = Vec3f(b2.y, n->y, b1.y);
    auto const r3 = Vec3f(b2.z, n->z, b1.z);
    return Vec3f(dot(r1, v), dot(r2, v), dot(r3, v));
  }

  Vec3f to_local(Vec3f const &v) const
  {
    return Vec3f(dot(v, b2), dot(v, n), dot(v, b1));
  }

  UVec3f to_world(UVec3f const &v) const
  {
    // since the matrix is orthonormal (no scaling), the transformation has the
    // property that dot(local_from_world * v, local_from_world * v) == dot(v,
    // v) == 1
    return UVec3f::create_unchecked(to_world(v.unwrap()));
  }

  UVec3f to_local(UVec3f const &v) const
  {
    // see to_world for justification
    return UVec3f::create_unchecked(to_local(v.unwrap()));
  }

private:
  UNormal3f n;
  Vec3f b1;
  Vec3f b2;
};