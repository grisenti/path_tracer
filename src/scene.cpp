#include "scene.hpp"

#include <toml++/toml.hpp>

#include "integrators.hpp"

toml::table parse_toml(std::string_view path)
{
  try
  {
    return toml::parse_file(path);
  }
  catch (toml::parse_error const &err)
  {
    PT_FATAL("failed to parse scene file {}.\nDetails: {}", path, err.what());
  }
}

Shape create_shape(toml::table const &table)
{
  auto type = table.get_as<std::string>("type");
  if (!type)
  {
    PT_FATAL("missing type specifier in entity.shape at {}",
        table.source().begin.line);
  }
  if (*type == "sphere")
  {
    return Shape::new_sphere(Properties(&table));
  }

  PT_FATAL("shape type {} has not yet been implemented", (std::string)*type);
}

Point3f Properties::get_point(std::string_view prop_name) const
{
  auto const list_node = _table->get(prop_name);
  if (!list_node)
  {
    PT_FATAL("missing property '{}' at line {}", prop_name,
        _table->source().begin.line);
  }
  auto const list = list_node->as_array();
  if (!list || list->size() != 3)
  {
    PT_FATAL("expected 3 component array at line {}",
        list_node->source().begin.line);
  }
  auto const p0 = list->get_as<double>(0);
  auto const p1 = list->get_as<double>(1);
  auto const p2 = list->get_as<double>(2);
  if (p0 && p1 && p2)
    return Point3f((double)*p0, (double)*p1, (double)*p2);
  PT_FATAL("expected float components for array at line {}",
      list->source().begin.line);
}

Point3f Properties::get_point(std::string_view prop_name,
    Point3f default_value) const
{
  PT_FATAL("unimplemented");
}

Real Properties::get_float(std::string_view prop_name) const
{
  auto const value = _table->get_as<double>(prop_name);
  if (value)
    return (double)*value;
  PT_FATAL("missing property '{}' at line {}", prop_name,
      _table->source().begin.line);
}

Real Properties::get_float(std::string_view prop_name,
    Real default_value) const
{
  auto const value = _table->get_as<double>(prop_name);
  if (value)
    return (double)*value;
  return default_value;
}

Scene Scene::read_from_file(std::string_view path)
{
  auto table = parse_toml(path);
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