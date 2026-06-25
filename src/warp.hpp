#pragma once

#include <cmath>

#include "common.hpp"

inline UVec3f square_to_uniform_hemisphere(Point2f const &sample)
{
  auto const cos_theta = sample.y;
  auto const sin_theta = std::sqrt(std::max(0._r, 1 - pow2(cos_theta)));

  auto const sin_phi = std::sin(2 * PI * sample.x);
  auto const cos_phi = std::cos(2 * PI * sample.x);

  return UVec3f::create_unchecked(
      cos_phi * sin_theta, cos_theta, sin_phi * sin_theta);
}

inline Real square_to_uniform_hemisphere_pdf(Vec3f const &sample)
{
  auto const s_len = square_length(sample);
  if (sample.y >= 0.f && s_len >= 1.f - EPSILON && s_len <= 1.f + EPSILON)
    return 1.f / (2.f * PI);
  return 0.f;
}
