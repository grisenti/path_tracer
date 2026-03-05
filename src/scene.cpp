#include "scene.hpp"

#include <algorithm>
#include <toml++/toml.hpp>

#include "integrators.hpp"

namespace
{

toml::table parse_file(std::string_view path)
{
  try
  {
    return toml::parse_file(path);
  }
  catch (toml::parse_error const &err)
  {
    throw SceneParseException("failed to parse scene file {}.\nDetails: {}",
        path, err.what());
  }
}

Shape create_shape(toml::table const &table)
{
  auto type = table.get_as<std::string>("type");
  if (!type)
  {
    throw SceneParseException("missing type specifier in entity.shape at {}",
        table.source().begin.line);
  }
  if (*type == "sphere")
  {
    return Shape::new_sphere(Properties(&table));
  }

  throw SceneParseException("shape type {} has not yet been implemented",
      (std::string)*type);
}

// works with both double nodes and integer nodes to remove some of the
// friction when writing scene files manually
Real node_to_float(toml::node const &n)
{
  return n.visit([&]<typename V>(V &v) -> Real
  {
    if constexpr (toml::is_floating_point<V>)
    {
      return static_cast<float>(static_cast<double>(v));
    }
    if constexpr (toml::is_integer<V>)
    {
      return static_cast<float>(static_cast<int64_t>(v));
    }
    throw SceneParseException("expected number at line {}",
        n.source().begin.line);
  });
}

template <size_t N>
VecN<N, Real> node_to_number_vec(toml::node const &list_node)
{
  auto const list_ptr = list_node.as_array();
  if (!list_ptr || list_ptr->size() != N)
  {
    throw SceneParseException("expected {} component array at line {}", N,
        list_node.source().begin.line);
  }
  auto const &list = *list_ptr;
  VecN<N, Real> result;
  for (size_t i = 0; i < N; ++i)
  {
    result[i] = node_to_float(list[i]);
  }
  return result;
}

}


void Properties::specify_valid_properties(
    std::initializer_list<std::string_view> valid_properties) const
{
  for (auto const &prop : *_table)
  {
    // we always expect 'type', and it's not relevant to the user
    if (prop.first.str() == "type")
      continue;
    if (!std::ranges::contains(valid_properties, prop.first.str()))
    {
      // TODO: probably replace with proper logging
      std::println(
          "WARNING: table starting at line {} contains unused property '{}'",
          _table->source().begin.line, prop.first.str());
    }
  }

}

Point3f Properties::get_point(std::string_view prop_name) const
{
  if (auto const node = _table->get(prop_name))
    return as_point3(node_to_number_vec<3>(*node));
  throw SceneParseException("missing property '{}' at line {}", prop_name,
      _table->source().begin.line);
}

Point3f Properties::get_point(std::string_view prop_name,
    Point3f default_value) const
{
  if (auto const node = _table->get(prop_name))
    return as_point3(node_to_number_vec<3>(*node));
  return default_value;
}

Real Properties::get_float(std::string_view prop_name) const
{
  if (auto const node = _table->get(prop_name))
    return node_to_float(*node);
  PT_FATAL("missing property '{}' at line {}", prop_name,
      _table->source().begin.line);
}

Real Properties::get_float(std::string_view prop_name,
    Real default_value) const
{
  if (auto const node = _table->get(prop_name))
    return node_to_float(*node);
  return default_value;
}

Scene Scene::read_from_file(std::string_view path)
{
  auto table = parse_file(path);
  auto entities = table["entity"].as_array();
  std::vector<Primitive> primitives;
  for (auto const &entity : *entities)
  {
    auto const &entity_table = *entity.as_table();
    auto shape = create_shape(*entity_table["shape"].as_table());
    primitives.
        push_back(Primitive::new_basic_primitive(BasicPrimitive(shape)));
  }
  Scene result;
  result.top_level_primitive = Primitive::new_list_aggregate(
      ListAggregate(std::move(primitives)));
  result.integrator = std::make_unique<NormalIntegrator>();
  return result;
}