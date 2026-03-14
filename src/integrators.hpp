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
  Color3f Li(Scene const &scene, Sampler &, Ray const &ray) const override
  {
    if (auto const its = scene.intersect(ray))
      return as_color3(its->shape_intersection.n * 0.5f) + Color3f(0.5);
    return { 0 };
  }
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
      uint32_t const depth) const
  {
    auto const its = scene.intersect(ray);
    if (!its)
      return { 0.0f };
    auto const &shape_its = its->shape_intersection;

    auto Le = Color3f(0.0f);
    if (its->emitter.is_valid())
    {
      Le = its->emitter.eval(shape_its.n, -ray.d);
    }

    if (depth == m_max_depth)
      return Le;

    auto const wo = square_to_uniform_hemisphere(sampler.next_2D());
    auto const brdf = its->bsdf.eval();
    auto const bounce_ray = Ray(shape_its.p + shape_its.n * EPSILON,
        UVec3f::normalize(shape_its.world_from_local * wo));
    auto const pdf = square_to_uniform_hemisphere_pdf(wo);
    auto const cosine_term = wo.z;
    return Le
           + componentwise_mul(
               brdf, LiRec(scene, sampler, bounce_ray, depth + 1) * cosine_term
                         / pdf);
  }

  uint64_t m_max_depth;
};
