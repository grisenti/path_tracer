#pragma once

#include "color.hpp"
#include "common.hpp"
#include "handle.hpp"

class Diffuse;
class Properties;

struct BSDFParams
{
  UVec3f wi;
  UVec3f wo;
  Point2f uv;
};

enum class Measure
{
  SolidAngle,
  Discrete,
  End,
};

struct BSDFRecord
{
  BSDFParams params;
  Measure measure;
  Color3f value;
};

class BSDF
{
public:
  static BSDF new_invalid_bsdf();
  static BSDF new_diffuse_bsdf(Diffuse bsdf);

  std::optional<BSDFRecord> sample(
      UVec3f const &wi, Point2f const &sample, Point2f const &uv) const;
  Color3f eval(BSDFParams const &params) const;
  Real pdf(BSDFParams const &params) const;
  bool is_diffuse() const;

private:
  static std::vector<Diffuse> all_diffuse_bsdfs;

  enum class BSDFType
  {
    Diffuse,
    End
  };

  BSDF(uint64_t index, BSDFType type) : _handle(index, type)
  {
  }

  Handle<BSDFType> _handle;
};

class Diffuse
{
public:
  explicit Diffuse(Properties const &props);

  std::optional<BSDFRecord> sample(
      UVec3f const &wi, Point2f const &sample, Point2f const &uv) const;

  Color3f eval(BSDFParams const &) const;

  Real pdf(BSDFParams const &params) const;

private:
  Color3f _albedo;
};

inline std::optional<BSDFRecord> BSDF::sample(
    UVec3f const &wi, Point2f const &sample, Point2f const &uv) const
{
  switch (_handle.type())
  {
  case BSDFType::Diffuse:
    return all_diffuse_bsdfs[_handle.index()].sample(wi, sample, uv);
  case BSDFType::End:
    PT_FATAL("called sample on invalid bsdf");
  default:
    PT_FATAL("bsdf has not yet been implemented");
  }
}

inline Color3f BSDF::eval(BSDFParams const &params) const
{
  switch (_handle.type())
  {
  case BSDFType::Diffuse:
    return all_diffuse_bsdfs[_handle.index()].eval(params);
  case BSDFType::End:
    PT_FATAL("called eval on invalid bsdf");
  default:
    PT_FATAL("bsdf has not yet been implemented");
  }
}

inline Real BSDF::pdf(BSDFParams const &params) const
{
  switch (_handle.type())
  {
  case BSDFType::Diffuse:
    return all_diffuse_bsdfs[_handle.index()].pdf(params);
  case BSDFType::End:
    PT_FATAL("called pdf on invalid bsdf");
  default:
    PT_FATAL("bsdf has not yet been implemented");
  }
}

inline bool BSDF::is_diffuse() const
{
  switch (_handle.type())
  {
  case BSDFType::Diffuse:
    return true;
  case BSDFType::End:
    PT_FATAL("called 'id_diffuse' on invalid bsdf");
  default:
    PT_FATAL("bsdf has not yet been implemented");
  }
}