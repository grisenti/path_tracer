#pragma once

#include "color.hpp"
#include "scene.hpp"

class Integrator
{
public:
  ~Integrator() = default;

  virtual Color3f Li(Scene const &scene, Ray ray) = 0;
};

class NormalIntegrator : public Integrator
{
public:
  Color3f Li(Scene const &scene, Ray ray) override
  {
    auto const its = scene.intersect(ray);
    if (its.has_value())
      return as_color3(its->n * 0.5f) + Color3f(0.5);
    return color_from_web_hex(0x1F80FF);
  }
};