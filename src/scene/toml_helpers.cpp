#include "toml_helpers.hpp"

toml::table parse_toml_file(std::filesystem::path const &path)
{
  try
  {
    return toml::parse_file(path.c_str());
  }
  catch (toml::parse_error const &err)
  {
    throw SceneParseException("failed to parse scene file '{}'.\nDetails: {}",
        path.string(), err.what());
  }
}

Real to_float(toml::node const &n)
{
  return n.visit(
      [&]<typename V>(V &v) -> Real
      {
        if constexpr (toml::is_floating_point<V>)
        {
          return as<float>(static_cast<double>(v));
        }
        else if constexpr (toml::is_integer<V>)
        {
          return as<float>(static_cast<int64_t>(v));
        }
        else
        {
          throw SceneParseException(
              "expected number at line {}", n.source().begin.line);
        }
      });
}

std::string_view get_type(toml::table const &table)
{
  return get_as<std::string>(table, "type");
}

Transform3D read_view_transform(
    toml::table const &table, std::string_view attribute_name)
{
  auto const transform_table = get_as<toml::table>(table, attribute_name);
  auto const type = get_type(transform_table);
  Transform3D result;
  if (type == "matrix")
  {
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
    throw SceneParseException("invalid transform type: '{}' (line {})", type,
        table.source().begin.line);
  }
  auto const inverse_ptr = transform_table.get_as<bool>("inverse");
  if (inverse_ptr && *inverse_ptr)
    return result.inverse();
  return result;
}

std::optional<Transform3D> read_object_transform_opt(
    toml::table const &table, std::string_view attribute_name)
{
  // TODO: maker this more efficient, too many calls that search for the
  // attribute in the table
  if (!table.contains(attribute_name))
    return std::nullopt;

  // supports both 3x4 array or table with more info
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
  throw SceneParseException(
      "expected transform matrix at line {}", table.source().begin.line);
}
