#include "bsdfs.hpp"

#include "scene.hpp"

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