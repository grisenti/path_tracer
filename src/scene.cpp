#include "scene.hpp"

#include <algorithm>

#include "cameras.hpp"
#include "integrators.hpp"
#include "core/utils.hpp"
#include "scene/toml_helpers.hpp"
#include "scene/mesh_parser.hpp"
#include "scene/properties.hpp"

#include <filesystem>

namespace
{

std::vector<uint32_t> read_indices(toml::array const &list)
{
  std::vector<uint32_t> result;
  result.reserve(list.size());
  std::ranges::transform(list, std::back_insert_iterator(result),
      [](toml::node const &node) -> uint32_t
      {
        if (auto const value_ptr = node.as<int64_t>())
          return as<uint32_t>(static_cast<int64_t>(*value_ptr));
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

  if (auto const transform = read_object_transform_opt(table, "to_world"))
  {
    for (auto &p : positions)
      p = *transform * p;
  }

  TriangleMesh mesh(std::move(indices), std::move(positions));
  return Shape::new_triangles(std::move(mesh));
}

BSDF read_bsdf(toml::table const &primitive_table)
{
  auto const &bsdf_table = get_as<toml::table>(primitive_table, "bsdf");
  auto const type = get_type(bsdf_table);
  Properties const props(&bsdf_table);
  if (type == "diffuse")
    return BSDF::new_diffuse_bsdf(Diffuse(props));
  if (type == "dielectric")
    return BSDF::new_dielectric_bsdf(Dielectric(props));
  if (type == "mirror")
    return BSDF::new_mirror_bsdf(Mirror(props));
  throw SceneParseException("bsdf type {} has not been implemented yet", type);
}

class SceneParser
{
public:
  SceneParser(std::filesystem::path const &file_path)
      : _scene_config(parse_toml_file(file_path)),
        _base_path(file_path.parent_path())
  {
  }

  // only meant to be called once
  Scene parse();
  void generate_top_level_primitive();
  void process_primitive(
      std::vector<Primitive> &primitives, toml::node const &prim);
  void process_primitive_object(
      std::vector<Primitive> &primitives, toml::table const &prim_table);
  std::vector<Shape> create_shapes(toml::table const &table) const;
  void read_integrator();
  void generate_camera();

private:
  toml::table _scene_config;
  std::filesystem::path _base_path;
  Scene _result = {};
};

Scene SceneParser::parse()
{
  Timer timer;
  _result.image_width = as<uint32_t>(get_as<int64_t>(_scene_config, "width"));
  _result.image_height
      = as<uint32_t>(get_as<int64_t>(_scene_config, "height"));
  _result.spp = as<uint32_t>(get_as<int64_t>(_scene_config, "spp"));
  _result.sampler = std::make_unique<UniformSampler>();

  generate_top_level_primitive();
  read_integrator();
  generate_camera();

  auto const time_to_read_scene = timer.check();
  std::println("reading scene took {}", duration_string(time_to_read_scene));
  return std::move(_result);
}

void SceneParser::generate_top_level_primitive()
{
  const auto primitives_config = _scene_config["primitive"].as_array();
  if (!primitives_config)
    throw SceneParseException("scene should contain at least one primitive");

  std::vector<Primitive> primitives;
  for (auto const &prim : *primitives_config)
    process_primitive(primitives, prim);

  Timer bvh_build_timer;
  auto bvh = BVHAggregate(std::move(primitives));
  auto const bvh_build_time = bvh_build_timer.check();
  bvh.print_statistics();
  std::println("BVH building took {}", duration_string(bvh_build_time));
  _result.top_level_primitive = Primitive::new_bvh_aggregate(std::move(bvh));
}

void SceneParser::process_primitive(
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
    process_primitive_object(primitives, prim_table);
  }
  else
  {
    throw SceneParseException("invalid primitive type: '{}' (line {})",
        prim_type, prim_table.source().begin.line);
  }
}

void SceneParser::process_primitive_object(
    std::vector<Primitive> &primitives, toml::table const &prim_table)
{
  auto const shapes = create_shapes(get_as<toml::table>(prim_table, "shape"));
  auto const bsdf = read_bsdf(prim_table);
  auto const emitter_table_opt
      = get_as_opt<toml::table>(prim_table, "emitter");
  if (!emitter_table_opt.has_value())
  {
    for (auto const s : shapes)
    {
      primitives.push_back(Primitive::new_basic_primitive(
          BasicPrimitive(s, Emitter::new_invalid_emitter(), bsdf)));
    }
    return;
  }
  auto const &emitter_table = *emitter_table_opt.value();
  auto const type = get_type(emitter_table);
  if (type == "area")
  {
    for (auto const s : shapes)
    {
      auto const emitter = Emitter::new_area_emitter(
          AreaEmitter(Properties(&emitter_table), s));
      _result.lights.push_back(emitter);
      primitives.push_back(
          Primitive::new_basic_primitive(BasicPrimitive(s, emitter, bsdf)));
    }
  }
  else
  {
    throw SceneParseException(
        "emitter type {} has not been implemented yet", type);
  }
}

std::vector<Shape> SceneParser::create_shapes(toml::table const &table) const
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
    auto const &filename = get_as<std::string>(table, "filename");
    auto const full_path = _base_path / filename;
    auto const transform = read_object_transform_opt(table, "to_world");
    return Shape::new_triangles(read_obj_mesh(full_path, transform));
  }
  throw SceneParseException(
      "shape type {} has not yet been implemented", type);
}

void SceneParser::read_integrator()
{
  auto const &integrator_config
      = get_as<toml::table>(_scene_config, "integrator");
  auto const type = get_type(integrator_config);
  if (type == "normals")
  {
    _result.integrator = std::make_unique<NormalsIntegrator>();
  }
  else if (type == "random_walk")
  {
    _result.integrator = std::make_unique<RandomWalkIntegrator>(
        Properties(&integrator_config));
  }
  else if (type == "simple_path")
  {
    _result.integrator = std::make_unique<SimplePathIntegrator>(
        Properties(&integrator_config),
        std::make_unique<UniformLightSampler>(_result));
  }
  else if (type == "path")
  {
    _result.integrator
        = std::make_unique<PathIntegrator>(Properties(&integrator_config),
            std::make_unique<UniformLightSampler>(_result));
  }
  else
  {
    throw SceneParseException(
        "integrator type '{}' has not yet been implemented", type);
  }
}

void SceneParser::generate_camera()
{
  auto const &camera_config = get_as<toml::table>(_scene_config, "camera");
  auto const type = get_type(camera_config);
  if (type == "perspective")
  {
    auto const view_transform
        = read_view_transform(camera_config, "view_transform");
    _result.camera
        = std::make_unique<PerspectiveCamera>(Properties(&camera_config),
            view_transform, _result.image_width, _result.image_height);
  }
  else if (type == "orthographic")
  {
    auto const view_transform
        = read_view_transform(camera_config, "view_transform");
    _result.camera
        = std::make_unique<OrthographicCamera>(Properties(&camera_config),
            view_transform, _result.image_width, _result.image_height);
  }
  else
  {
    throw SceneParseException(
        "camera type '{}' has not yet been implemented", type);
  }
}

}

Scene Scene::read_from_file(std::string_view path)
{
  auto parser = SceneParser(path);
  return parser.parse();
}