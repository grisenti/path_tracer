#include "bvh_8.hpp"

#include <numeric>

#include "bvh_build.hpp"
#include "primitives.hpp"
#include "core/utils.hpp"

namespace
{

struct SimdRay
{
  v8f o8[3];
  v8f d8[3];
  v8f dRcp8[3];
  int signs[3];
  Real t_min;
  Real t_max;

  explicit SimdRay(Ray const &r);
};

template <typename Iter>
void swap_range(Iter s1, Iter s2);

const v8f ZERO8 = _mm256_set1_ps(0.f);

v8f ray_bounds_intersection_test(const Bvh8Node &b, const SimdRay &r);

extern const v8i LUT[256];

}

Bvh8::Bvh8(std::vector<Primitive> primitives)
    : _primitives(std::move(primitives))
{
  auto ir = build_bvh(_primitives);
  _bounds = ir.root->bounds;
  _leaf_nodes_count = ir.leaf_nodes;
  _internal_nodes_count = ir.leaf_nodes;
  pack_tree(ir.root.get());
}

std::optional<PrimitiveIntersection> Bvh8::intersect(Ray ray) const
{
  if (!_bounds.intersect(ray))
    return std::nullopt;

  std::optional<PrimitiveIntersection> result;
  SimdRay sray(ray);
  const int signs
      = (sray.signs[0] | sray.signs[1] << 1 | sray.signs[2] << 2) * 3;
  std::array<uint32_t, 64> node_stack;
  uint32_t stack_top = 0;
  uint32_t current_node = 0;
  while (true)
  {
    if (Bvh8ChildIndex::is_leaf(current_node))
    {
      auto const first_prim
          = Bvh8ChildIndex::primitive_starting_index(current_node);
      auto const prim_count = Bvh8ChildIndex::primitive_count(current_node);
      auto prims = std::span{ _primitives.data() + first_prim, prim_count };
      for (auto const prim : prims)
      {
        if (auto const its = prim.intersect(ray))
        {
          sray.t_max = ray.t_max = its->shape_intersection.t;
          result = its;
        }
      }
    }
    else
    {
      auto const &node = _nodes[current_node];
      auto const intersection_mask8 = ray_bounds_intersection_test(node, sray);
      auto const intersection_mask
          = (uint32_t)_mm256_movemask_ps(intersection_mask8);
      auto const intersection_count
          = (uint32_t)std::popcount(intersection_mask);
      if (intersection_count == 1)
      {
        auto const index = std::bit_width(intersection_mask) - 1;
        auto const child = ((uint32_t *)&node.children)[index];
        node_stack[stack_top++] = child;
      }
      else if (intersection_count > 0)
      {
        auto const index = _mm256_srli_epi32(node.permutations, signs);
        auto const permuted_children8
            = _mm256_permutevar8x32_epi32(node.children, index);
        auto const permuted_intersection_mask8
            = _mm256_permutevar8x32_ps(intersection_mask8, index);
        auto const permuted_intersection_mask
            = _mm256_movemask_ps(permuted_intersection_mask8);
        auto const compress_idx = LUT[permuted_intersection_mask];
        auto const compressed_permuted_children8
            = _mm256_permutevar8x32_epi32(permuted_children8, compress_idx);
        _mm256_storeu_si256((v8i *)(node_stack.data() + stack_top),
            compressed_permuted_children8);
        stack_top += intersection_count;
      }
    }
    if (stack_top == 0)
      break;
    current_node = node_stack[--stack_top];
  }
  return result;
}

Bbox3f Bvh8::bounds() const
{
  return _bounds;
}

void Bvh8::print_statistics() const
{
  std::println("BVH8 statistics:");
  std::println("- primitives count: {}", split_thousands(_primitives.size()));
  std::println("- internal nodes: {}", split_thousands(_internal_nodes_count));
  std::println("- leaf nodes: {}", split_thousands(_leaf_nodes_count));
  std::println("- node packs: {}", _nodes.size());
  std::println("- node pack occupancy: {:.2f}",
      as<Real>(_internal_nodes_count + _leaf_nodes_count)
          / (as<Real>(_nodes.size()) * 8.f));
  std::println("- average primitive per leaf: {:.2f}",
      as<Real>(_primitives.size()) / as<Real>(_leaf_nodes_count));

  auto const node_memory = _internal_nodes_count * sizeof(Bvh8Node);
  auto const triangle_memory = _primitives.size() * sizeof(Primitive);
  std::println("- memory use: {}", mem_string(node_memory + triangle_memory));
}

