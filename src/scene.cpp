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

std::string get_type(toml::table const &table)
{
  auto type = table.get_as<std::string>("type");
  if (!type)
  {
    throw SceneParseException("missing type specifier for table at line {}",
        table.source().begin.line);
  }
  return type->get();
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
    else if constexpr (toml::is_integer<V>)
    {
      return static_cast<float>(static_cast<int64_t>(v));
    }
    throw SceneParseException("expected number at line {}",
        n.source().begin.line);
  });
}

toml::array const &node_to_array(toml::node const &list_node)
{
  auto const list_ptr = list_node.as_array();
  if (!list_ptr)
  {
    throw SceneParseException("expected array at line {}",
        list_node.source().begin.line);
  }
  return *list_ptr;
}

std::vector<uint32_t> node_to_indices(toml::node const &list_node)
{
  auto const &list = node_to_array(list_node);
  std::vector<uint32_t> result;
  result.reserve(list.size());
  std::ranges::transform(list, std::back_insert_iterator(result),
      [](toml::node const &node) -> uint32_t
      {
        if (auto const value_ptr = node.as<int64_t>())
          return static_cast<int32_t>(static_cast<int64_t>(*value_ptr));
        throw SceneParseException(
            "index array should only contain integers (line: {})",
            node.source().begin.line);
      });
  return result;
}

template <ModifiableArray A> requires std::is_same_v<ComponentT<A>, Real>
std::vector<A> node_to_list_of_arrays(toml::node const &list_node)
{
  auto const &list = node_to_array(list_node);
  if (list.size() % A::SIZE != 0)
  {
    throw SceneParseException("array size is not divisible by {} at line {}",
        A::SIZE, list_node.source().begin.line);
  }

  std::vector<A> result;
  for (size_t i = 0; i < list.size(); i += A::SIZE)
  {
    A arr;
    for (size_t j = 0; j < A::SIZE; ++j)
      arr[j] = node_to_float(list[i + j]);
    result.push_back(arr);
  }
  return result;
}

std::vector<Shape> create_shapes(toml::table const &table)
{
  auto type = get_type(table);
  if (type == "sphere")
  {
    return { Shape::new_sphere(Properties(&table)) };
  }
  if (type == "triangle_mesh")
  {
    auto const &indices_node = table["indices"];
    if (!indices_node)
    {
      throw SceneParseException(
          "missing indices field for triangle mesh at line {}",
          table.source().begin.line);
    }
    auto indices = node_to_indices(*indices_node.node());

    auto const positions_node = table["positions"];
    if (!positions_node)
    {
      throw SceneParseException(
          "missing positions field for triangle mesh at line {}",
          table.source().begin.line);
    }
    auto positions = node_to_list_of_arrays<Point3f>(
        *positions_node.node());

    if (positions.size() < 3)
    {
      throw SceneParseException(
          "positions field of triangle mesh should have at least 3 elements (line {})",
          table.source().begin.line);
    }

    TriangleMesh mesh(std::move(indices), std::move(positions));
    return Shape::new_triangles(std::move(mesh));
  }
  throw SceneParseException("shape type {} has not yet been implemented",
      type);
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

void process_primitive(std::vector<Primitive> &primitives,
    toml::array::value_type const &prim)
{
  auto const prim_table_ptr = prim.as_table();
  if (!prim_table_ptr)
  {
    throw SceneParseException("expected table at line {}",
        prim.source().begin.line);
  }
  auto const &prim_table = *prim.as_table();
  if (auto const prim_type = get_type(prim_table); prim_type == "object")
  {
    const auto shape_ptr = prim_table["shape"].as_table();
    if (!shape_ptr)
    {
      throw SceneParseException(
          "expected shape in primitive starting at line {}",
          prim.source().begin.line);
    }
    auto const shapes = create_shapes(*shape_ptr);
    std::ranges::transform(shapes, std::back_insert_iterator(primitives),
        [](Shape const s)
        {
          return Primitive::new_basic_primitive(BasicPrimitive(s));
        });
  }
}

Scene Scene::read_from_file(std::string_view path)
{
  auto table = parse_file(path);
  auto primitives_config = table["primitive"].as_array();
  std::vector<Primitive> primitives;
  for (auto const &prim : *primitives_config)
  {
    process_primitive(primitives, prim);
  }
  Scene result;
  result.top_level_primitive = Primitive::new_list_aggregate(
      ListAggregate(std::move(primitives)));
  result.integrator = std::make_unique<NormalIntegrator>();
  return result;
}