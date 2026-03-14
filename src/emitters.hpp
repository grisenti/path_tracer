#pragma once

#include <vector>

#include "common.hpp"
#include "color.hpp"
#include "handle.hpp"

class Properties;
class AreaEmitter;

class Emitter
{
public:
  static Emitter new_invalid_emitter();
  static Emitter new_area_emitter(AreaEmitter emitter);

  Color3f eval(UNormal3f const &n, UVec3f const &w) const;

  bool is_valid() const
  {
    return _handle.type() != EmitterType::End;
  }

private:
  static std::vector<AreaEmitter> all_area_emitters;

  enum class EmitterType
  {
    Area,
    End,
  };

  Emitter(uint64_t index, EmitterType type) : _handle(index, type)
  {
  }

  Handle<EmitterType> _handle;
};

class AreaEmitter
{
public:
  AreaEmitter(Properties const &props);

  Color3f eval(UNormal3f const &n, UVec3f const &w) const
  {
    if (dot(n, -w) <= 0.f)
      return Color3f(0.f);
    return _radiance;
  }

private:
  Color3f _radiance;
};

inline Color3f Emitter::eval(UNormal3f const &n, UVec3f const &w) const
{
  switch (_handle.type())
  {
  case EmitterType::Area:
    return all_area_emitters[_handle.index()].eval(n, w);
  case EmitterType::End:
    PT_FATAL("called eval on an invalid emitter");
  default:
    PT_FATAL("emitter type has yet to be implemented");
  }
}
