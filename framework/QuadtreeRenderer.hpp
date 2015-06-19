#ifndef QUADTREERENDERER_HPP
#define QUADTREERENDERER_HPP

#include "data_types_fwd.hpp"

#include <vector>
#include <stack>
#include <queue>
#include <algorithm>
#include <functional>
#include <memory>

#include <quadtree_layout.h>

#include <string>
#include <map>

#define GLM_FORCE_RADIANS
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#define CHILDREN 4
#define NEIGHBORS 8

class QuadtreeRenderer
{
    class q_node;
    class q_tree;
    typedef q_node* q_node_ptr;
    typedef q_tree* q_tree_ptr;

public:
    struct TreeInfo{
        unsigned max_budget;
        unsigned used_budget;

        size_t memory_usage;

        float puffer_size;

        unsigned max_depth;
        
        size_t time_new_tree_update;
        size_t time_current_tree_update;

        glm::uvec2 page_dim;
        glm::uvec2 ref_dim;

        float global_error;
        float global_error_difference;
    
    } m_treeInfo;

    struct Vertex{
        glm::vec3 position;
        glm::vec3 color;
    };
        
    class q_node{

    public:
        ~q_node(){
            for (unsigned c = 0; c != CHILDREN; ++c){
                if (child_node[c]){
                    delete child_node[c];
                }
            }
        };

        unsigned node_id;        
        bool leaf;
        unsigned depth;
        float importance;
        float error;
        float priority;         
        q_node_ptr parent;
        q_node_ptr child_node[CHILDREN];

        q_tree_ptr tree;

        q_node(){            
            leaf = false;
            depth = 0;
            importance = 0.0;
            error = 0.0;
            priority = 0.0;

            parent = nullptr;

            for (unsigned c = 0; c != CHILDREN; ++c){
                child_node[c] = nullptr;
            }
        }

        bool operator<(const q_node& rhs) const { priority < rhs.priority; }
        bool operator>(const q_node& rhs) const { priority > rhs.priority; }
    };

    class q_tree{
    public:
        q_node_ptr root_node;  
        unsigned max_depth;
        unsigned budget;
        unsigned frame_budget;
        unsigned budget_filled;        
        bool strict;
        
        std::vector<q_node_ptr> qtree_index_data;
        std::vector<char> qtree_depth_data; //visulizing only
        std::vector<int> qtree_id_data; //visulizing only
    };
            
public:
    QuadtreeRenderer();
    ~QuadtreeRenderer() {}

    void reset();
        
    void set_restriction(bool restriction, glm::vec2 restriction_line[2], bool restriction_direction);
    void set_frustum(glm::vec2 camera_point, glm::vec2 restriction_line[2]);
    void set_test_point(glm::vec2 test_point);
    void update_and_draw(glm::vec2 screen_pos, glm::uvec2 screen_dim);
    
    struct greater_prio_ptr : public std::binary_function<QuadtreeRenderer::q_node_ptr,
        QuadtreeRenderer::q_node_ptr, bool>
    {
        inline bool operator()(const QuadtreeRenderer::q_node_ptr& lhs,
        const QuadtreeRenderer::q_node_ptr& rhs) const {
            return lhs->priority > rhs->priority;
        }
    }; // struct greater_prio_ptr

    struct lesser_prio_ptr: public std::binary_function<QuadtreeRenderer::q_node_ptr,
        QuadtreeRenderer::q_node_ptr, bool>
    {
        inline bool operator()(const QuadtreeRenderer::q_node_ptr& lhs,
        const QuadtreeRenderer::q_node_ptr& rhs) const {
            return lhs->priority < rhs->priority;
        }
    }; // struct greater_prio_ptr

private:

    void split_node(q_node_ptr n);
    void collapse_node(q_node_ptr n);
    bool splitable(q_node_ptr n) const;
    bool collabsible(q_node_ptr n) const;

    std::map<unsigned, q_node_ptr> get_all_current_leafs(QuadtreeRenderer::q_tree_ptr tree) const;

    q_node_ptr get_neighbor_node(const q_node_ptr n, const q_tree_ptr tree, const unsigned neighbor_nbr) const;
    std::vector<q_node_ptr> check_neighbors_for_level_div(const q_node_ptr n, const float level_div) const;
    std::vector<q_node_ptr> check_neighbors_for_split(const q_node_ptr n) const;
    std::vector<q_node_ptr> check_neighbors_for_merge(const q_node_ptr n) const;
    void generate_ideal_tree(q_tree_ptr src, q_tree_ptr dst);
    void copy_tree(q_tree_ptr src, q_tree_ptr dst);
    void collapse_negative_nodes(q_tree_ptr t);
    void optimize_tree(q_tree_ptr t);
    
    void update_vbo();
    void update_tree();
    void update_priorities(q_tree_ptr m_tree, glm::vec2 pos);

    bool check_frustrum(glm::vec2 pos) const;

    unsigned int      m_tree_resolution;

    unsigned int      m_program_id;
    unsigned int      m_program_texture_id;
    
    unsigned int      m_vao;
    unsigned int      m_vao_i;

    unsigned int      m_vao_p;
    unsigned int      m_vao_r;

    unsigned int      m_vbo;
    unsigned int      m_vbo_i;

    unsigned int      m_vbo_p;
    unsigned int      m_vbo_r;

    unsigned int      m_texture_id_current;
    unsigned int      m_texture_id_ideal;
    unsigned int      m_vao_quad;
    unsigned int      m_vbo_quad;
    
    bool              m_restriction;
    glm::vec2         m_restriction_line[2];
    glm::vec2         m_restriction_line_trans[2];
    bool              m_restriction_direction;
    
    glm::vec2         m_camera_point;
    glm::vec2         m_camera_point_trans;

    glm::vec2         m_frustrum_points[2];
    glm::vec2         m_frustrum_points_trans[2];

    glm::vec2         m_test_point;
    glm::vec2         m_test_point_trans;

    glm::mat4         m_model;
    glm::mat4         m_model_inverse;
    
    q_tree_ptr m_tree_ideal;
    q_tree_ptr m_tree_current;

    std::vector<QuadtreeRenderer::Vertex> m_cubeVertices;
    std::vector<QuadtreeRenderer::Vertex> m_cubeVertices_i;
    std::vector<glm::vec3> m_quadVertices;

    bool              m_dirty;

    scm::data::quadtree_layout q_layout;
};


#endif // define QUADTREERENDERER_HPP
