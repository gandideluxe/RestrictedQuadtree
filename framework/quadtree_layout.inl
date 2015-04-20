
// Copyright (c) 2012 Christopher Lux <christopherlux@gmail.com>
// Distributed under the Modified BSD License, see license.txt.

//#include <scm/core/math/math.h>

#include <quadtree_layout.h>

#include <cassert>

namespace scm {
namespace data {

inline
glm::int32
floor_log2(glm::int32 x)
{
    glm::int32 pos = 0;

    if (x & 0xffff0000u) { x >>= 16; pos += 16; }
    if (x & 0x0000ff00u) { x >>= 8; pos += 8; }
    if (x & 0x000000f0u) { x >>= 4; pos += 4; }
    if (x & 0x0000000cu) { x >>= 2; pos += 2; }
    if (x & 0x00000002u) { pos += 1; }

    return ((x == 0) ? (-1) : pos);
}

inline
quadtree_layout::index_type
quadtree_layout::total_node_count(unsigned depth) const
{
    //   ((4 ^ (depth + 1)) - 1) / (4 - 1)
    // = ((2 ^ 2) ^ (depth + 1) - 1) / ((2 ^ 2) - 1)
    // = ((2 ^ (2 * (depth + 1)) - 1) / ((2 ^ 2) - 1)
    return (((index_type(1) << (2 * (depth + 1))) - 1) / 3);
}

inline
quadtree_layout::index_type
quadtree_layout::total_node_count_level(unsigned level) const
{
    //   4 ^ level
    // = (2 ^ 2) ^ level
    // =  2 ^ (2 * level)
    return (index_type(1) << (2 * level));
}

inline
quadtree_layout::index_type
quadtree_layout::root_index() const
{
    return (0);
}

inline
unsigned
quadtree_layout::level_index(index_type node_index) const
{
    return (floor_log2(node_index * 3u + 1u) / 2u);
}

inline
quadtree_layout::index_type
quadtree_layout::parent_node_index(index_type  node_index) const
{
    // p_index = floor((c_index - 1) / 4)
    return ((node_index - 1) >> 2);
}

inline
quadtree_layout::index_type
quadtree_layout::child_node_index(index_type node_index,
                                                     unsigned   child_number) const
{
    assert(0 <= child_number && child_number <= 3);

    // c_index = 4 * p_index + child_number(1..4)
    return ((node_index << 2) + child_number + 1u);
}

inline
int
quadtree_layout::is_in_sub_tree_of(index_type child_index,
                                                      index_type parent_index) const
{
    // search bottom up if the parent index is in the path to the root
    index_type temp_par = child_index;
    int levels_up = 0;

    while (temp_par > parent_index) {
        temp_par = parent_node_index(temp_par);
        ++levels_up;
    }

    if (temp_par == parent_index) {
        return (levels_up);
    }
    else {
        return (-1);
    }
}

inline
unsigned
quadtree_layout::child_node_number(const position_type& position) const 
{
    //assert(0 <= position.x && position.x <= 1);
    //assert(0 <= position.y && position.y <= 1);

    //return (   position.x
    //        + (position.y << 1));

    return (   position.x & 1u
            | ((position.y & 1u) << 1));

}

inline
quadtree_layout::position_type
quadtree_layout::child_node_position(unsigned c) const  // 0 <= c <= 3
{
    assert(0 <= c && c <= 3);

    position_type  tmp;

    tmp.x   = (c & 1u);
    tmp.y   = (c & 2u) >> 1;

    return (tmp);
}

inline
quadtree_layout::position_type
quadtree_layout::node_position(index_type node_index) const  // 0 <= layer_index <= 8^plah
{
    // we use the z-order curve, so we have to deinterlace the bits
    // to get the position
    static const unsigned max_iterations = ((sizeof(index_type) * 8) / 2);

    // get the node index on the current level
    const unsigned node_level = level_index(node_index);
    node_index -= (node_level == 0 ? 0 : total_node_count(node_level - 1));

    position_type  tmp(0u, 0u);

    for (unsigned i = 0; i < max_iterations; ++i) {

        tmp.x |= index_type(node_index & 1) << i;
        tmp.y |= index_type(node_index & 2) << i;

        node_index >>= 2;
    }

    tmp.y >>= 1;

    return (tmp);
}

inline
quadtree_layout::index_type
quadtree_layout::node_index(const position_type& node_position, unsigned level) const  
{
#if 1
    // we use the z-order curve, so we have to interlace the bits
    // of the 2d position to get the index per level and add the node count
    // of the lower levels to get the global quadtree index
    static const unsigned max_iterations = ((sizeof(index_type) * 8) / 2);

    index_type  tmp(0u);

    for (unsigned i = 0; i < max_iterations; ++i) {
        position_type::value_type bit = (1 << i);

        tmp |= index_type(node_position.x & bit) << (i);
        tmp |= index_type(node_position.y & bit) << (i + 1);
    }
#else
static const unsigned short MortonTable256[256] = 
{
  0x0000, 0x0001, 0x0004, 0x0005, 0x0010, 0x0011, 0x0014, 0x0015, 
  0x0040, 0x0041, 0x0044, 0x0045, 0x0050, 0x0051, 0x0054, 0x0055, 
  0x0100, 0x0101, 0x0104, 0x0105, 0x0110, 0x0111, 0x0114, 0x0115, 
  0x0140, 0x0141, 0x0144, 0x0145, 0x0150, 0x0151, 0x0154, 0x0155, 
  0x0400, 0x0401, 0x0404, 0x0405, 0x0410, 0x0411, 0x0414, 0x0415, 
  0x0440, 0x0441, 0x0444, 0x0445, 0x0450, 0x0451, 0x0454, 0x0455, 
  0x0500, 0x0501, 0x0504, 0x0505, 0x0510, 0x0511, 0x0514, 0x0515, 
  0x0540, 0x0541, 0x0544, 0x0545, 0x0550, 0x0551, 0x0554, 0x0555, 
  0x1000, 0x1001, 0x1004, 0x1005, 0x1010, 0x1011, 0x1014, 0x1015, 
  0x1040, 0x1041, 0x1044, 0x1045, 0x1050, 0x1051, 0x1054, 0x1055, 
  0x1100, 0x1101, 0x1104, 0x1105, 0x1110, 0x1111, 0x1114, 0x1115, 
  0x1140, 0x1141, 0x1144, 0x1145, 0x1150, 0x1151, 0x1154, 0x1155, 
  0x1400, 0x1401, 0x1404, 0x1405, 0x1410, 0x1411, 0x1414, 0x1415, 
  0x1440, 0x1441, 0x1444, 0x1445, 0x1450, 0x1451, 0x1454, 0x1455, 
  0x1500, 0x1501, 0x1504, 0x1505, 0x1510, 0x1511, 0x1514, 0x1515, 
  0x1540, 0x1541, 0x1544, 0x1545, 0x1550, 0x1551, 0x1554, 0x1555, 
  0x4000, 0x4001, 0x4004, 0x4005, 0x4010, 0x4011, 0x4014, 0x4015, 
  0x4040, 0x4041, 0x4044, 0x4045, 0x4050, 0x4051, 0x4054, 0x4055, 
  0x4100, 0x4101, 0x4104, 0x4105, 0x4110, 0x4111, 0x4114, 0x4115, 
  0x4140, 0x4141, 0x4144, 0x4145, 0x4150, 0x4151, 0x4154, 0x4155, 
  0x4400, 0x4401, 0x4404, 0x4405, 0x4410, 0x4411, 0x4414, 0x4415, 
  0x4440, 0x4441, 0x4444, 0x4445, 0x4450, 0x4451, 0x4454, 0x4455, 
  0x4500, 0x4501, 0x4504, 0x4505, 0x4510, 0x4511, 0x4514, 0x4515, 
  0x4540, 0x4541, 0x4544, 0x4545, 0x4550, 0x4551, 0x4554, 0x4555, 
  0x5000, 0x5001, 0x5004, 0x5005, 0x5010, 0x5011, 0x5014, 0x5015, 
  0x5040, 0x5041, 0x5044, 0x5045, 0x5050, 0x5051, 0x5054, 0x5055, 
  0x5100, 0x5101, 0x5104, 0x5105, 0x5110, 0x5111, 0x5114, 0x5115, 
  0x5140, 0x5141, 0x5144, 0x5145, 0x5150, 0x5151, 0x5154, 0x5155, 
  0x5400, 0x5401, 0x5404, 0x5405, 0x5410, 0x5411, 0x5414, 0x5415, 
  0x5440, 0x5441, 0x5444, 0x5445, 0x5450, 0x5451, 0x5454, 0x5455, 
  0x5500, 0x5501, 0x5504, 0x5505, 0x5510, 0x5511, 0x5514, 0x5515, 
  0x5540, 0x5541, 0x5544, 0x5545, 0x5550, 0x5551, 0x5554, 0x5555
};

unsigned short x = static_cast<unsigned short>(node_position.x); // Interleave bits of x and y, so that all of the
unsigned short y = static_cast<unsigned short>(node_position.y); // bits of x are in the even positions and y in the odd;
unsigned int tmp;   // z gets the resulting 32-bit Morton Number.

tmp = MortonTable256[y >> 8]   << 17 | 
    MortonTable256[x >> 8]   << 16 |
    MortonTable256[y & 0xFF] <<  1 | 
    MortonTable256[x & 0xFF];
#endif


    return (tmp + (level == 0 ? 0 : total_node_count(level - 1)));
}

} // namespace data
} // namespace scm
