#pragma once

#include "color.hpp"
#include "sampler.hpp"
#include "scene.hpp"
#include "warp.hpp"

class Integrator
{
public:
  ~Integrator() = default;

  virtual Color3f Li(
      Scene const &scene, Sampler &sampler, Ray const &ray) const = 0;
};

class NormalIntegrator : public Integrator
{
public:
  Color3f Li(Scene const &scene, Sampler &, Ray const &ray) const override;
};

class RandomWalkIntegrator : public Integrator
{
public:
  RandomWalkIntegrator(Properties const &props)
  {
    m_max_depth = props.get_int("max_depth", 5);
  }

  Color3f Li(
      Scene const &scene, Sampler &sampler, Ray const &ray) const override
  {
    return LiRec(scene, sampler, ray, 0);
  }

private:
  Color3f LiRec(Scene const &scene, Sampler &sampler, Ray const &ray,
      uint32_t depth) const;

  uint64_t m_max_depth;
};
