#pragma once

#include "common.hpp"

#include <memory>
#include <random>

class Sampler
{
public:
  virtual ~Sampler() = default;
  virtual Real next_1D() = 0;
  virtual Point2f next_2D() = 0;
  virtual Point3f next_3D() = 0;
  virtual std::unique_ptr<Sampler> clone() const = 0;
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

  Point3f next_3D() override
  {
    return { next_1D(), next_1D(), next_1D() };
  }

  std::unique_ptr<Sampler> clone() const override
  {
    return std::make_unique<UniformSampler>(*this);
  }

private:
  std::default_random_engine _engine;
  std::uniform_real_distribution<Real> _distribution;
};