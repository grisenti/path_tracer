#include "bsdfs.hpp"

#include "coordinate_frame.hpp"
#include "scene/properties.hpp"
#include "warp.hpp"

std::vector<Diffuse> BSDF::all_diffuse_bsdfs;
std::vector<Dielectric> BSDF::all_dielectric_bsdfs;
std::vector<Mirror> BSDF::all_mirror_bsdfs;
std::vector<Microfacet> BSDF::all_microface_bsdfs;

BSDF BSDF::new_invalid_bsdf()
{
  return { 0, BSDFType::End };
}

BSDF BSDF::new_diffuse_bsdf(Diffuse bsdf)
{
  auto const idx = as<uint64_t>(all_diffuse_bsdfs.size());
  all_diffuse_bsdfs.push_back(bsdf);
  return { idx, BSDFType::Diffuse };
}

BSDF BSDF::new_dielectric_bsdf(Dielectric bsdf)
{
  auto const idx = as<uint64_t>(all_dielectric_bsdfs.size());
  all_dielectric_bsdfs.push_back(bsdf);
  return { idx, BSDFType::Dielectric };
}

BSDF BSDF::new_mirror_bsdf(Mirror bsdf)
{
  auto const idx = as<uint64_t>(all_mirror_bsdfs.size());
  all_mirror_bsdfs.push_back(bsdf);
  return { idx, BSDFType::Mirror };
}

BSDF BSDF::new_microfacet_bsdf(Microfacet bsdf)
{
  auto const idx = as<uint64_t>(all_microface_bsdfs.size());
  all_microface_bsdfs.push_back(bsdf);
  return { idx, BSDFType::Microfacet };
}

Diffuse::Diffuse(Properties const &props) : _albedo(props.get_color("albedo"))
{
}

std::optional<BSDFRecord> Diffuse::sample(
    UVec3f const &wi, Point2f const &sample, Point2f const &uv) const
{
  if (CoordinateFrame::cos_theta(wi) <= 0)
    return {};

  auto const wo = square_to_cosine_hemisphere(sample);
  if (CoordinateFrame::cos_theta(wo) <= 0)
    return {};

  auto const params = BSDFParams{ wi, wo, uv };
  auto const value
      = CoordinateFrame::cos_theta(wo) * eval(params) / pdf(params);

  return BSDFRecord{
    .params = params, .measure = Measure::SolidAngle, .value = value
  };
}

Color3f Diffuse::eval(BSDFParams const &params) const
{
  if (CoordinateFrame::cos_theta(params.wi) <= 0
      || CoordinateFrame::cos_theta(params.wo) <= 0)
    return Color3f(0.f);
  return _albedo * INV_PI;
}

Real Diffuse::pdf(BSDFParams const &params) const
{
  return square_to_cosine_hemisphere_pdf(params.wo);
}

Dielectric::Dielectric(Properties const &props)
{
  // use glass as the default
  _int_ior = props.get_float("int_ior", 1.5046_r);
  // use air as default
  _ext_ior = props.get_float("ext_ior", 1.000277_r);
}

namespace
{
// theory and implementation taken from PBRT
Real frenel(Real const cos_theta_i, Real eta)
{
  Real const sin2_theta_i = 1 - pow2(cos_theta_i);
  Real const sin2_theta_t = sin2_theta_i / pow2(eta);
  if (sin2_theta_t >= 1)
    return 1.f;
  Real const cos_theta_t = std::sqrt(1 - sin2_theta_t);
  Real const r_par
      = (eta * cos_theta_i - cos_theta_t) / (eta * cos_theta_i + cos_theta_t);
  Real const r_perp
      = (cos_theta_i - eta * cos_theta_t) / (cos_theta_i + eta * cos_theta_t);
  return (pow2(r_par) + pow2(r_perp)) / 2.f;
}

// theory and implementation taken from PBRT
std::optional<UVec3f> refract(UVec3f const &wi, UNormal3f const &n, Real eta)
{
  auto const cos_theta_i = dot(wi, n);
  Real const sin2_theta_i = std::max(0.f, 1.f - pow2(cos_theta_i));
  Real const sin2_theta_t = sin2_theta_i / pow2(eta);
  if (sin2_theta_t >= 1.f)
    return std::nullopt;

  Real const cos_theta_t = std::sqrt(1 - sin2_theta_t);
  return UVec3f::normalize(
      -wi / eta + as_vec3(n * (cos_theta_i / eta - cos_theta_t)));
}

UVec3f reflect(UVec3f const &v, UVec3f const &n)
{
  // in theory the resulting vector should be normalized,
  // not sure if that is also true in practice.
  // For now we'll just normalize it before returning
  // TODO: look into this
  return UVec3f::normalize(2.f * dot(n, v) * n - v);
}

Real G1(UVec3f const &wv, Vec3f const &wh, Real alpha)
{
  if (CoordinateFrame::cos_theta(wv) == 0._r)
    return 0._r;
  auto const c = dot(wv, wh) / CoordinateFrame::cos_theta(wv);
  if (c > 0._r)
  {
    auto const b = 1._r / (alpha * CoordinateFrame::tan_theta(wv));
    if (b < 1.6_r)
    {
      return (3.535_r * b + 2.181_r * pow2(b))
             / (1._r + 2.276_r * b + 2.577_r * pow2(b));
    }
    return 1._r;
  }
  return 0._r;
}

}

