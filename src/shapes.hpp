#pragma once

#include <optional>
#include <vector>

#include "common.hpp"
#include "core/bbox.hpp"
#include "core/handle.hpp"
#include "coordinate_frame.hpp"
#include "gmath/array_algorithms.hpp"

struct ShapeIntersection
{
  ShapeIntersection() = default;

  ShapeIntersection(Point3f const &p, Real const ray_t, Point2f const uv,
      UNormal3f const &geo_normal, UNormal3f const &sh_normal)
      : p(p), t(ray_t), uv(uv), geo_frame(geo_normal), sh_frame(sh_normal)
  {
  }

  ShapeIntersection(Ray const &r, Real const t, Point2f const uv,
      UNormal3f const &geo_normal, UNormal3f const &sh_normal)
      : ShapeIntersection(r.at(t), t, uv, geo_normal, sh_normal)
  {
  }

  Point3f p = { 0, 0, 0 };
  Real t = 0;
  Point2f uv = { 0, 0 };
  CoordinateFrame geo_frame
      = CoordinateFrame(UNormal3f::create_unchecked(0, 1, 0));
  CoordinateFrame sh_frame
      = CoordinateFrame(UNormal3f::create_unchecked(0, 1, 0));
};

class Sphere;
class TriangleMesh;
class Properties;

struct SampledPosition
{
  Point3f pos;
  UNormal3f normal;
  Real pdf;
};

// Shape interface
class Shape
{
public:
  static Shape new_sphere(Properties const &props);
  static std::vector<Shape> new_triangles(TriangleMesh mesh);

  // creates an invalid shape for testing purposes
  static Shape new_invalid_shape();

  std::optional<ShapeIntersection> intersect(Ray const &r) const;
  bool is_hit_by(Ray const &r) const;
  SampledPosition sample_position(Point2f sample) const;
  Real pdf() const;

  Bbox3f bounds() const;

private:
  static std::vector<Sphere> all_spheres;
  static std::vector<TriangleMesh> all_triangle_meshes;

  enum class ShapeType : uint64_t
  {
    Sphere,
    Triangle,
    End,
  };

  static constexpr uint64_t TRIANGLE_INDEX_FACE_MASK = 0x00000000ffffffff;
  static constexpr uint64_t TRIANGLE_INDEX_MESH_MASK = 0x0fffffff00000000;
  static constexpr uint64_t TRIANGLE_INDEX_MESH_SHIFT = 32;

  static constexpr uint64_t FACE_COUNT_MAX = TRIANGLE_INDEX_FACE_MASK;
  static constexpr uint64_t MESH_COUNT_MAX
      = TRIANGLE_INDEX_MESH_MASK >> TRIANGLE_INDEX_MESH_SHIFT;

  static uint64_t pack_triangle_index(uint64_t mesh_index, uint64_t face_index)
  {
    PT_ASSERT(mesh_index <= MESH_COUNT_MAX);
    PT_ASSERT(face_index <= FACE_COUNT_MAX);
    auto const m_idx = mesh_index;
    auto const f_idx = face_index;
    return (m_idx << TRIANGLE_INDEX_MESH_SHIFT) | f_idx;
  }

  Shape(uint64_t index, ShapeType type) : _handle(index, type)
  {
  }

  // only relevant if type() == ShapeType::Triangle
  uint32_t mesh_index() const
  {
    PT_ASSERT(_handle.type() == ShapeType::Triangle);
    return (_handle.index() & TRIANGLE_INDEX_MESH_MASK)
           >> TRIANGLE_INDEX_MESH_SHIFT;
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
    auto const a = square_magnitude(r.d());
    auto const hb = dot(r.d(), oc);
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
    auto const n = UNormal3f::normalize((p - _center) / _radius);

    // TODO: uvs
    return ShapeIntersection{ p, t, {}, n, n };
  }

  bool is_hit_by(Ray const &r) const
  {
    auto const oc = r.o - _center;
    auto const a = square_magnitude(r.d());
    auto const hb = dot(r.d(), oc);
    auto const c = square_magnitude(oc) - pow2(_radius);
    auto const delta = pow2(hb) - a * c;
    if (delta < 0)
      return false;

    auto const sqrt_delta = std::sqrt(delta);
    auto t = (-hb - sqrt_delta) / a;
    if (!in_range(t, r.t_min, r.t_max))
    {
      t = (-hb + sqrt_delta) / a;
      if (!in_range(t, r.t_min, r.t_max))
        return false;
    }
    return true;
  }

  Bbox3f bounds() const
  {
    auto const r_vec = splat<Vec3f>(_radius);
    return { _center - r_vec, _center + r_vec };
  }

private:
  Point3f _center;
  Real _radius;
};

class TriangleMesh
{
public:
  TriangleMesh(std::vector<uint32_t> indices, std::vector<Point3f> positions,
      std::vector<Normal3f> normals = {});

  uint32_t n_triangles() const
  {
    return as<uint32_t>(_indices.size() / 3);
  }

  std::optional<ShapeIntersection> intersect_face(
      uint32_t face_idx, Ray const &r) const
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

    auto const p_vec = cross(r.d(), e2);
    auto const det = dot(p_vec, e1);
    if (det > -1e-8f && det < 1e-8f)
      return {};

    auto const inv_det = 1.f / det;

    auto const t_vec = r.o - v0;
    auto const u = dot(p_vec, t_vec) * inv_det;
    if (!in_range(u, 0.f, 1.f))
      return {};