Bvh8ChildIndex Bvh8::pack_tree(BvhBuildNode *build_node)
{
  if (build_node->primitive_count != 0)
    return { build_node->first_primitive, build_node->primitive_count };

  std::array<Bbox3f, 8> bounds{};
  std::array<Bvh8ChildIndex, 8> children{};
  std::array<uint8_t, 7> split_axis{};
  split_axis[6] = as<uint8_t>(build_node->split_axis);

  auto const node_index = _nodes.size();
  _nodes.emplace_back();

  auto const left_child = build_node->children[0].get();
  split_axis[5] = as<uint8_t>(left_child->split_axis);
  if (left_child->primitive_count == 0)
  {
    auto const ll_child = left_child->children[0].get();
    split_axis[0] = as<uint8_t>(ll_child->split_axis);
    if (ll_child->primitive_count == 0)
    {
      bounds[0] = ll_child->children[0]->bounds;
      children[0] = pack_tree(ll_child->children[0].get());
      bounds[1] = ll_child->children[1]->bounds;
      children[1] = pack_tree(ll_child->children[1].get());
    }
    else
    {
      bounds[0] = ll_child->bounds;
      children[0] = pack_tree(ll_child);
      bounds[1] = Bbox3f();
    }
    auto lr_child = left_child->children[1].get();
    split_axis[1] = as<uint8_t>(lr_child->split_axis);
    if (lr_child->primitive_count == 0)
    {
      bounds[2] = lr_child->children[0]->bounds;
      children[2] = pack_tree(lr_child->children[0].get());
      bounds[3] = lr_child->children[1]->bounds;
      children[3] = pack_tree(lr_child->children[1].get());
    }
    else
    {
      bounds[2] = lr_child->bounds;
      children[2] = pack_tree(lr_child);
      bounds[3] = Bbox3f();
    }
  }
  else
  {
    bounds[0] = left_child->bounds;
    children[0] = pack_tree(left_child);
    bounds[1] = Bbox3f();
    bounds[2] = Bbox3f();
    bounds[3] = Bbox3f();
  }

  auto const right_child = build_node->children[1].get();
  split_axis[4] = as<uint8_t>(right_child->split_axis);
  if (right_child->primitive_count == 0)
  {
    auto const rl_child = right_child->children[0].get();
    split_axis[2] = as<uint8_t>(rl_child->split_axis);
    if (rl_child->primitive_count == 0)
    {
      bounds[4] = rl_child->children[0]->bounds;
      children[4] = pack_tree(rl_child->children[0].get());
      bounds[5] = rl_child->children[1]->bounds;
      children[5] = pack_tree(rl_child->children[1].get());
    }
    else
    {
      bounds[4] = rl_child->bounds;
      children[4] = pack_tree(rl_child);
      bounds[5] = Bbox3f();
    }
    auto rr_child = right_child->children[1].get();
    split_axis[3] = as<uint8_t>(rr_child->split_axis);
    if (rr_child->primitive_count == 0)
    {
      bounds[6] = rr_child->children[0]->bounds;
      children[6] = pack_tree(rr_child->children[0].get());
      bounds[7] = rr_child->children[1]->bounds;
      children[7] = pack_tree(rr_child->children[1].get());
    }
    else
    {
      bounds[6] = rr_child->bounds;
      children[6] = pack_tree(rr_child);
      bounds[7] = Bbox3f();
    }
  }
  else
  {
    bounds[4] = right_child->bounds;
    children[4] = pack_tree(right_child);
    bounds[5] = Bbox3f();
    bounds[6] = Bbox3f();
    bounds[7] = Bbox3f();
  }

  Bvh8Node &node = _nodes[node_index];
  node.permutations = _mm256_setzero_si256();
  for (size_t i = 0; i < 8; ++i)
  {
    ((uint32_t *)&node.children)[i] = children[i].value;
    for (size_t axis = 0; axis < 3; ++axis)
    {
      ((float *)&node.corners[0][axis])[i] = bounds[i].min[axis];
      ((float *)&node.corners[1][axis])[i] = bounds[i].max[axis];
    }
  }
  for (size_t s = 0; s < 8; ++s)
  {
    std::array<uint8_t, 8> positions;
    std::iota(positions.begin(), positions.end(), 0);
    for (size_t i = 0; i < 3; ++i)
    {
      if (s & (1 << i))
      {
        for (size_t j = 0; j < 4; ++j)
        {
          if (split_axis[j] == i)
            std::swap(positions[j * 2], positions[j * 2 + 1]);
        }

        for (size_t j = 0; j < 2; ++j)
        {
          if (split_axis[j + 4] == i)
            swap_range(&positions[j * 4], &positions[j * 4 + 2]);
        }

        if (split_axis[6] == i)
          swap_range(&positions[0], &positions[4]);
      }
    }
    for (size_t i = 0; i < 8; ++i)
    {
      ((uint32_t *)&node.permutations)[i]
          |= as<uint32_t>(positions[i] << (s * 3));
    }
  }
  return Bvh8ChildIndex(as<uint32_t>(node_index), false);
}