std::optional<BSDFRecord> Dielectric::sample(
    UVec3f const &wi, Point2f const &sample, Point2f const &) const
{
  auto cos_theta_i = CoordinateFrame::cos_theta(wi);
  auto eta = _int_ior / _ext_ior;
  auto n = UNormal3f::create_unchecked(0, 1, 0);
  if (cos_theta_i < 0)
  {
    eta = 1.f / eta;
    n = -n;
    cos_theta_i = -cos_theta_i;
  }
  auto const R = frenel(cos_theta_i, eta);
  if (sample.x < R)
  {
    // reflection
    // no need to normalize again since wi is already normalized
    auto const wo = UVec3f::create_unchecked(-wi->x, wi->y, -wi->z);
    auto const params = BSDFParams{ wi, wo, Point2f(0, 0) };
    return BSDFRecord{
      .params = params, .measure = Measure::Discrete, .value = Color3f(1)
    };
  }
  // refraction
  auto const wo = refract(wi, n, eta);
  if (!wo)
    return {};
  auto const params = BSDFParams{ wi, *wo, Point2f(0, 0) };
  return BSDFRecord{
    .params = params, .measure = Measure::Discrete, .value = Color3f(1)
  };
}

Color3f Dielectric::eval(BSDFParams const &) const
{
  return Color3f(0._r);
}

Real Dielectric::pdf(BSDFParams const &) const
{
  return 0._r;
}

Mirror::Mirror(Properties const &)
{
}

std::optional<BSDFRecord> Mirror::sample(
    UVec3f const &wi, Point2f const &, Point2f const &uv) const
{
  if (CoordinateFrame::cos_theta(wi) <= 0)
    return std::nullopt;

  // since wi is in the local coordinate frame (normal is (0, 1, 0)),
  // wo = -reflect(wi, Vec3f(0, 1, 0)). Also, since wi is normalized,
  // we don't need to normalize again
  auto const wo = UVec3f::create_unchecked(-wi->x, wi->y, -wi->z);
  auto const params = BSDFParams{ wi, wo, uv };
  return BSDFRecord{
    .params = params, .measure = Measure::Discrete, .value = Color3f(1._r)
  };
}

Color3f Mirror::eval(BSDFParams const &) const
{
  return Color3f(0._r);
}

Real Mirror::pdf(BSDFParams const &) const
{
  return 0._r;
}

Microfacet::Microfacet(Properties const &props)
{
  _alpha = props.get_float("alpha", 0.1_r);
  _int_ior = props.get_float("int_ior", 1.5046_r);
  _ext_ior = props.get_float("ext_ior", 1.000277_r);
  _kd = props.get_color("kd", splat<Color3f>(0.5));
  _ks = 1._r - *std::ranges::max_element(_kd);
}

std::optional<BSDFRecord> Microfacet::sample(
    UVec3f const &wi, Point2f const &sample, Point2f const &uv) const
{
  if (CoordinateFrame::cos_theta(wi) <= 0)
    return {};

  auto xi_1 = sample.x;
  auto xi_2 = sample.y;
  auto wo = -Y3f;
  if (xi_1 < _ks) // specular case
  {
    xi_1 /= _ks; // scale xi_1 so that its in [0, 1] instead of [0, m_ks)
    auto const n = square_to_beckmann({ xi_1, xi_2 }, _alpha);
    wo = reflect(wi, n);
  }
  else // diffuse case
  {
    // same as the specular case, we need to scale xi_1
    xi_1 = (xi_1 - _ks) / (1._r - _ks);
    wo = square_to_cosine_hemisphere({ xi_1, xi_2 });
  }

  BSDFParams const params{
    .wi = wi,
    .wo = wo,
    .uv = uv,
  };
  auto const pdf_ = pdf(params);
  if (pdf_ == 0)
    return std::nullopt;
  auto const value = eval(params) * CoordinateFrame::cos_theta(wo) / pdf_;
  return BSDFRecord{
    .params = params,
    .measure = Measure::SolidAngle,
    .value = value,
  };
}

Color3f Microfacet::eval(BSDFParams const &params) const
{
  if (CoordinateFrame::cos_theta(params.wi) < 0._r
      || CoordinateFrame::cos_theta(params.wo) < 0._r)
  {
    return Color3f(0._r);
  }

  auto const wh = UVec3f::normalize(params.wi + params.wo);
  auto const D = square_to_beckmann_pdf(wh, _alpha);
  auto const F = frenel(dot(wh, params.wi), _int_ior / _ext_ior);
  auto const G = G1(params.wi, wh, _alpha) * G1(params.wo, wh, _alpha);

  auto const ct = CoordinateFrame::cos_theta;
  auto const dielectric_component = Color3f(
      _ks * D * F * G / (4._r * ct(params.wi) * ct(params.wo) * ct(wh)));
  return _kd * INV_PI + dielectric_component;
}

Real Microfacet::pdf(BSDFParams const &params) const
{
  if (CoordinateFrame::cos_theta(params.wi) <= 0
      || CoordinateFrame::cos_theta(params.wo) <= 0)
  {
    return 0._r;
  }
  auto const wh = UVec3f::normalize(params.wi + params.wo);
  auto const D = square_to_beckmann_pdf(wh, _alpha);
  auto const Jh = 1._r / (4._r * dot(wh, params.wo));
  return _ks * D * Jh
         + (1 - _ks) * CoordinateFrame::cos_theta(params.wo) * INV_PI;
}