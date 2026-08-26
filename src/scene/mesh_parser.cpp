#include "mesh_parser.hpp"

#include <fast_obj.h>

#include "scene.hpp"

// from:
// https://stackoverflow.com/questions/35985960/c-why-is-boosthash-combine-the-best-way-to-combine-hash-values
static size_t hash_combine(size_t acc, fastObjUInt x)
{
  std::hash<fastObjUInt> hasher;
  acc ^= hasher(x) + 0x9e3779b9 + (acc << 6) + (acc >> 2);
  return acc;
}

template <>
struct std::hash<fastObjIndex>
{
  size_t operator()(fastObjIndex const &idx) const noexcept
  {
    auto const h1 = hash_combine(0, idx.p);
    auto const h2 = hash_combine(h1, idx.t);
    return hash_combine(h2, idx.n);
  }
};

bool operator==(fastObjIndex const &a, fastObjIndex const &b)
{
  return a.p == b.p && a.t == b.t && a.n == b.n;
}

namespace
{

template <ModifiableArray A>
A aggregate_from_c_array(ComponentT<A> *c_arr)
{
  A res;
  for (size_t i = 0; i < A::SIZE; ++i)
    res[i] = c_arr[i];
  return res;
}

}

TriangleMesh read_obj_mesh(std::filesystem::path const &path,
    std::optional<Transform3D> const &transform)
{
  // TODO: maybe converting to string and then to cstr is not a good idea
  auto const mesh = fast_obj_read(path.string().c_str());
  if (!mesh)
    throw SceneParseException("failed to parse obj file '{}'", path.string());

  std::unordered_map<fastObjIndex, uint32_t> index_map;
  uint32_t last_index = 0;
  std::vector<Point3f> positions;
  std::vector<Normal3f> normals;
  std::vector<Vec2f> uvs;
  std::vector<uint32_t> indices;

  for (size_t i = 0; i < mesh->index_count; ++i)
  {
    auto const idx = mesh->indices[i];
    auto const it = index_map.find(idx);
    if (it == index_map.end())
    {
      auto pos = aggregate_from_c_array<Point3f>(mesh->positions + idx.p * 3);
      if (transform)
        pos = transform.value() * pos;
      positions.push_back(pos);
      if (idx.n != 0)
      {
        auto normal
            = aggregate_from_c_array<Normal3f>(mesh->normals + idx.n * 3);
        if (transform)
          normal = normal * transform.value();
        normals.push_back(normal);
      }
      if (idx.t != 0)
      {
        auto const uv
            = aggregate_from_c_array<Vec2f>(mesh->texcoords + idx.t * 2);
        uvs.push_back(uv);
      }
      indices.push_back(last_index);
      index_map.insert({ idx, last_index });
      ++last_index;
    }
    else
    {
      indices.push_back(it->second);
    }
  }

  fast_obj_destroy(mesh);

  return TriangleMesh(
      std::move(indices), std::move(positions), std::move(normals));
}