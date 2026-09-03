#pragma once

#include "common.hpp"

#include <memory>
#include <random>

class Sampler
{
public:
  virtual ~Sampler() = default;
  virtual void start_pixel_samples(uint64_t sample_index) = 0;
  virtual Real next_1D() = 0;
  virtual Point2f next_2D() = 0;
  virtual Point3f next_3D() = 0;
  virtual std::unique_ptr<Sampler> clone() const = 0;
};

class UniformSampler : public Sampler
{
public:
  UniformSampler() = default;

  void start_pixel_samples(uint64_t sample_index) override
  {
    _engine.seed(sample_index);
  }

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
  std::mt19937_64 _engine;
  std::uniform_real_distribution<Real> _distribution;
};