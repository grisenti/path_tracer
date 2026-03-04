#pragma once

#include <optional>
#include <vector>

#include "common.hpp"

struct ShapeIntersection
{
  Point3f p;
  Real t;
  UNormal3f n;
};

enum class ShapeType : uint64_t
{
  Sphere,
  Triangle,

  End,
};

struct ShapeHandle
{
  static constexpr uint64_t SHAPE_TYPE_MASK = 0xff00000000000000;
  static constexpr uint64_t SHAPE_TYPE_SHIFT = 56;

  static constexpr uint64_t TRIANGLE_INDEX_FACE_MASK = 0x00000000ffffffff;
  static constexpr uint64_t TRIANGLE_INDEX_MESH_MASK = 0x0fffffff00000000;
  static constexpr uint64_t TRIANGLE_INDEX_MESH_SHIFT = 32;

  static uint64_t pack_triangle_index(uint64_t mesh_index, uint64_t face_index)
  {
    PT_ASSERT((face_index & ~TRIANGLE_INDEX_FACE_MASK) == 0);
    PT_ASSERT((mesh_index & ~TRIANGLE_INDEX_MESH_MASK) == 0);
    return (mesh_index << TRIANGLE_INDEX_MESH_SHIFT) | face_index;
  }

  ShapeHandle(uint64_t index, ShapeType type)
  {
    PT_ASSERT((index & SHAPE_TYPE_MASK) == 0);
    handle = index | static_cast<uint64_t>(type) << SHAPE_TYPE_SHIFT;
  }

  uint64_t shape_index() const
  {
    return handle & ~SHAPE_TYPE_MASK;
  }

  // only relevant if type() == ShapeType::Triangle
  uint32_t mesh_index() const
  {
    PT_ASSERT(type() == ShapeType::Triangle);
    return shape_index() & TRIANGLE_INDEX_MESH_MASK >>
           TRIANGLE_INDEX_MESH_SHIFT;
  }

  // only relevant if type() == ShapeType::Triangle
  uint32_t face_index() const
  {
    PT_ASSERT(type() == ShapeType::Triangle);
    return shape_index() & TRIANGLE_INDEX_FACE_MASK;
  }

  ShapeType type() const
  {
    return static_cast<ShapeType>(
      (handle & SHAPE_TYPE_MASK) >> SHAPE_TYPE_SHIFT);
  }

  uint64_t handle;
};

class Sphere;
class TriangleMesh;

// Shape interface
class Shape
{
public:
  static std::vector<Sphere> spheres;
  static std::vector<TriangleMesh> triangle_meshes;

  static ShapeHandle add_sphere_to_scene(Sphere s);
  static ShapeHandle add_triangle(Point3f a, Point3f b, Point3f c);

  Shape(ShapeHandle handle) : _handle(handle)
  {
  }

  std::optional<ShapeIntersection> intersect(Ray const &r) const;

private:
  ShapeHandle _handle;
};

class Sphere
{
public:
  Sphere(Point3f center, Real radius) : _center(center), _radius(radius)
  {
  }

  std::optional<ShapeIntersection> intersect(Ray const &r) const
  {
    auto const oc = r.o - _center;
    auto const a = square_magnitude(r.d);
    auto const hb = dot(r.d, oc);
    auto const c = square_magnitude(oc) - pow2(_radius);
    auto const delta = pow2(hb) - a * c;
    if (delta < 0)
      return std::nullopt;

    auto const sqrt_delta = std::sqrt(delta);
    auto t = (-hb - sqrt_delta) / a;
    if (!in_range(t, r.t_min, r.t_max))
    {
      t = (-hb + sqrt_delta) / a;
      if (!in_range(t, r.t_min, r.t_max))
        return std::nullopt;
    }
    auto const p = r.at(t);
    auto const n
        = UNormal3f::normalize((p - _center) / _radius);

    return ShapeIntersection{ p, t, n };
  }

private:
  Point3f _center;
  Real _radius;
};

class TriangleMesh
{
public:
  TriangleMesh(std::vector<uint32_t> &indices, std::vector<Point3f> &positions)
  {
    indices.swap(_indices);
    positions.swap(_positions);
  }

  std::optional<ShapeIntersection> intersect_triangle(uint32_t triangle_idx,
      Ray const &r) const
  {
    PT_ASSERT(triangle_idx + 2 < _indices.size());
    auto const v0 = _positions[_indices[triangle_idx]];
    auto const v1 = _positions[_indices[triangle_idx + 1]];
    auto const v2 = _positions[_indices[triangle_idx + 2]];

    // Möller–Trumbore 1997
    // EXPAND: could replace this with Watertight Ray/Triangle Intersection
    // by Woop 2013. Should be a bit slower but more precise.
    auto const e1 = v1 - v0;
    auto const e2 = v2 - v0;

    auto const p_vec = cross(r.d, e2);
    auto const det = dot(p_vec, e1);
    if (det > -1e-8f && det < 1e-8f)
      return {};

    auto const inv_det = 1.f / det;

    auto const t_vec = r.o - v0;
    auto const u = dot(p_vec, t_vec) * inv_det;
    if (!in_range(u, 0.f, 1.f))
      return {};

    auto const q_vec = cross(t_vec, e1);
    auto const v = dot(q_vec, r.d);
    if (!in_range(v, 0.f, 1.f - u))
      return {};

    auto const t = dot(q_vec, e2);
    if (!in_range(t, r.t_min, r.t_max))
      return {};

    auto n = UNormal3f::normalize(cross(e1, e2));
    auto p = v0 + e1 * u + e2 * v;
    return ShapeIntersection{ p, t, n };
  }

  std::optional<ShapeIntersection> intersect(Ray r) const
  {
    std::optional<ShapeIntersection> result;
    for (uint32_t i = 0; i < _indices.size(); i += 3)
    {
      result = intersect_triangle(i, r);
      if (result)
        r.t_max = result->t;
    }
    return result;
  }

private:
  std::vector<uint32_t> _indices;
  std::vector<Point3f> _positions;
  // TODO: add these
  //std::vector<Normal3f> normals;
  //std::vector<Point2f> uvs;
};

inline std::optional<ShapeIntersection> Shape::intersect(Ray const &r) const
{

  auto const shape_index = _handle.shape_index();
  auto const type = _handle.type();
  switch (type)
  {
  case ShapeType::Sphere:
    return spheres[shape_index].intersect(r);
  case ShapeType::Triangle:
  {
    auto const mesh_index = _handle.mesh_index();
    auto const face_index = _handle.face_index();
    return triangle_meshes[mesh_index].intersect_triangle(
        face_index, r);
  }
  default:
    return std::nullopt;
  }

}

