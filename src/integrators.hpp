#pragma once

#include "core/color.hpp"
#include "light_samplers.hpp"
#include "sampler.hpp"

class Properties;

class Integrator
{
public:
  virtual Color3f Li(
      Scene const &scene, Sampler &sampler, Ray const &ray) const = 0;

  virtual ~Integrator() = default;
};

class NormalsIntegrator : public Integrator
{
public:
  Color3f Li(Scene const &scene, Sampler &, Ray const &ray) const override;
};

class RandomWalkIntegrator : public Integrator
{
public:
  RandomWalkIntegrator(Properties const &props);

  Color3f Li(
      Scene const &scene, Sampler &sampler, Ray const &ray) const override
  {
    auto const c = LiRec(scene, sampler, ray, 0);
    auto const min = *std::ranges::min_element(c);
    PT_ASSERT(min >= 0, "value is {}", min);
    return c;
  }

private:
  Color3f LiRec(Scene const &scene, Sampler &sampler, Ray const &ray,
      uint32_t depth) const;

  int64_t _max_depth;
};

class SimplePathIntegrator : public Integrator
{
public:
  SimplePathIntegrator(
      Properties const &props, std::unique_ptr<LightSampler> light_sampler);

  Color3f Li(
      Scene const &scene, Sampler &sampler, Ray const &ray) const override;

private:
  std::unique_ptr<LightSampler> _light_sampler;
  int64_t _max_depth;
  bool _sample_lights;
  bool _sample_bsdf;
};

class PathIntegrator : public Integrator
{
public:
  PathIntegrator(
      Properties const &props, std::unique_ptr<LightSampler> light_sampler);

  Color3f Li(
      Scene const &scene, Sampler &sampler, Ray const &ray) const override;

private:
  std::unique_ptr<LightSampler> _light_sampler;
  int64_t _max_depth;
};