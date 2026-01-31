#include "cameras.hpp"

#include <gmath/array_algorithms.hpp>

#include "gmath/vec.hpp"
#include "gmath/transform/transform2D.hpp"

OrthographicCamera::OrthographicCamera(Extent2u const &image_extent_u,
    Point3f position)
{
  image_extent.x = image_extent_u.x;
  image_extent.y = image_extent_u.y;

  world_from_view = Transform3D::translate(as_vec3(position));
}

Ray OrthographicCamera::camera_ray(Point2f const &sample_position) const
{

  auto const aspect_ratio = image_extent.x / image_extent.y;
  auto const ndc_from_screen =
      Transform2D::translate({ -1, 1 / aspect_ratio })
      * Transform2D::scale({ 2.f, -2.f / aspect_ratio })
      * Transform2D::scale({ 1 / image_extent.x, 1 / image_extent.y });
  auto const ndc_point = ndc_from_screen * sample_position;
  auto const ortho = Transform3D::orthographic(
      -1, 1, -1, 1, -0.1, -100);
  auto world_from_ndc = world_from_view * ortho.inverse();

  auto const world_point = world_from_ndc * extend(ndc_point, 0.f);

  auto const dir = world_from_view * Vec3f(0, 0, -1);

  return Ray{ world_point, UVec3f::normalize(dir) };
}

PerspectiveCamera::PerspectiveCamera(Extent2u const &image_extent_u,
    Point3f position)
{
  image_extent.x = image_extent_u.x;
  image_extent.y = image_extent_u.y;

  world_from_view = Transform3D::translate(as_vec3(position));
}

Ray PerspectiveCamera::camera_ray(Point2f const &sample_position) const
{
  auto const aspect_ratio = image_extent.x / image_extent.y;
  auto const ndc_from_screen =
      Transform2D::translate({ -1, 1 / aspect_ratio })
      * Transform2D::scale({ 2.f, -2.f / aspect_ratio })
      * Transform2D::scale({ 1 / image_extent.x, 1 / image_extent.y });
  auto const ndc_point = ndc_from_screen * sample_position;
  auto const persp = ProjectiveTransform::perspective(
      45_deg, -0.1, -100);

  auto const dir_view_h = persp.inverse() * extend(ndc_point, 0.f);
  auto dir = world_from_view * as_vec3(dir_view_h);

  return Ray{ world_from_view * Point3f(0, 0, 0),
              UVec3f::normalize(dir) };
}