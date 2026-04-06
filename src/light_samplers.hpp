#pragma once

#include "common.hpp"
#include "emitters.hpp"

struct SampledLight
{
  Real pmf;
  Emitter emitter;
};

class LightSampler
{
public:
  virtual std::optional<SampledLight> sample(Real u) = 0;
  virtual Real pmf() = 0;
};

class UniformLightSampler : public LightSampler
{
public:
  std::optional<SampledLight> sample(Real u) override;
  Real pmf() override;

private:
  std::span<Emitter> _scene_emitters;
};