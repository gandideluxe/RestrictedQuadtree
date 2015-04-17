#ifndef QUADTREERENDERER_HPP
#define QUADTREERENDERER_HPP

#include "data_types_fwd.hpp"

#include <string>
#include <map>

#define GLM_FORCE_RADIANS
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#define CHILDREN 4

class QuadtreeRenderer
{
    typedef struct q_node;
    typedef q_node* q_node_ptr;

public:
    struct Vertex{
        glm::vec3 position;
        glm::vec3 color;
    };
        
    struct q_node{
        unsigned node_id;
        bool root;
        bool leaf;
        unsigned depth;
        float priority;
        float error;
        q_node_ptr parent;
        q_node_ptr child_node[CHILDREN];

        q_node(){
            root = false;
            leaf = true;
        }
    };

    struct qtree{
        q_node_ptr root_node;  
        unsigned budget;
        unsigned frame_budget;
        unsigned budget_filled;
        unsigned frame_budget_filled;
        bool strict;
    };
        
public:
    QuadtreeRenderer();
    ~QuadtreeRenderer() {}

    void add(float, glm::vec4);
    void add(unsigned, glm::vec4);

    void reset();
        
    void update_and_draw(glm::vec2 screen_pos, glm::uvec2 screen_dim);
    void update_vbo();
    void update_tree(glm::vec2 screen_pos);

private:

    void split_node(q_node_ptr n);
    void collapse_node(q_node_ptr n);
    bool splitable(q_node_ptr n);
    bool collabsible(q_node_ptr n);

    unsigned int      m_program_id;
    unsigned int      m_vao;

    qtree m_tree;

    std::vector<QuadtreeRenderer::Vertex> m_cubeVertices;

    bool              m_dirty;
};

#endif // define QUADTREERENDERER_HPP
