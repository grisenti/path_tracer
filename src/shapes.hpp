#pragma once

#include <optional>
#include <vector>

#include "common.hpp"
#include "handle.hpp"
#include "gmath/array_algorithms.hpp"

struct ShapeIntersection
{
  Point3f p;
  Real t;
  UNormal3f n;
};

class Sphere;
class TriangleMesh;
class Properties;

// Shape interface
class Shape
{
public:
  static std::vector<Sphere> all_spheres;
  static Shape new_sphere(Properties const &props);

  static std::vector<TriangleMesh> all_triangle_meshes;
  static std::vector<Shape> new_triangles(TriangleMesh mesh);

  // creates an invalid shape for testing purposes
  static Shape new_invalid_shape();

  std::optional<ShapeIntersection> intersect(Ray const &r) const;

  BoundingBox3f bounds() const;

private:
  enum class ShapeType : uint64_t
  {
    Sphere,
    Triangle,
    End,
  };

  static constexpr uint64_t TRIANGLE_INDEX_FACE_MASK = 0x00000000ffffffff;
  static constexpr uint64_t TRIANGLE_INDEX_MESH_MASK = 0x0fffffff00000000;
  static constexpr uint64_t TRIANGLE_INDEX_MESH_SHIFT = 32;

  static constexpr size_t FACE_COUNT_MAX = TRIANGLE_INDEX_FACE_MASK;
  static constexpr size_t MESH_COUNT_MAX =
      TRIANGLE_INDEX_MESH_MASK >> TRIANGLE_INDEX_MESH_SHIFT;

  static uint64_t pack_triangle_index(size_t mesh_index, size_t face_index)
  {
    PT_ASSERT(mesh_index <= MESH_COUNT_MAX);
    PT_ASSERT(face_index <= FACE_COUNT_MAX);
    auto const m_idx = static_cast<uint64_t>(mesh_index);
    auto const f_idx = static_cast<uint64_t>(face_index);
    return (m_idx << TRIANGLE_INDEX_MESH_SHIFT) | f_idx;
  }

  Shape(uint64_t index, ShapeType type) : _handle(index, type)
  {
  }

  // only relevant if type() == ShapeType::Triangle
  uint32_t mesh_index() const
  {
    PT_ASSERT(_handle.type() == ShapeType::Triangle);
    return (_handle.index() & TRIANGLE_INDEX_MESH_MASK) >>
           TRIANGLE_INDEX_MESH_SHIFT;
  }

  // only relevant if type() == ShapeType::Triangle
  uint32_t face_index() const
  {
    PT_ASSERT(_handle.type() == ShapeType::Triangle);
    return _handle.index() & TRIANGLE_INDEX_FACE_MASK;
  }

  Handle<ShapeType> _handle;
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

  BoundingBox3f bounds() const
  {
    auto const r_vec = splat<Vec3f>(_radius);
    return { _center - r_vec,
             _center + r_vec };
  }

private:
  Point3f _center;
  Real _radius;
};

class TriangleMesh
{
public:
  TriangleMesh(std::vector<uint32_t> indices, std::vector<Point3f> positions) :
    _indices(std::move(indices)), _positions(std::move(positions))
  {
    PT_ASSERT(_indices.size() % 3 == 0);
    PT_ASSERT(_indices.size() >= 3, "mesh contains 0 triangles");
    PT_ASSERT(_indices.size() / 3 < std::numeric_limits<uint32_t>::max(),
        "mesh created with too many faces");
    // there's at least one triangle
    PT_ASSERT(_positions.size() >= 3);
  }

  uint32_t n_triangles() const
  {
    return static_cast<uint32_t>(_indices.size() / 3);
  }

  std::optional<ShapeIntersection> intersect_face(uint32_t face_idx,
      Ray const &r) const
  {
    check_valid_index(face_idx);

    auto const v0 = _positions[_indices[face_idx * 3]];
    auto const v1 = _positions[_indices[face_idx * 3 + 1]];
    auto const v2 = _positions[_indices[face_idx * 3 + 2]];

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
    auto const v = dot(q_vec, r.d) * inv_det;
    if (!in_range(v, 0.f, 1.f - u))
      return {};

    auto const t = dot(q_vec, e2) * inv_det;
    if (!in_range(t, r.t_min, r.t_max))
      return {};

    auto const n = UNormal3f::normalize(cross(e1, e2));
    auto const p = v0 + e1 * u + e2 * v;
    return ShapeIntersection{ p, t, n };
  }

  BoundingBox3f face_bounds(uint32_t face_idx) const
  {
    check_valid_index(face_idx);
    auto const a = _positions[_indices[face_idx * 3]];
    auto const b = _positions[_indices[face_idx * 3 + 1]];
    auto const c = _positions[_indices[face_idx * 3 + 2]];
    return { a, b, c };
  }

private:
  void check_valid_index(uint32_t face_idx) const
  {
    PT_ASSERT(face_idx * 3 + 2 < _indices.size());
    PT_ASSERT(_indices[face_idx * 3] < _positions.size());
    PT_ASSERT(_indices[face_idx * 3 + 1] < _positions.size());
    PT_ASSERT(_indices[face_idx * 3 + 2] < _positions.size());
  }


  std::vector<uint32_t> _indices;
  std::vector<Point3f> _positions;
  // TODO: add these
  //std::vector<Normal3f> normals;
  //std::vector<Point2f> uvs;
};

inline std::optional<ShapeIntersection> Shape::intersect(Ray const &r) const
{
  switch (_handle.type())
  {
  case ShapeType::Sphere:
    return all_spheres[_handle.index()].intersect(r);
  case ShapeType::Triangle:
    return all_triangle_meshes[mesh_index()].intersect_face(
        face_index(), r);
  default:
    PT_FATAL("Shape not implemented");
  }
}

inline BoundingBox3f Shape::bounds() const
{
  switch (_handle.type())
  {
  case ShapeType::Sphere:
    return all_spheres[_handle.index()].bounds();
  case ShapeType::Triangle:
    return all_triangle_meshes[mesh_index()].face_bounds(
        face_index());
  default:
    PT_FATAL("Shape not implemented");
  }
}

