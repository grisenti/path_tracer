#include "shapes.hpp"

#include "scene.hpp"

std::vector<Sphere> Shape::all_spheres;
std::vector<TriangleMesh> Shape::all_triangle_meshes;

Shape Shape::new_sphere(Properties const &props)
{
  auto const radius = props.get_float("radius");
  auto const center = props.get_point("center");
  auto const idx = all_spheres.size();
  all_spheres.emplace_back(center, radius);
  return Shape(idx, ShapeType::Sphere);
}

Shape Shape::add_triangle(Point3f a, Point3f b, Point3f c)
{
  auto positions = std::vector{ a, b, c };
  auto indices = std::vector<uint32_t>{ 0, 1, 2 };
  auto const mesh_idx = all_triangle_meshes.size();
  all_triangle_meshes.emplace_back(indices, positions);
  return triangle_from_mesh_and_face_index(mesh_idx, 0);
}

Shape Shape::triangle_from_mesh_and_face_index(size_t mesh_index,
    size_t face_index)
{
  return Shape(pack_triangle_index(mesh_index, face_index),
      ShapeType::Triangle);
}