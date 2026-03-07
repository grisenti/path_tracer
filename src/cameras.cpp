#include "cameras.hpp"

#include "gmath/vec.hpp"
#include "gmath/transform/transform2D.hpp"

#include "scene.hpp"

ProjectiveCamera::ProjectiveCamera(Properties const &props)
{
  props.specify_valid_properties({ "width", "height", "position", "target",
                                   "up" });
  auto const width = props.get_float("width");
  auto const height = props.get_float("height");

  _aspect_ratio = width / height;

  _ndc_from_raster =
      Transform3D::scale({ 1, -1, 1 })
      * Transform3D::translate({ -1, -1, 0.f })
      * Transform3D::scale({ 2.f, 2.f, 1.f })
      * Transform3D::scale({ 1 / width, 1 / height, 1.f });

  auto const position = props.get_point3("position");
  auto const target = props.get_point3("target");
  auto const up = props.get_vec3("up");

  _world_from_view = Transform3D::look_at(position, target, up).inverse();
}

OrthographicCamera::OrthographicCamera(Properties const &props) :
  ProjectiveCamera(props)
{
  auto const ndc_from_view = Transform3D::orthographic(
      -1.f, 1.f, -1.f, 1.f, 0.1f,
      100.f);
  _view_from_raster = ndc_from_view.inverse() * _ndc_from_raster;
}

Ray OrthographicCamera::camera_ray(Point2f const &sample_position) const
{

  auto const world_point = _world_from_view * _view_from_raster * extend(
                               sample_position, 0.f);
  auto const dir = _world_from_view * Vec3f(0, 0, -1);
  return Ray{ world_point, UVec3f::normalize(dir) };
}

PerspectiveCamera::PerspectiveCamera(Properties const &props) :
  ProjectiveCamera(props)
{
  auto const fovy = props.get_float("fovy", 45.f);
  auto const ndc_from_view = ProjectiveTransform::perspective(
      Degf(fovy), _aspect_ratio, 0.1f, 100.f);
  _view_from_raster = ndc_from_view.inverse() * _ndc_from_raster;
}

Ray PerspectiveCamera::camera_ray(Point2f const &sample_position) const
{
  // `as_vec` here is the same as `- Point3f(0, 0, 0)`
  auto const dir_view = as_vec3(
      _view_from_raster * extend(sample_position, 0.f));
  return Ray(_world_from_view * Point3f(0, 0, 0),
      UVec3f::normalize(_world_from_view * dir_view));
}