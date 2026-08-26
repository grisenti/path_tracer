#pragma once

#include <vector>

#include "common.hpp"
#include "core/color.hpp"
#include "core/handle.hpp"
#include "shapes.hpp"

class Properties;
class AreaEmitter;

struct EmitterParams
{
  Point3f p_intersection = Point3f(0, 0, 0);
  UNormal3f n_intersection = UNormal3f::normalize(1, 0, 0);
  UVec3f dir_to_intersection = X3f;
  Real distance = 0.f;

  EmitterParams() = default;

  EmitterParams(
      Point3f const &p_start, Point3f const &p_int, UNormal3f const &n_int)
      : p_intersection(p_int), n_intersection(n_int)
  {
    auto const dir = p_intersection - p_start;
    distance = length(dir);
    dir_to_intersection = UVec3f::create_unchecked(dir / distance);
  }
};

struct EmitterRecord
{
  EmitterParams params;
  Real pdf;
  Color3f w;
};

class Emitter
{
public:
  static Emitter new_invalid_emitter();
  static Emitter new_area_emitter(AreaEmitter emitter);

  std::optional<EmitterRecord> sample(
      Point3f const &pos, Point2f const &sample) const;

  Real pdf(EmitterParams const &params) const;

  Color3f eval(EmitterParams const &params) const;

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
  AreaEmitter(Properties const &props, Shape shape);

  std::optional<EmitterRecord> sample(
      Point3f const &pos, Point2f const &sample) const;

  Real pdf(EmitterParams const &params) const;

  Color3f eval(EmitterParams const &params) const;

private:
  Shape _shape;
  Color3f _radiance;
};

inline std::optional<EmitterRecord> Emitter::sample(
    Point3f const &pos, Point2f const &sample) const
{
  switch (_handle.type())
  {
  case EmitterType::Area:
    return all_area_emitters[_handle.index()].sample(pos, sample);
  case EmitterType::End:
    PT_FATAL("called eval on an invalid emitter");
  default:
    PT_FATAL("emitter type has yet to be implemented");
  }
}

inline Color3f Emitter::eval(EmitterParams const &params) const
{
  switch (_handle.type())
  {
  case EmitterType::Area:
    return all_area_emitters[_handle.index()].eval(params);
  case EmitterType::End:
    PT_FATAL("called eval on an invalid emitter");
  default:
    PT_FATAL("emitter type has yet to be implemented");
  }
}
