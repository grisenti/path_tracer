#include "shapes.hpp"

#include <ranges>

#include "scene.hpp"

std::vector<Sphere> Shape::all_spheres;
std::vector<TriangleMesh> Shape::all_triangle_meshes;

TriangleMesh::TriangleMesh(std::vector<uint32_t> indices,
    std::vector<Point3f> positions, std::vector<Normal3f> normals)
    : _indices(std::move(indices)), _positions(std::move(positions)),
      _normals(std::move(normals))
{
  PT_ASSERT(_indices.size() % 3 == 0);
  PT_ASSERT(_indices.size() >= 3, "mesh contains 0 triangles");
  PT_ASSERT(_indices.size() / 3 < std::numeric_limits<uint32_t>::max(),
      "mesh created with too many faces");
  PT_ASSERT(_positions.size() >= 3);
  PT_ASSERT(_normals.empty() || _normals.size() == _positions.size());
}

Shape Shape::new_sphere(Properties const &props)
{
  props.specify_valid_properties({ "radius", "center" });
  auto const radius = props.get_float("radius");
  auto const center = props.get_point3("center");
  auto const idx = all_spheres.size();
  all_spheres.emplace_back(center, radius);
  return Shape(idx, ShapeType::Sphere);
}

std::vector<Shape> Shape::new_triangles(TriangleMesh mesh)
{
  auto const mesh_idx = all_triangle_meshes.size();
  auto const shapes_view = std::views::iota(0u, mesh.n_triangles()) |
                           std::views::transform(
                               [mesh_idx](uint32_t face_index)
                               {
                                 return Shape(
                                     pack_triangle_index(mesh_idx, face_index),
                                     ShapeType::Triangle);
                               });
  all_triangle_meshes.emplace_back(std::move(mesh));
  return { shapes_view.begin(), shapes_view.end() };
}

Shape Shape::new_invalid_shape()
{
  return Shape(0, ShapeType::End);
}