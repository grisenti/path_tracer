#pragma once

#include "color.hpp"
#include "common.hpp"
#include "handle.hpp"

class Diffuse;
class Properties;

class BSDF
{
public:
  static BSDF new_invalid_bsdf();
  static BSDF new_diffuse_bsdf(Diffuse bsdf);

  Color3f eval() const;

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

  Color3f eval() const
  {
    return _albedo * INV_PI;
  }

private:
  Color3f _albedo;
};

inline Color3f BSDF::eval() const
{
  switch (_handle.type())
  {
  case BSDFType::Diffuse:
    return all_diffuse_bsdfs[_handle.index()].eval();
  case BSDFType::End:
    PT_FATAL("called eval on invalid bsdf");
  default:
    PT_FATAL("bsdf has not yet been implemented");
  }
}