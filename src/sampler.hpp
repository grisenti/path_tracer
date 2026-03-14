#pragma once

#include "common.hpp"

#include <random>

class Sampler
{
public:
  virtual Real next_1D() = 0;
  virtual Point2f next_2D() = 0;
};

class UniformSampler : public Sampler
{
public:
  UniformSampler() = default;

  Real next_1D() override
  {
    return _distribution(_engine);
  }

  Point2f next_2D() override
  {
    return { next_1D(), next_1D() };
  }

private:
  std::default_random_engine _engine;
  std::uniform_real_distribution<Real> _distribution;
};