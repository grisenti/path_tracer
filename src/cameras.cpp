#include "cameras.hpp"

#include "gmath/vec.hpp"
#include "gmath/transform/transform2D.hpp"

#include "scene.hpp"

ProjectiveCamera::ProjectiveCamera(Properties const &props,
    Transform3D const &view_transform, uint32_t width, uint32_t height)
{

  auto const width_r = as<Real>(width);
  auto const height_r = as<Real>(height);

  _aspect_ratio = width_r / height_r;

  _ndc_from_raster = Transform3D::scale({ 1, -1, 1 })
                     * Transform3D::translate({ -1, -1, 0.f })
                     * Transform3D::scale({ 2.f, 2.f, 1.f })
                     * Transform3D::scale({ 1 / width_r, 1 / height_r, 1.f });

  _world_from_view = view_transform.inverse();
}

OrthographicCamera::OrthographicCamera(Properties const &props,
    Transform3D const &view_transform, uint32_t width, uint32_t height)
    : ProjectiveCamera(props, view_transform, width, height)
{
  auto const ndc_from_view = Transform3D::orthographic(
      -_aspect_ratio, _aspect_ratio, -1.f, 1.f, 0.1f, 100.f);
  _view_from_raster = ndc_from_view.inverse() * _ndc_from_raster;
}

Ray OrthographicCamera::camera_ray(Point2f const &sample_position) const
{

  auto const world_point
      = _world_from_view * _view_from_raster * extend(sample_position, 0.f);
  auto const dir = _world_from_view * Vec3f(0, 0, -1);
  return Ray{ world_point, UVec3f::normalize(dir) };
}

PerspectiveCamera::PerspectiveCamera(Properties const &props,
    Transform3D const &view_transform, uint32_t width, uint32_t height)
    : ProjectiveCamera(props, view_transform, width, height)
{
  auto const fovy = props.get_float("fovy", 45.f);
  auto const ndc_from_view = ProjectiveTransform::perspective(
      Degf(fovy), _aspect_ratio, 0.1f, 100.f);
  _view_from_raster = ndc_from_view.inverse() * _ndc_from_raster;
}

Ray PerspectiveCamera::camera_ray(Point2f const &sample_position) const
{
  // `as_vec` here is the same as `- Point3f(0, 0, 0)`
  auto const dir_view
      = as_vec3(_view_from_raster * extend(sample_position, 0.f));
  return Ray(_world_from_view * Point3f(0, 0, 0),
      UVec3f::normalize(_world_from_view * dir_view));
}