    auto const q_vec = cross(t_vec, e1);
    auto const v = dot(q_vec, r.d()) * inv_det;
    if (!in_range(v, 0.f, 1.f - u))
      return {};

    auto const t = dot(q_vec, e2) * inv_det;
    if (!in_range(t, r.t_min, r.t_max))
      return {};

    auto const geo_n = UNormal3f::normalize(cross(e1, e2));
    auto sh_n = geo_n;
    if (!_normals.empty())
    {
      auto const n0 = _normals[_indices[face_idx * 3 + 0]];
      auto const n1 = _normals[_indices[face_idx * 3 + 1]];
      auto const n2 = _normals[_indices[face_idx * 3 + 2]];
      sh_n = UNormal3f::normalize(n0 * (1 - u - v) + n1 * u + n2 * v);
    }

    auto const p = v0 + e1 * u + e2 * v;
    // TODO: uvs
    return ShapeIntersection{ p, t, {}, geo_n, sh_n };
  }

  bool is_face_hit_by(uint32_t face_idx, Ray const &r) const
  {
    check_valid_index(face_idx);

    auto const v0 = _positions[_indices[face_idx * 3]];
    auto const v1 = _positions[_indices[face_idx * 3 + 1]];
    auto const v2 = _positions[_indices[face_idx * 3 + 2]];

    // Möller–Trumbore 1997
    // EXPAND: same as intersect_face
    auto const e1 = v1 - v0;
    auto const e2 = v2 - v0;

    auto const p_vec = cross(r.d(), e2);
    auto const det = dot(p_vec, e1);
    if (det > -1e-8f && det < 1e-8f)
      return false;

    auto const inv_det = 1.f / det;

    auto const t_vec = r.o - v0;
    auto const u = dot(p_vec, t_vec) * inv_det;
    if (!in_range(u, 0.f, 1.f))
      return false;

    auto const q_vec = cross(t_vec, e1);
    auto const v = dot(q_vec, r.d()) * inv_det;
    if (!in_range(v, 0.f, 1.f - u))
      return false;

    auto const t = dot(q_vec, e2) * inv_det;
    if (!in_range(t, r.t_min, r.t_max))
      return false;

    return true;
  }

  SampledPosition sample_position(uint32_t face_idx, Point2f sample)
  {
    auto const p0 = _positions[_indices[face_idx * 3]];
    auto const p1 = _positions[_indices[face_idx * 3 + 1]];
    auto const p2 = _positions[_indices[face_idx * 3 + 2]];
    auto const v1 = p1 - p0;
    auto const v2 = p2 - p0;

    auto const alpha = 1._r - std::sqrt(1._r - sample.x);
    auto const beta = sample.y * std::sqrt(1._r - sample.x);
    Point3f const p = p0 + alpha * v1 + beta * v2;
    UNormal3f const n = UNormal3f::normalize(Normal3f::cross(v1, v2));
    return { p, n, 1._r / surface_area(face_idx) };
  }

  Real pdf(uint32_t face_idx)
  {
    check_valid_index(face_idx);
    return 1._r / surface_area(face_idx);
  }

  Bbox3f face_bounds(uint32_t face_idx) const
  {
    check_valid_index(face_idx);
    auto const a = _positions[_indices[face_idx * 3]];
    auto const b = _positions[_indices[face_idx * 3 + 1]];
    auto const c = _positions[_indices[face_idx * 3 + 2]];
    return { a, b, c };
  }

  Real surface_area(uint32_t face_idx) const
  {
    check_valid_index(face_idx);
    auto const p0 = _positions[_indices[face_idx * 3]];
    auto const p1 = _positions[_indices[face_idx * 3 + 1]];
    auto const p2 = _positions[_indices[face_idx * 3 + 2]];
    return 0.5_r * length(cross(p1 - p0, p2 - p0));
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
  std::vector<Normal3f> _normals;
};

inline std::optional<ShapeIntersection> Shape::intersect(Ray const &r) const
{
  switch (_handle.type())
  {
  case ShapeType::Sphere:
    return all_spheres[_handle.index()].intersect(r);
  case ShapeType::Triangle:
    return all_triangle_meshes[mesh_index()].intersect_face(face_index(), r);
  default:
    PT_FATAL("Shape not implemented");
  }
}

inline SampledPosition Shape::sample_position(Point2f sample) const
{
  switch (_handle.type())
  {
  case ShapeType::Triangle:
    return all_triangle_meshes[mesh_index()].sample_position(
        face_index(), sample);
  default:
    PT_FATAL("Shape not implemented");
  }
}

inline Real Shape::pdf() const
{
  switch (_handle.type())
  {
  case ShapeType::Triangle:
    return all_triangle_meshes[mesh_index()].pdf(face_index());
  default:
    PT_FATAL("Shape not implemented");
  }
}

inline Bbox3f Shape::bounds() const
{
  switch (_handle.type())
  {
  case ShapeType::Sphere:
    return all_spheres[_handle.index()].bounds();
  case ShapeType::Triangle:
    return all_triangle_meshes[mesh_index()].face_bounds(face_index());
  default:
    PT_FATAL("Shape not implemented");
  }
}

inline bool Shape::is_hit_by(Ray const &r) const
{
  switch (_handle.type())
  {
  case ShapeType::Sphere:
    return all_spheres[_handle.index()].is_hit_by(r);
  case ShapeType::Triangle:
    return all_triangle_meshes[mesh_index()].is_face_hit_by(face_index(), r);
  default:
    PT_FATAL("Shape not implemented");
  }
}