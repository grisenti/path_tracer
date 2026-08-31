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
  if (sample.y >= 0._r && s_len >= 1._r - EPSILON && s_len <= 1._r + EPSILON)
    return 1._r / (2._r * PI);
  return 0._r;
}

inline Point2f square_to_uniform_disk(const Point2f &sample)
{
  auto const r = std::sqrt(sample.x);
  auto const theta = 2._r * PI * sample.y;

  return Point2f(r * std::cos(theta), r * std::sin(theta));
}

inline Real square_to_uniform_disk_pdf(const Point2f &p)
{
  auto const len = square_magnitude(as_vec2(p));
  if (len > 1._r)
    return 0._r;
  return 1._r / PI;
}

inline UVec3f square_to_cosine_hemisphere(Point2f const &sample)
{
  auto const p = square_to_uniform_disk(sample);
  return UVec3f::create_unchecked(
      p.x, std::sqrt(1._r - pow2(p.x) - pow2(p.y)), p.y);
}

inline Real square_to_cosine_hemisphere_pdf(Vec3f const &v)
{
  auto const s_len = square_magnitude(v);
  if (v.y >= 0._r && s_len >= 1._r - EPSILON && s_len <= 1._r + EPSILON)
    return v.y / PI;
  return 0._r;
}