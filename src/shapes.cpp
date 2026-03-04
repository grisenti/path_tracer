#include "shapes.hpp"

std::vector<Sphere> Shape::spheres;
std::vector<TriangleMesh> Shape::triangle_meshes;

ShapeHandle Shape::add_sphere_to_scene(Sphere s)
{
  auto const idx = spheres.size();
  spheres.push_back(s);

  return ShapeHandle(idx, ShapeType::Sphere);
}

ShapeHandle Shape::add_triangle(Point3f a, Point3f b, Point3f c)
{
  auto positions = std::vector{ a, b, c };
  auto indices = std::vector<uint32_t>{ 0, 1, 2 };
  auto const mesh_idx = triangle_meshes.size();
  triangle_meshes.emplace_back(indices, positions);
  return ShapeHandle(ShapeHandle::pack_triangle_index(mesh_idx, 0),
      ShapeType::Triangle);
}