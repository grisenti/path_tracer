#include "light_samplers.hpp"

#include "scene.hpp"

UniformLightSampler::UniformLightSampler(Scene const &scene)
{
  _scene_emitters = std::span(scene.lights);
}

std::optional<SampledLight> UniformLightSampler::sample(Real u)
{
  if (_scene_emitters.empty())
    return {};
  auto index = as<size_t>(u * as<Real>(_scene_emitters.size()));
  if (index == _scene_emitters.size())
    index -= 1;
  return SampledLight{ pmf(), _scene_emitters[index] };
}

Real UniformLightSampler::pmf()
{
  return 1._r / as<Real>(_scene_emitters.size());
}