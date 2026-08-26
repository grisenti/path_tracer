#include "integrators.hpp"

#include "warp.hpp"
#include "scene/properties.hpp"
#include "scene.hpp"

Color3f NormalsIntegrator::Li(
    Scene const &scene, Sampler &, Ray const &ray) const
{
  if (auto const its = scene.intersect(ray))
    return as_color3(its->shape_intersection.sh_frame.normal() * 0.5f)
           + Color3f(0.5);
  return { 0 };
}

RandomWalkIntegrator::RandomWalkIntegrator(Properties const &props)
{
  _max_depth = props.get_int("max_depth", 5);
  PT_ASSERT(_max_depth > 0);
}

Color3f RandomWalkIntegrator::LiRec(Scene const &scene, Sampler &sampler,
    Ray const &ray, uint32_t const depth) const
{
  auto const its = scene.intersect(ray);
  if (!its)
    return { 0.0f };
  auto const &shape_its = its->shape_intersection;

  auto Le = Color3f(0.0f);
  if (its->emitter.is_valid())
  {
    Le = its->emitter.eval(
        { ray.o, shape_its.p, shape_its.sh_frame.normal() });
  }

  if (depth == _max_depth)
    return Le;

  auto const wi = shape_its.sh_frame.to_local(-ray.d);
  auto const wo = square_to_uniform_hemisphere(sampler.next_2D());
  auto const brdf = its->bsdf.eval({ wi, wo, {} });
  auto const bounce_ray
      = Ray(shape_its.p + shape_its.geo_frame.normal() * EPSILON,
          shape_its.sh_frame.to_world(wo));
  auto const pdf = square_to_uniform_hemisphere_pdf(wo);
  if (pdf == 0)
    return {};
  auto const cosine_term = CoordinateFrame::cos_theta(wo);
  auto const li = LiRec(scene, sampler, bounce_ray, depth + 1);
  return Le + componentwise_mul(brdf, li * cosine_term / pdf);
}

SimplePathIntegrator::SimplePathIntegrator(
    Properties const &props, std::unique_ptr<LightSampler> light_sampler)
{
  _max_depth = props.get_int("max_depth", 5);
  PT_ASSERT(_max_depth > 0);
  _light_sampler = std::move(light_sampler);
  _sample_lights = props.get_bool("sample_lights", true);
  _sample_bsdf = props.get_bool("sample_bsdf", true);
}

Color3f SimplePathIntegrator::Li(
    Scene const &scene, Sampler &sampler, Ray const &starting_ray) const
{
  Color3f incoming_radiance(0.0f);
  Color3f throughput(1.0f);
  auto ray = starting_ray;
  uint32_t depth = 0;
  bool specular_bounce = true;
  while (!components_near_zero(throughput, 0._r))
  {
    if (depth++ == _max_depth)
      break;

    auto its = scene.intersect(ray);
    if (!its)
      break;
    auto &shape_its = its->shape_intersection;

    if (!_sample_lights || specular_bounce)
    {
      Color3f emitted_radiance(0.0f);
      if (its->emitter.is_valid())
      {
        emitted_radiance = its->emitter.eval(
            EmitterParams{ ray.o, shape_its.p, shape_its.sh_frame.normal() });
      }
      incoming_radiance += componentwise_mul(throughput, emitted_radiance);
    }

    auto const wi = shape_its.sh_frame.to_local(-ray.d);
    // TODO: uv
    auto brec = its->bsdf.sample(wi, sampler.next_2D(), {});
    if (!brec.has_value())
      break;
    if (brec->measure != Measure::Discrete)
    {
      if (auto sampled_light = _light_sampler->sample(sampler.next_1D()))
      {
        if (auto ls
            = sampled_light->emitter.sample(shape_its.p, sampler.next_2D()))
        {
          auto emitted_radiance = sampled_light->emitter.eval(ls->params);
          auto const wo
              = shape_its.sh_frame.to_local(ls->params.dir_to_intersection);
          Color3f const f = its->bsdf.eval(BSDFParams(wi, wo, {}))
                            * CoordinateFrame::cos_theta(wo);
          Ray const shadow_ray(
              shape_its.p + shape_its.geo_frame.normal() * EPSILON,
              ls->params.dir_to_intersection, EPSILON,
              ls->params.distance - 4.f * EPSILON);
          if (!all_equal_to(f, 0._r) && ls->pdf > 0
              && !scene.intersect(shadow_ray))
          {
            incoming_radiance += componentwise_mul(throughput,
                                     componentwise_mul(f, emitted_radiance))
                                 / (sampled_light->pmf * ls->pdf);
          }
        }
      }
    }

    if (_sample_bsdf)
    {
      throughput = componentwise_mul(brec->value, throughput);
      specular_bounce = !its->bsdf.is_diffuse();
      auto const dir_sign = std::copysign(1._r, brec->params.wo->y);
      ray = Ray(
          shape_its.p + dir_sign * shape_its.geo_frame.normal() * EPSILON,
          shape_its.sh_frame.to_world(brec->params.wo));
    }
    else
    {
      auto const wo = square_to_uniform_hemisphere(sampler.next_2D());
      auto const pdf = square_to_uniform_hemisphere_pdf(wo);
      auto const cosine_term = CoordinateFrame::cos_theta(wo);
      // TODO: uv
      auto const f = its->bsdf.eval(BSDFParams{
          wi, wo, { 0, 0 }
      });
      throughput = componentwise_mul(throughput, f * cosine_term / pdf);
      specular_bounce = false;
      ray = Ray(shape_its.p + shape_its.geo_frame.normal() * EPSILON,
          shape_its.sh_frame.to_world(wo));
    }
  }
  return incoming_radiance;
}