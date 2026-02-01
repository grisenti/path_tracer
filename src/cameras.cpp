#include "cameras.hpp"

#include "gmath/vec.hpp"
#include "gmath/transform/transform2D.hpp"

ProjectiveCamera::ProjectiveCamera(Extent2u const &image_extent_u,
    Point3f position,
    ProjectiveTransform const &ndc_from_view)
{
  _image_extent.x = static_cast<Real>(image_extent_u.x);
  _image_extent.y = static_cast<Real>(image_extent_u.y);

  auto const aspect_ratio = _image_extent.x / _image_extent.y;

  auto const ndc_from_screen =
      Transform3D::translate({ -1, 1 / aspect_ratio, 0.f })
      * Transform3D::scale({ 2.f, -2.f / aspect_ratio, 1.f })
      * Transform3D::scale({ 1 / _image_extent.x, 1 / _image_extent.y, 1.f });

  _view_from_raster = ndc_from_view.inverse() * ndc_from_screen;

  _world_from_view = Transform3D::translate(as_vec3(position));
}

OrthographicCamera::OrthographicCamera(Extent2u const &image_extent,
    Point3f position) : ProjectiveCamera(image_extent, position,
    Transform3D::orthographic(
        -1.f, 1.f, -1.f, 1.f, -0.1f, -100.f))
{
}

Ray OrthographicCamera::camera_ray(Point2f const &sample_position) const
{

  auto const world_point = _world_from_view * _view_from_raster * extend(
                               sample_position, 0.f);
  auto const dir = _world_from_view * Vec3f(0, 0, -1);
  return Ray{ world_point, UVec3f::normalize(dir) };
}

PerspectiveCamera::PerspectiveCamera(Extent2u const &image_extent,
    Point3f position) : ProjectiveCamera(image_extent, position,
    ProjectiveTransform::perspective(
        45_deg, -0.1f, -100.f))
{
}

Ray PerspectiveCamera::camera_ray(Point2f const &sample_position) const
{
  // `to_vec` here is the same as `- Point3f(0, 0, 0)`
  auto const dir_view = as_vec3(
      _view_from_raster * extend(sample_position, 0.f));
  return Ray(_world_from_view * Point3f(0, 0, 0),
      UVec3f::normalize(_world_from_view * dir_view));
}