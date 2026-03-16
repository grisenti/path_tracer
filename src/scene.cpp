#include "scene.hpp"

#include <algorithm>
#include <toml++/toml.hpp>
#include <fast_obj.h>

#include "cameras.hpp"
#include "integrators.hpp"
#include "utils.hpp"

namespace
{

toml::table parse_toml_file(std::string_view path)
{
  try
  {
    return toml::parse_file(path);
  }
  catch (toml::parse_error const &err)
  {
    throw SceneParseException(
        "failed to parse scene file {}.\nDetails: {}", path, err.what());
  }
}

template <typename T>
T const &get_as(toml::table const &table, std::string_view node_name)
{
  if (auto const val_ptr = table.get_as<T>(node_name))
  {
    if constexpr (std::derived_from<T, toml::node>)
      return *val_ptr;
    else
      return val_ptr->get();
  }
  throw SceneParseException(
      "attribute {} is missing or incorrect (table at line {})", node_name,
      table.source().begin.line);
}

template <typename T>
std::optional<T const *> get_as_opt(
    toml::table const &table, std::string_view node_name)
{
  auto const node_ptr = table.get(node_name);
  if (!node_ptr)
    return std::nullopt;
  if (auto const val_ptr = node_ptr->as<T>())
  {
    if constexpr (std::derived_from<T, toml::node>)
      return val_ptr;
    else
      return val_ptr->get();
  }
  throw SceneParseException(
      "attribute {} has incorrect type (table at line {})", node_name,
      table.source().begin.line);
}

std::string_view get_type(toml::table const &table)
{
  return get_as<std::string>(table, "type");
}

// works with both double nodes and integer nodes to remove some of the
// friction when writing scene files manually
Real to_float(toml::node const &n)
{
  return n.visit(
      [&]<typename V>(V &v) -> Real
      {
        if constexpr (toml::is_floating_point<V>)
        {
          return static_cast<float>(static_cast<double>(v));
        }
        else if constexpr (toml::is_integer<V>)
        {
          return static_cast<float>(static_cast<int64_t>(v));
        }
        else
        {
          throw SceneParseException(
              "expected number at line {}", n.source().begin.line);
        }
      });
}

template <ModifiableArray A>
  requires(std::is_same_v<ComponentT<A>, Real>)
A get_float_array(toml::table const &table, std::string_view attribute_name)
{
  auto const &list = get_as<toml::array>(table, attribute_name);
  if (list.size() != A::SIZE)
  {
    throw SceneParseException("expected {} component array at line {}",
        A::SIZE, list.source().begin.line);
  }
  A result;
  for (size_t i = 0; i < A::SIZE; ++i)
    result[i] = to_float(list[i]);
  return result;
}

template <ModifiableArray A>
  requires(std::is_same_v<ComponentT<A>, Real>)
A get_float_array_or_default(toml::table const &table,
    std::string_view attribute_name, A const &default_value)
{
  if (!table.contains(attribute_name))
    return default_value;
  return get_float_array<A>(table, attribute_name);
}

std::optional<Transform3D> read_transform3d_opt(
    toml::table const &table, std::string_view attribute_name)
{
  // TODO: maker this more efficient, too many calls that search for the
  // attribute in the table
  if (!table.contains(attribute_name))
    return std::nullopt;

  // supports both 4x3 array or table with more info
  if (table[attribute_name].is_array())
  {
    auto const values
        = get_float_array<VecN<3 * 4, Real>>(table, attribute_name);
    // clang-format off
    auto const matrix = Mat3f::from_rows({
      values[0], values[1], values[2],
      values[4], values[5], values[6],
      values[8], values[9], values[10],
    });
    // clang-format on
    auto const translation = Vec3f(values[3], values[7], values[11]);
    return Transform3D{ matrix, translation };
  }

  auto const transform_table = get_as<toml::table>(table, attribute_name);
  auto const type = get_type(transform_table);
  Transform3D result;
  if (type == "affine")
  {
    // TODO: should probably clean up this duplicate code
    auto const values
        = get_float_array<VecN<3 * 4, Real>>(transform_table, "matrix");
    // clang-format off
    result.matrix = Mat3f::from_rows({
      values[0], values[1], values[2],
      values[4], values[5], values[6],
      values[8], values[9], values[10],
    });
    // clang-format on
    result.translation = Vec3f(values[3], values[7], values[11]);
  }
  else if (type == "look_at")
  {
    auto const position
        = get_float_array<Point3f>(transform_table, "position");
    auto const target = get_float_array<Point3f>(transform_table, "target");
    auto const up = get_float_array<Vec3f>(transform_table, "up");
    result = Transform3D::look_at(position, target, up);
  }
  else
  {
    throw SceneParseException(
        "transform type {} has not yet been implemented (line {})", type,
        table.source().begin.line);
  }
  auto const inverse_ptr = transform_table.get_as<bool>("inverse");
  if (inverse_ptr && *inverse_ptr)
    return result.inverse();
  return result;
}

std::vector<uint32_t> read_indices(toml::array const &list)
{
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

template <ModifiableArray A>
  requires std::is_same_v<ComponentT<A>, Real>
std::vector<A> read_list_of_arrays(toml::array const &list)
{
  if (list.size() % A::SIZE != 0)
  {
    throw SceneParseException("array size is not divisible by {} at line {}",
        A::SIZE, list.source().begin.line);
  }

  std::vector<A> result;
  for (size_t i = 0; i < list.size(); i += A::SIZE)
  {
    A arr;
    for (size_t j = 0; j < A::SIZE; ++j)
      arr[j] = to_float(list[i + j]);
    result.push_back(arr);
  }
  return result;
}

std::vector<Shape> read_triangle_mesh(toml::table const &table)
{
  auto indices = read_indices(get_as<toml::array>(table, "indices"));
  auto positions
      = read_list_of_arrays<Point3f>(get_as<toml::array>(table, "positions"));
  if (positions.size() < 3)
  {
    throw SceneParseException("positions field of triangle mesh should have "
                              "at least 3 elements (line {})",
        table.source().begin.line);
  }

  if (auto const transform = read_transform3d_opt(table, "to_world"))
  {
    for (auto &p : positions)
      p = *transform * p;
  }

  TriangleMesh mesh(std::move(indices), std::move(positions));
  return Shape::new_triangles(std::move(mesh));
}

std::vector<Shape> read_obj_mesh(const toml::table &table)
{
  auto const &filename = get_as<std::string>(table, "filename");
  auto const transform = read_transform3d_opt(table, "to_world");

  auto const mesh = fast_obj_read(filename.c_str());

  std::vector<Point3f> positions;
  positions.reserve((mesh->position_count - 1) * 3);
  for (size_t i = 1; i < mesh->position_count; ++i)
  {
    auto pos = Point3f(mesh->positions[i * 3], mesh->positions[i * 3 + 1],
        mesh->positions[i * 3 + 2]);
    if (transform)
      pos = *transform * pos;
    positions.push_back(pos);
  }

  std::vector<uint32_t> indices;
  indices.reserve(mesh->index_count);
  for (size_t i = 0; i < mesh->index_count; ++i)
    indices.push_back(mesh->indices[i].p - 1);

  fast_obj_destroy(mesh);

  return Shape::new_triangles(
      TriangleMesh(std::move(indices), std::move(positions)));
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
    return read_triangle_mesh(table);
  }
  if (type == "obj_mesh")
  {
    return read_obj_mesh(table);
  }
  throw SceneParseException(
      "shape type {} has not yet been implemented", type);
}

Emitter read_emitter(toml::table const &primitive_table)
{
  auto const emitter_table_opt
      = get_as_opt<toml::table>(primitive_table, "emitter");
  if (!emitter_table_opt.has_value())
    return Emitter::new_invalid_emitter();
  auto const &emitter_table = *emitter_table_opt.value();

  auto const type = get_type(emitter_table);
  if (type == "area")
    return Emitter::new_area_emitter(AreaEmitter(Properties(&emitter_table)));

  throw SceneParseException(
      "emitter type {} has not been implemented yet", type);
}

BSDF read_bsdf(toml::table const &primitive_table)
{
  auto const &bsdf_table = get_as<toml::table>(primitive_table, "bsdf");
  auto const type = get_type(bsdf_table);
  if (type == "diffuse")
    return BSDF::new_diffuse_bsdf(Diffuse(Properties(&bsdf_table)));
  throw SceneParseException("bsdf type {} has not been implemented yet", type);
}

void process_primitive(
    std::vector<Primitive> &primitives, toml::node const &prim)
{
  auto const prim_table_ptr = prim.as_table();
  if (!prim_table_ptr)
  {
    throw SceneParseException(
        "expected table at line {}", prim.source().begin.line);
  }
  auto const &prim_table = *prim.as_table();
  auto const prim_type = get_type(prim_table);
  if (prim_type == "object")
  {
    auto const shapes
        = create_shapes(get_as<toml::table>(prim_table, "shape"));
    auto const emitter = read_emitter(prim_table);
    auto const bsdf = read_bsdf(prim_table);
    std::ranges::transform(shapes, std::back_insert_iterator(primitives),
        [emitter, bsdf](Shape const s)
        {
          return Primitive::new_basic_primitive(
              BasicPrimitive(s, emitter, bsdf));
        });
  }
}

Primitive generate_top_level_primitive(toml::table const &scene_config)
{
  const auto primitives_config = scene_config["primitive"].as_array();
  if (!primitives_config)
    throw SceneParseException("scene should contain at least one primitive");

  std::vector<Primitive> primitives;
  for (auto const &prim : *primitives_config)
    process_primitive(primitives, prim);

  Timer bvh_build_timer;
  auto bvh = BVHAggregate(std::move(primitives));
  auto const bvh_build_time = bvh_build_timer.stop();
  bvh.print_statistics();
  std::println("BVH building took {}", duration_string(bvh_build_time));
  return Primitive::new_bvh_aggregate(std::move(bvh));
}

std::unique_ptr<Camera> generate_camera(toml::table const &scene_config,
    uint32_t image_width, uint32_t image_height)
{
  auto const &camera_config = get_as<toml::table>(scene_config, "camera");
  auto const type = get_type(camera_config);
  if (type == "perspective")
  {
    return std::make_unique<PerspectiveCamera>(
        Properties(&camera_config), image_width, image_height);
  }
  if (type == "orthographic")
  {
    return std::make_unique<OrthographicCamera>(
        Properties(&camera_config), image_width, image_height);
  }
  throw SceneParseException(
      "camera type '{}' has not yet been implemented", type);
}

std::unique_ptr<Integrator> read_integrator(toml::table const &scene_config)
{
  auto const &integrator_config
      = get_as<toml::table>(scene_config, "integrator");
  auto const type = get_type(integrator_config);
  if (type == "normal")
  {
    return std::make_unique<NormalIntegrator>();
  }
  if (type == "random_walk")
  {
    return std::make_unique<RandomWalkIntegrator>(
        Properties(&integrator_config));
  }

  throw SceneParseException(
      "integrator type '{}' has not yet been implemented", type);
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

Point3f Properties::get_point3(std::string_view prop_name) const
{
  return get_float_array<Point3f>(*_table, prop_name);
}

Point3f Properties::get_point3(
    std::string_view prop_name, Point3f const &default_value) const
{
  return get_float_array_or_default<Point3f>(
      *_table, prop_name, default_value);
}

Vec3f Properties::get_vec3(std::string_view prop_name) const
{
  return get_float_array<Vec3f>(*_table, prop_name);
}

Vec3f Properties::get_vec3(
    std::string_view prop_name, Vec3f const &default_value) const
{
  return get_float_array_or_default<Vec3f>(*_table, prop_name, default_value);
}

Color3f Properties::get_color(std::string_view prop_name) const
{
  return get_float_array<Color3f>(*_table, prop_name);
}

Color3f Properties::get_color(
    std::string_view prop_name, Color3f const &default_value) const
{
  return get_float_array_or_default<Color3f>(
      *_table, prop_name, default_value);
}

Transform3D Properties::get_transform3d(std::string_view prop_name) const
{
  if (auto const t = read_transform3d_opt(*_table, prop_name))
    return t.value();
  throw SceneParseException(
      "expected transform named {} inside table at line {}", prop_name,
      _table->source().begin.line);
}

Transform3D Properties::get_transform3d(
    std::string_view prop_name, Transform3D const &default_value)
{
  return read_transform3d_opt(*_table, prop_name).value_or(default_value);
}

Real Properties::get_float(std::string_view prop_name) const
{
  if (auto const node = _table->get(prop_name))
    return to_float(*node);
  throw SceneParseException("missing property '{}' at line {}", prop_name,
      _table->source().begin.line);
}

Real Properties::get_float(
    std::string_view prop_name, Real default_value) const
{
  if (auto const node = _table->get(prop_name))
    return to_float(*node);
  return default_value;
}

int64_t Properties::get_int(std::string_view prop_name) const
{
  return get_as<int64_t>(*_table, prop_name);
}

int64_t Properties::get_int(
    std::string_view prop_name, int64_t default_value) const
{
  if (auto const node = _table->get(prop_name))
  {
    if (!node->is_integer())
    {
      throw SceneParseException(
          "expected integer at line {}", prop_name, node->source().begin.line);
    }
    return node->as_integer()->get();
  }
  return default_value;
}

Scene Scene::read_from_file(std::string_view path)
{
  auto const scene_config = parse_toml_file(path);
  Scene result;
  result.image_width
      = static_cast<uint32_t>(get_as<int64_t>(scene_config, "width"));
  result.image_height
      = static_cast<uint32_t>(get_as<int64_t>(scene_config, "height"));
  result.spp = static_cast<uint32_t>(get_as<int64_t>(scene_config, "spp"));
  result.integrator = read_integrator(scene_config);
  result.camera
      = generate_camera(scene_config, result.image_width, result.image_height);
  result.sampler = std::make_unique<UniformSampler>();
  result.top_level_primitive = generate_top_level_primitive(scene_config);

  return result;
}