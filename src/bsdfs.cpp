#include "bsdfs.hpp"

#include "scene.hpp"
#include "warp.hpp"

std::vector<Diffuse> BSDF::all_diffuse_bsdfs;

BSDF BSDF::new_invalid_bsdf()
{
  return { 0, BSDFType::End };
}

BSDF BSDF::new_diffuse_bsdf(Diffuse bsdf)
{
  auto const idx = static_cast<uint64_t>(all_diffuse_bsdfs.size());
  all_diffuse_bsdfs.push_back(bsdf);
  return { idx, BSDFType::Diffuse };
}

Diffuse::Diffuse(Properties const &props) : _albedo(props.get_color("albedo"))
{
}

std::optional<BSDFRecord> Diffuse::sample(
    UVec3f const &wi, Point2f const &sample, Point2f const &uv) const
{
  if (wi->y <= 0)
    return {};

  auto const wo = square_to_uniform_hemisphere(sample);
  if (wo->y <= 0)
    return {};

  auto const params = BSDFParams{ wi, wo, uv };
  auto const value = wo->y * eval(params) / pdf(params);

  return BSDFRecord{
    .params = params, .measure = Measure::Discrete, .value = value
  };
}

Color3f Diffuse::eval(BSDFParams const &params) const
{
  if (params.wi->y <= 0 || params.wo->y <= 0)
    return Color3f(0.f);
  return _albedo * INV_PI;
}

Real Diffuse::pdf(BSDFParams const &params) const
{
  return square_to_uniform_hemisphere_pdf(params.wo);
}