namespace
{

SimdRay::SimdRay(Ray const &r)
{
  o8[0] = _mm256_set1_ps(r.o.x);
  o8[1] = _mm256_set1_ps(r.o.y);
  o8[2] = _mm256_set1_ps(r.o.z);
  d8[0] = _mm256_set1_ps(r.d()->x);
  d8[1] = _mm256_set1_ps(r.d()->y);
  d8[2] = _mm256_set1_ps(r.d()->z);
  dRcp8[0] = _mm256_set1_ps(r.inv_d().x);
  dRcp8[1] = _mm256_set1_ps(r.inv_d().y);
  dRcp8[2] = _mm256_set1_ps(r.inv_d().z);

  signs[0] = std::signbit(r.d()->x);
  signs[1] = std::signbit(r.d()->y);
  signs[2] = std::signbit(r.d()->z);

  t_min = r.t_min;
  t_max = r.t_max;
}

template <typename Iter>
void swap_range(Iter s1, Iter s2)
{
  auto it2 = s2;
  for (auto it = s1; it < s2; ++it)
  {
    std::swap(*it, *it2);
    ++it2;
  }
}

// code based on the slab method described in
// https://tavianator.com/2022/ray_box_boundary.html
v8f ray_bounds_intersection_test(const Bvh8Node &b, const SimdRay &r)
{
  v8f tmin = _mm256_set1_ps(r.t_min);
  v8f tmax = _mm256_set1_ps(r.t_max);
  for (size_t d = 0; d < 3; ++d)
  {
    v8f const bmin = b.corners[r.signs[d]][d];
    v8f const bmax = b.corners[!r.signs[d]][d];

    v8f const dmin = _mm256_mul_ps(_mm256_sub_ps(bmin, r.o8[d]), r.dRcp8[d]);
    v8f const dmax = _mm256_mul_ps(_mm256_sub_ps(bmax, r.o8[d]), r.dRcp8[d]);

    tmin = _mm256_max_ps(dmin, tmin);
    tmax = _mm256_min_ps(dmax, tmax);
  }

  return _mm256_and_ps(_mm256_cmp_ps(tmin, tmax, _CMP_LE_OQ),
      _mm256_cmp_ps(_mm256_castsi256_ps(b.children), ZERO8, _CMP_NEQ_OQ));
}

#define tov8i(x) _mm256_cvtepu8_epi32(_mm_cvtsi64_si128(x))
// given a mask of which bboxes got hit, returns a partition
// such that hit elements are at the front in the reverse order
// (for pushing onto the stack)
const v8i LUT[256] = {
  tov8i(506097522914230528),
  tov8i(506097522914230528),
  tov8i(506097522914230273),
  tov8i(506097522914230273),
  tov8i(506097522914164738),
  tov8i(506097522914164738),
  tov8i(506097522914099458),
  tov8i(506097522914099458),
  tov8i(506097522897387523),
  tov8i(506097522897387523),
  tov8i(506097522897322243),
  tov8i(506097522897322243),
  tov8i(506097522880545283),
  tov8i(506097522880545283),
  tov8i(506097522863833603),
  tov8i(506097522863833603),
  tov8i(506097518602420228),
  tov8i(506097518602420228),
  tov8i(506097518602354948),
  tov8i(506097518602354948),
  tov8i(506097518585577988),
  tov8i(506097518585577988),
  tov8i(506097518568866308),
  tov8i(506097518568866308),
  tov8i(506097514290610948),
  tov8i(506097514290610948),
  tov8i(506097514273899268),
  tov8i(506097514273899268),
  tov8i(506097509978997508),
  tov8i(506097509978997508),
  tov8i(506097505700807428),
  tov8i(506097505700807428),
  tov8i(506096419090792453),
  tov8i(506096419090792453),
  tov8i(506096419090727173),
  tov8i(506096419090727173),
  tov8i(506096419073950213),
  tov8i(506096419073950213),
  tov8i(506096419057238533),
  tov8i(506096419057238533),
  tov8i(506096414778983173),
  tov8i(506096414778983173),
  tov8i(506096414762271493),
  tov8i(506096414762271493),
  tov8i(506096410467369733),
  tov8i(506096410467369733),
  tov8i(506096406189179653),
  tov8i(506096406189179653),
  tov8i(506095315267355653),
  tov8i(506095315267355653),
  tov8i(506095315250643973),
  tov8i(506095315250643973),
  tov8i(506095310955742213),
  tov8i(506095310955742213),
  tov8i(506095306677552133),
  tov8i(506095306677552133),
  tov8i(506094211444179973),
  tov8i(506094211444179973),
  tov8i(506094207165989893),
  tov8i(506094207165989893),
  tov8i(506093107671139333),
  tov8i(506093107671139333),
  tov8i(506092012454478853),
  tov8i(506092012454478853),
  tov8i(505814944114081798),
  tov8i(505814944114081798),
  tov8i(505814944114016518),
  tov8i(505814944114016518),
  tov8i(505814944097239558),
  tov8i(505814944097239558),
  tov8i(505814944080527878),
  tov8i(505814944080527878),
  tov8i(505814939802272518),
  tov8i(505814939802272518),
  tov8i(505814939785560838),
  tov8i(505814939785560838),
  tov8i(505814935490659078),
  tov8i(505814935490659078),
  tov8i(505814931212468998),
  tov8i(505814931212468998),
  tov8i(505813840290644998),
  tov8i(505813840290644998),
  tov8i(505813840273933318),
  tov8i(505813840273933318),
  tov8i(505813835979031558),
  tov8i(505813835979031558),
  tov8i(505813831700841478),
  tov8i(505813831700841478),
  tov8i(505812736467469318),
  tov8i(505812736467469318),
  tov8i(505812732189279238),
  tov8i(505812732189279238),
  tov8i(505811632694428678),
  tov8i(505811632694428678),
  tov8i(505810537477768198),
  tov8i(505810537477768198),
  tov8i(505532365313934598),
  tov8i(505532365313934598),
  tov8i(505532365297222918),
  tov8i(505532365297222918),
  tov8i(505532361002321158),
  tov8i(505532361002321158),
  tov8i(505532356724131078),
  tov8i(505532356724131078),
  tov8i(505531261490758918),
  tov8i(505531261490758918),
  tov8i(505531257212568838),
  tov8i(505531257212568838),
  tov8i(505530157717718278),
  tov8i(505530157717718278),
  tov8i(505529062501057798),
  tov8i(505529062501057798),
  tov8i(505249786514113798),
  tov8i(505249786514113798),
  tov8i(505249782235923718),
  tov8i(505249782235923718),
  tov8i(505248682741073158),
  tov8i(505248682741073158),
  tov8i(505247587524412678),
  tov8i(505247587524412678),
  tov8i(504967207781139718),
  tov8i(504967207781139718),
  tov8i(504966112564479238),
  tov8i(504966112564479238),
  tov8i(504684641882735878),
  tov8i(504684641882735878),
  tov8i(504404266417652998),
  tov8i(504404266417652998),
  tov8i(433757350076153863),
  tov8i(433757350076153863),
  tov8i(433757350076088583),
  tov8i(433757350076088583),
  tov8i(433757350059311623),
  tov8i(433757350059311623),
  tov8i(433757350042599943),
  tov8i(433757350042599943),
  tov8i(433757345764344583),
  tov8i(433757345764344583),
  tov8i(433757345747632903),
  tov8i(433757345747632903),
  tov8i(433757341452731143),
  tov8i(433757341452731143),
  tov8i(433757337174541063),
  tov8i(433757337174541063),
  tov8i(433756246252717063),
  tov8i(433756246252717063),
  tov8i(433756246236005383),
  tov8i(433756246236005383),
  tov8i(433756241941103623),
  tov8i(433756241941103623),
  tov8i(433756237662913543),
  tov8i(433756237662913543),
  tov8i(433755142429541383),
  tov8i(433755142429541383),
  tov8i(433755138151351303),
  tov8i(433755138151351303),
  tov8i(433754038656500743),
  tov8i(433754038656500743),
  tov8i(433752943439840263),
  tov8i(433752943439840263),
  tov8i(433474771276006663),
  tov8i(433474771276006663),
  tov8i(433474771259294983),
  tov8i(433474771259294983),
  tov8i(433474766964393223),
  tov8i(433474766964393223),
  tov8i(433474762686203143),
  tov8i(433474762686203143),
  tov8i(433473667452830983),
  tov8i(433473667452830983),
  tov8i(433473663174640903),
  tov8i(433473663174640903),
  tov8i(433472563679790343),
  tov8i(433472563679790343),
  tov8i(433471468463129863),
  tov8i(433471468463129863),
  tov8i(433192192476185863),
  tov8i(433192192476185863),
  tov8i(433192188197995783),
  tov8i(433192188197995783),
  tov8i(433191088703145223),
  tov8i(433191088703145223),
  tov8i(433189993486484743),
  tov8i(433189993486484743),
  tov8i(432909613743211783),
  tov8i(432909613743211783),
  tov8i(432908518526551303),
  tov8i(432908518526551303),
  tov8i(432627047844807943),
  tov8i(432627047844807943),
  tov8i(432346672379725063),
  tov8i(432346672379725063),
  tov8i(361417177238078983),
  tov8i(361417177238078983),
  tov8i(361417177221367303),
  tov8i(361417177221367303),
  tov8i(361417172926465543),
  tov8i(361417172926465543),
  tov8i(361417168648275463),
  tov8i(361417168648275463),
  tov8i(361416073414903303),
  tov8i(361416073414903303),
  tov8i(361416069136713223),
  tov8i(361416069136713223),
  tov8i(361414969641862663),
  tov8i(361414969641862663),
  tov8i(361413874425202183),
  tov8i(361413874425202183),
  tov8i(361134598438258183),
  tov8i(361134598438258183),
  tov8i(361134594160068103),
  tov8i(361134594160068103),
  tov8i(361133494665217543),
  tov8i(361133494665217543),
  tov8i(361132399448557063),
  tov8i(361132399448557063),
  tov8i(360852019705284103),
  tov8i(360852019705284103),
  tov8i(360850924488623623),
  tov8i(360850924488623623),
  tov8i(360569453806880263),
  tov8i(360569453806880263),
  tov8i(360289078341797383),
  tov8i(360289078341797383),
  tov8i(289077004400395783),
  tov8i(289077004400395783),
  tov8i(289077000122205703),
  tov8i(289077000122205703),
  tov8i(289075900627355143),
  tov8i(289075900627355143),
  tov8i(289074805410694663),
  tov8i(289074805410694663),
  tov8i(288794425667421703),
  tov8i(288794425667421703),
  tov8i(288793330450761223),
  tov8i(288793330450761223),
  tov8i(288511859769017863),
  tov8i(288511859769017863),
  tov8i(288231484303934983),
  tov8i(288231484303934983),
  tov8i(216736831646270983),
  tov8i(216736831646270983),
  tov8i(216735736429610503),
  tov8i(216735736429610503),
  tov8i(216454265747867143),
  tov8i(216454265747867143),
  tov8i(216173890282784263),
  tov8i(216173890282784263),
  tov8i(144396676004906503),
  tov8i(144396676004906503),
  tov8i(144116300539823623),
  tov8i(144116300539823623),
  tov8i(72059806013523463),
  tov8i(72059806013523463),
  tov8i(283686952306183),
  tov8i(283686952306183),
};

}