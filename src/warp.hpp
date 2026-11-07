#pragma once

#include <cmath>

#include "common.hpp"

inline UVec3f square_to_uniform_sphere(Point2f const &sample)
{
  auto const cos_theta = 1._r - sample.y * 2._r;
  auto const sin_theta = std::sqrt(std::max(0._r, 1._r - pow2(cos_theta)));
  auto const sin_phi = sin(2 * PI * sample.x);
  auto const cos_phi = cos(2 * PI * sample.x);

  return UVec3f::normalize(
      cos_phi * sin_theta, sin_phi * sin_theta, cos_theta);
}

inline Real square_to_uniform_sphere_pdf(Vec3f const &sample)
{
  auto const s_len = square_length(sample);
  if (s_len >= 1._r - EPSILON && s_len <= 1._r + EPSILON)
    return 1._r / (4._r * PI);
  return 0._r;
}

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

inline UVec3f square_to_beckmann(Point2f const &sample, Real alpha)
{
  auto const phi = 2 * PI * sample.x;
  auto const theta = atan(alpha * sqrt(log(1._r / (1._r - sample.y))));
  // the vector is already normalized, we normalize again here for
  return UVec3f::normalize(
      cos(phi) * sin(theta), cos(theta), sin(phi) * sin(theta));
}

inline Real square_to_beckmann_pdf(Vec3f const &m, Real alpha)
{
  auto const s_len = square_length(m);
  if (m.y >= 0 && s_len < 1.f + EPSILON && s_len > 1.f - EPSILON)
  {
    auto const theta = acos(m.y);
    auto const a2 = pow2(alpha);
    return std::exp(-pow2(tan(theta)) / a2)
           / (PI * a2 * pow(cos(theta), 3._r));
  }
  return 0.f;
}