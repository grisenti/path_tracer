#pragma once

#include "common.hpp"

#include "core/ray.hpp"

class Properties;

class Camera
{
public:
  Camera() = default;

  // sample position in [0, image_width] x [0, image_height]
  virtual Ray camera_ray(Point2f const &sample_position) const = 0;

  virtual ~Camera() = default;
};

class ProjectiveCamera : public Camera
{
public:
  ProjectiveCamera(Properties const &props, Transform3D const &view_transform,
      uint32_t width, uint32_t height);

  Ray camera_ray(Point2f const &sample_position) const override = 0;

protected:
  Real _aspect_ratio;
  Transform3D _ndc_from_raster;
  Transform3D _world_from_view;
};

class OrthographicCamera : public ProjectiveCamera
{
public:
  OrthographicCamera(Properties const &props,
      Transform3D const &view_transform, uint32_t width, uint32_t height);

  Ray camera_ray(Point2f const &sample_position) const override;

private:
  Transform3D _view_from_raster;
};

class PerspectiveCamera : public ProjectiveCamera
{
public:
  PerspectiveCamera(Properties const &props, Transform3D const &view_transform,
      uint32_t width, uint32_t height);

  Ray camera_ray(Point2f const &sample_position) const override;

private:
  ProjectiveTransform _view_from_raster;
};