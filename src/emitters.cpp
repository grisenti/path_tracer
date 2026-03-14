#include "emitters.hpp"

#include "scene.hpp"

std::vector<AreaEmitter> Emitter::all_area_emitters;

Emitter Emitter::new_invalid_emitter()
{
  return { 0, EmitterType::End };
}

Emitter Emitter::new_area_emitter(AreaEmitter emitter)
{
  auto const idx = static_cast<uint64_t>(all_area_emitters.size());
  all_area_emitters.push_back(emitter);
  return { idx, EmitterType::Area };
}

AreaEmitter::AreaEmitter(Properties const &props)
{
  _radiance = props.get_color("radiance");
}
