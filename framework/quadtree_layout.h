
// Copyright (c) 2012 Christopher Lux <christopherlux@gmail.com>
// Distributed under the Modified BSD License, see license.txt.

#ifndef SCM_LARGE_DATA_QUADTREE_LAYOUT_ZORDER_H_INCLUDED
#define SCM_LARGE_DATA_QUADTREE_LAYOUT_ZORDER_H_INCLUDED

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>

namespace scm {
namespace data {

namespace detail {
} // namespace detail

// 3 dimensional z-order (Morton) space filling curve
// linear layout of octree
class quadtree_layout
{
public:
    typedef glm::uint32         index_type;
    typedef glm::uvec2          position_type;
    //typedef layout_z_order_tag  layout_type;

public:
    // counts
    index_type                  total_node_count(unsigned depth) const;
    index_type                  total_node_count_level(unsigned level) const;

    // indexing
    index_type                  root_index() const;
    unsigned                    level_index(index_type node_index) const;
    index_type                  parent_node_index(index_type node_index) const;
    index_type                  child_node_index(index_type node_index,
                                                 unsigned   child_number) const;

    // queries
    int                         is_in_sub_tree_of(index_type child_index,
                                                  index_type parent_index) const;

    // position -> index
    unsigned                    child_node_number(const position_type& position) const;

    // index -> position
    position_type               child_node_position(unsigned child_number) const;
    position_type               node_position(index_type node_index) const;
    index_type                  node_index(const position_type& node_position,
                                           unsigned             level) const;

}; // class tree_layout<layout_z_order_tag, 2>

} // namespace data
} // namespace scm

#include "quadtree_layout.inl"

#endif // SCM_LARGE_DATA_QUADTREE_LAYOUT_ZORDER_H_INCLUDED
