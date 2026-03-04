#include "shapes.hpp"

std::vector<Sphere> Shape::spheres;
std::vector<TriangleMesh> Shape::triangle_meshes;

Shape Shape::add_sphere_to_scene(Sphere s)
{
  auto const idx = spheres.size();
  spheres.push_back(s);

  return Shape(idx, ShapeType::Sphere);
}

Shape Shape::add_triangle(Point3f a, Point3f b, Point3f c)
{
  auto positions = std::vector{ a, b, c };
  auto indices = std::vector<uint32_t>{ 0, 1, 2 };
  auto const mesh_idx = triangle_meshes.size();
  triangle_meshes.emplace_back(indices, positions);
  return triangle_from_mesh_and_face_index(mesh_idx, 0);
}

Shape Shape::triangle_from_mesh_and_face_index(size_t mesh_index,
    size_t face_index)
{
  return Shape(pack_triangle_index(mesh_index, face_index),
      ShapeType::Triangle);
}