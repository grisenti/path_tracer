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

class OrthographicCamera : public Camera
{
public:
  OrthographicCamera(Extent2u const &image_extent, Point3f position);

  Ray camera_ray(Point2f const &sample_position) const override;

private:
  Extent2f image_extent;
  Mat4f camera_from_raster;
  Transform3D world_from_view;
};

class PerspectiveCamera : public Camera
{
public:
  PerspectiveCamera(Extent2u const &image_extent, Point3f position);

  Ray camera_ray(Point2f const &sample_position) const override;

private:
  Extent2f image_extent;
  Transform2D film_to_camera;
  Transform3D world_from_view;
};