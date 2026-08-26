#include "emitters.hpp"

#include "scene/properties.hpp"

std::vector<AreaEmitter> Emitter::all_area_emitters;

Emitter Emitter::new_invalid_emitter()
{
  return { 0, EmitterType::End };
}

Emitter Emitter::new_area_emitter(AreaEmitter emitter)
{
  auto const idx = as<uint64_t>(all_area_emitters.size());
  all_area_emitters.push_back(emitter);
  return { idx, EmitterType::Area };
}

AreaEmitter::AreaEmitter(Properties const &props, Shape shape) : _shape(shape)
{
  _radiance = props.get_color("radiance");
}

std::optional<EmitterRecord> AreaEmitter::sample(
    Point3f const &pos, Point2f const &sample) const
{
  auto const sampled_position = _shape.sample_position(sample);

  auto const params
      = EmitterParams(pos, sampled_position.pos, sampled_position.normal);
  auto const cosine_term
      = dot(params.n_intersection, -params.dir_to_intersection);
  if (cosine_term <= 0)
    return {};
  auto const pdf = sampled_position.pdf * pow2(params.distance) / cosine_term;
  return EmitterRecord{ .params = params, .pdf = pdf, .w = 1.0 };
}

Real AreaEmitter::pdf(EmitterParams const &params) const
{
  auto const c = dot(params.n_intersection, -params.dir_to_intersection);
  if (c <= 0.f)
    return 0.f;
  return _shape.pdf() * pow2(params.distance) / c;
}

Color3f AreaEmitter::eval(EmitterParams const &params) const
{
  if (dot(params.n_intersection, -params.dir_to_intersection) <= 0.f)
    return Color3f(0.f);
  return _radiance;
}
