#pragma once
#include "common.hpp"
#include <gmath/transform/transform3D.hpp>
#include <gmath/transform/transform2D.hpp>

class Camera
{
public:
  Camera() = default;

  // sample position in [0, image_width] x [0, image_height]
  virtual Ray camera_ray(Point2f const &sample_position) const = 0;
};

class ProjectiveCamera : public Camera
{
public:
  ProjectiveCamera(Extent2u const &image_extent, Point3f position,
      ProjectiveTransform const &ndc_from_view);

  Ray camera_ray(Point2f const &sample_position) const override = 0;

protected:
  Extent2f _image_extent;
  ProjectiveTransform _view_from_raster;
  Transform3D _world_from_view;
};

class OrthographicCamera : public ProjectiveCamera
{
public:
  OrthographicCamera(Extent2u const &image_extent, Point3f position);

  Ray camera_ray(Point2f const &sample_position) const override;
};

class PerspectiveCamera : public ProjectiveCamera
{
public:
  PerspectiveCamera(Extent2u const &image_extent, Point3f position);

  Ray camera_ray(Point2f const &sample_position) const override;
};