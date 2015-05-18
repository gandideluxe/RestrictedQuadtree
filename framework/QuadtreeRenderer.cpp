#include "QuadtreeRenderer.hpp"

#include <iostream>
#include <fstream>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include "utils.hpp"

const char* quad_vertex_shader = "\
#version 140\n\
#extension GL_ARB_shading_language_420pack : require\n\
#extension GL_ARB_explicit_attrib_location : require\n\
\n\
layout(location = 0) in vec3 position;\n\
layout(location = 1) in vec3 color;\n\
out vec3 vColor;\n\
uniform mat4 Projection;\n\
uniform mat4 Modelview;\n\
\n\
void main()\n\
{\n\
vColor = color;\n\
vec4 Position = vec4(position, 1.0);\n\
gl_Position = Projection * Modelview * Position;\n\
}\n\
";

const char* quad_fragment_shader = "\
#version 140\n\
#extension GL_ARB_shading_language_420pack : require\n\
#extension GL_ARB_explicit_attrib_location : require\n\
\n\
in vec3 vColor;\n\
layout(location = 0) out vec4 FragColor;\n\
\n\
void main()\n\
{\n\
FragColor = vec4(vColor, 1.0);\n\
}\n\
";

namespace helper {

    template<typename T>
    const T clamp(const T val, const T min, const T max)
    {
        return ((val > max) ? max : (val < min) ? min : val);
    }

    template<typename T>
    const T weight(const float w, const T a, const T b)
    {
        return ((1.0 - w) * a + w * b);
    }

} // namespace helper

QuadtreeRenderer::QuadtreeRenderer()
: m_program_id(0),
m_vao(0),
m_dirty(true)
{
    m_program_id = createProgram(quad_vertex_shader, quad_fragment_shader);

    m_tree_current = new q_tree();

    m_tree_current->budget = 2000;
    m_tree_current->budget_filled = 0;
    m_tree_current->frame_budget = 1;    
    m_tree_current->max_depth = 4;
    
    m_tree_current->root_node = new q_node();
    m_tree_current->root_node->node_id = 0;
    m_tree_current->root_node->root = true;
    m_tree_current->root_node->leaf = true;
    m_tree_current->root_node->depth = 0;
    m_tree_current->root_node->priority = 1.0;
    m_tree_current->root_node->error = 0.0;

    m_tree_current->root_node->tree = m_tree_current;
    
    m_tree_ideal = new q_tree();

    m_tree_ideal->budget = m_tree_current->budget;
    m_tree_ideal->budget_filled = 0;
    m_tree_ideal->frame_budget = 9999999;
    m_tree_ideal->max_depth = m_tree_current->max_depth;

    m_tree_ideal->root_node = new q_node();
    m_tree_ideal->root_node->node_id = 0;
    m_tree_ideal->root_node->root = true;
    m_tree_ideal->root_node->leaf = true;
    m_tree_ideal->root_node->depth = 0;
    m_tree_ideal->root_node->priority = 1.0; 
    m_tree_ideal->root_node->error = 0.0;//projection of face to POI/Camera

    m_tree_ideal->root_node->tree = m_tree_ideal;

    for (unsigned c = 0; c != CHILDREN; ++c){
        m_tree_current->root_node->child_node[c] = nullptr;
    }

    size_t max_nodes_finest_level = q_layout.total_node_count_level(m_tree_current->max_depth);
    size_t total_nodes = q_layout.total_node_count(m_tree_current->max_depth);

    std::cout << "max_nodes_finest_level " << max_nodes_finest_level << " Dimx_y: " << (size_t)glm::sqrt((float)max_nodes_finest_level) << " Total nodes:" << total_nodes << std::endl;

    m_tree_current->qtree_index_data.resize(max_nodes_finest_level, m_tree_current->root_node);
    
}

void
QuadtreeRenderer::split_node(QuadtreeRenderer::q_node_ptr n)
{
    for (unsigned c = 0; c != CHILDREN; ++c){
        n->child_node[c] = new q_node();
        n->child_node[c]->parent = n;
        n->child_node[c]->node_id = q_layout.child_node_index(n->node_id, c);
        n->child_node[c]->root = false;
        n->child_node[c]->leaf = true;
        n->child_node[c]->depth = n->depth + 1;
        n->child_node[c]->priority = n->priority * 0.25;
        n->child_node[c]->error = n->error * 0.25;
        n->child_node[c]->tree = n->tree;

        ///////setting node index image
        size_t max_nodes_finest_level = q_layout.total_node_count_level(m_tree_current->max_depth);
        auto nodes_on_lvl = q_layout.total_node_count_level(n->child_node[c]->depth);
        auto one_node_to_finest = glm::sqrt((float)max_nodes_finest_level / nodes_on_lvl);
        auto node_pos = q_layout.node_position(n->child_node[c]->node_id);
        auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);
        
        for (unsigned y = 0; y != one_node_to_finest; ++y){
            for (unsigned x = 0; x != one_node_to_finest; ++x){
                size_t index = (node_pos.x * one_node_to_finest + x) + (resolution - 1 - ((node_pos.y) * one_node_to_finest + y)) * resolution;
                m_tree_current->qtree_index_data[index] = n->child_node[c];
            }
        }
    }
#if 0
    size_t max_nodes_finest_level = q_layout.total_node_count_level(m_tree_current->max_depth);
    auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);
    
    for (unsigned y = 0; y != resolution; ++y){
        for (unsigned x = 0; x != resolution; ++x){
            auto index = (x) + (y) * resolution;
            if (m_tree_current->qtree_index_data[index]->node_id < 10)
                std::cout << " " << m_tree_current->qtree_index_data[index]->node_id << " ";
            else
                std::cout << m_tree_current->qtree_index_data[index]->node_id << " ";
        }
        std::cout<<std::endl;
    }
    std::cout << std::endl;

#endif
    n->leaf = false;
}

bool
QuadtreeRenderer::splitable(QuadtreeRenderer::q_node_ptr n){

    if (n->depth >= m_tree_current->max_depth){
        return false;
    }

    for (unsigned c = 0; c != CHILDREN; ++c){
        if (n->child_node[c] != nullptr){
            return false;
        }
    }
    return true;
}

bool
QuadtreeRenderer::collabsible(QuadtreeRenderer::q_node_ptr n){
    for (unsigned c = 0; c != CHILDREN; ++c){
        if (n->child_node[c] != nullptr && n->child_node[c]->leaf != true){
            return false;
        }
    }
    return true;
}


void
QuadtreeRenderer::collapse_node(QuadtreeRenderer::q_node_ptr n)
{
    if (!n->leaf){
        for (unsigned c = 0; c != CHILDREN; ++c){
            delete n->child_node[c];
        }
    }

    n->leaf = true;

    ///////setting node index image
    size_t max_nodes_finest_level = q_layout.total_node_count_level(m_tree_current->max_depth);
    auto nodes_on_lvl = q_layout.total_node_count_level(n->depth);
    auto one_node_to_finest = glm::sqrt((float)max_nodes_finest_level / nodes_on_lvl);
    auto node_pos = q_layout.node_position(n->node_id);
    auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);

    for (unsigned y = 0; y != one_node_to_finest; ++y){
        for (unsigned x = 0; x != one_node_to_finest; ++x){
            size_t index = (node_pos.x * one_node_to_finest + x) + (node_pos.y * one_node_to_finest + y) * resolution;
            m_tree_current->qtree_index_data[index] = n;
        }
    }

    
}

QuadtreeRenderer::q_node_ptr
QuadtreeRenderer::get_neighbour_node(const QuadtreeRenderer::q_node_ptr n, const QuadtreeRenderer::q_tree_ptr tree, const unsigned neighbour_nbr) const{
    
    size_t max_nodes_finest_level = q_layout.total_node_count_level(m_tree_current->max_depth);
    auto nodes_on_lvl = q_layout.total_node_count_level(n->depth);
    unsigned one_node_to_finest = glm::sqrt((float)max_nodes_finest_level / nodes_on_lvl);
    auto node_pos = q_layout.node_position(n->node_id);
    auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);


    auto offset_x = -1;
    auto offset_y = -1;

    switch (neighbour_nbr){
    case 0:
        offset_x = -1;
        offset_y = -1;
        break;
    case 1:
        offset_x = 0;
        offset_y = -1;
        break;
    case 2:
        offset_x = one_node_to_finest;
        offset_y = -1;
        break;
    case 3:
        offset_x = one_node_to_finest;
        offset_y = 0;
        break;
    case 4:
        offset_x = one_node_to_finest;
        offset_y = one_node_to_finest;
    case 5:
        offset_x = 0;
        offset_y = one_node_to_finest;
        break;
    case 6:
        offset_x = -1;
        offset_y = one_node_to_finest;
        break;
    case 7:
        offset_x = -1;
        offset_y = 0;
        break;
    default:
        break;
    }
    
    auto posx = node_pos.x * one_node_to_finest;
    auto posy = node_pos.y * one_node_to_finest;

    if ((posx + offset_x) < 0                             ||
        (posy + offset_y) < 0 ||
        (posx + offset_x) >= resolution                    ||
        (posy + offset_y) >= resolution)
    {
        return nullptr;
    }

    unsigned index = (posx + offset_x) + (resolution - 1 - (posy + offset_y)) * resolution;

    //if (tree->qtree_index_data[index])
    //    std::cout << "Neighbor: " << neighbour_nbr << " Pos: " << posx + offset_x << " , " << node_pos.y * one_node_to_finest - offset_y << " d: " << n->depth << " dn: " << tree->qtree_index_data[index]->depth << " ID: " << n->node_id << std::endl;

    if (tree->qtree_index_data[index] == n){
        std::cout << "Danger Dragons!" << std::endl;
        assert(0);
    }
       
    return tree->qtree_index_data[index];
}

bool
QuadtreeRenderer::check_neighbours_for_level_div(const QuadtreeRenderer::q_node_ptr n, const float lvl_diff) const
{            
    //std::cout << "!" << std::endl;

    assert(n);

    for (unsigned n_nbr = 0; n_nbr != NEIGHBORS; ++n_nbr){
        auto neighbor = get_neighbour_node(n, m_tree_current, n_nbr);
        
        if (neighbor)
        if (((int)n->depth - (int)neighbor->depth) >= lvl_diff){
                return false;
            }
    }
    
    return true;
}

void
QuadtreeRenderer::update_priorits(QuadtreeRenderer::q_tree_ptr tree, glm::vec2 screen_pos){

    std::stack<q_node_ptr> node_stack;

    node_stack.push(m_tree_current->root_node);

    q_node_ptr current_node;

    std::vector<q_node_ptr> leafs;

    while (!node_stack.empty()){
        current_node = node_stack.top();
        node_stack.pop();

        if (!current_node->leaf){
            for (unsigned c = 0; c != CHILDREN; ++c){
                if (current_node->child_node[c]){
                    node_stack.push(current_node->child_node[c]);
                }
            }
        }
        else{
            leafs.push_back(current_node);
        }
    }        

    for (auto l = leafs.begin(); l != leafs.end(); ++l){
        
        auto pos = q_layout.node_position((*l)->node_id);
        auto node_level = q_layout.level_index((*l)->node_id);
        auto total_node_index = q_layout.total_node_count(node_level);

        auto max_pos = q_layout.node_position(total_node_index - 1);

        //std::cout << "NodeId: " << (*l)->node_id << " px: " << (float)pos.x / (max_pos.x + 1.0) << " py: " << (float)pos.y / (max_pos.y + 1.0) << " level: " << q_layout.level_index((*l)->node_id) << std::endl;

        auto v_pos = glm::vec2((float)pos.x / (max_pos.x + 1.0), (float)pos.y / (max_pos.y + 1.0));
        auto v_length = 1.0 / (max_pos.x + 1);

        auto ref_pos = glm::vec2(v_pos.x + v_length * 0.5f, v_pos.y + v_length * 0.5f);
        
        auto distance = 1.0f/glm::length(ref_pos - screen_pos);

        auto depth = (*l)->depth;

        auto prio = distance / glm::pow((float)depth, 2.0f);

        (*l)->priority = prio;
    }
}

void
QuadtreeRenderer::update_tree(glm::vec2 screen_pos){
    //std::cout << "?" << std::endl;
    std::vector<q_node_ptr> split_able_nodes;
    std::vector<q_node_ptr> coll_able_nodes;

    std::stack<q_node_ptr> node_stack;

    node_stack.push(m_tree_current->root_node);

    q_node_ptr current_node;

    update_priorits(m_tree_current, screen_pos);

    while (!node_stack.empty()){
        current_node = node_stack.top();
        node_stack.pop();

        if (!current_node->leaf){
            for (unsigned c = 0; c != CHILDREN; ++c){
                if (current_node->child_node[c]){
                    node_stack.push(current_node->child_node[c]);
                }
            }
        }
        else{
            if (splitable(current_node))
                split_able_nodes.push_back(current_node);
        }

        if (collabsible(current_node)){
            coll_able_nodes.push_back(current_node);
        }
    }
        
    std::priority_queue<q_node_ptr, std::vector<q_node_ptr>, lesser_prio_ptr> split_able_nodes_pq(split_able_nodes.begin(), split_able_nodes.end());
    std::priority_queue<q_node_ptr, std::vector<q_node_ptr>, greater_prio_ptr> coll_able_nodes_pq(coll_able_nodes.begin(), coll_able_nodes.end());

    unsigned current_budget = 0;
        
    while (!split_able_nodes_pq.empty()
        && current_budget < m_tree_current->frame_budget 
        && m_tree_current->budget_filled < m_tree_current->budget){

        q_node_ptr q_split_node = split_able_nodes_pq.top();
        split_able_nodes_pq.pop();
        
        if (check_neighbours_for_level_div(q_split_node, 1.0f)){
            split_node(q_split_node);
//            std::cout << "split" << std::endl;
            ++current_budget;
            m_tree_current->budget_filled += CHILDREN;
        }

    }

}

void
QuadtreeRenderer::reset(){
    m_dirty = true;
}

void
QuadtreeRenderer::update_vbo(glm::vec2 screen_pos){

    std::stack<q_node_ptr> node_stack;

    node_stack.push(m_tree_current->root_node);

    q_node_ptr current_node;

    std::vector<q_node_ptr> leafs;

    while (!node_stack.empty()){
        current_node = node_stack.top();
        node_stack.pop();

        if (!current_node->leaf){
            for (unsigned c = 0; c != CHILDREN; ++c){
                if (current_node->child_node[c]){
                    node_stack.push(current_node->child_node[c]);
                }
            }
        }
        else{
            leafs.push_back(current_node);
        }
    }

    m_cubeVertices.clear();
    //std::cout << std::endl;
    unsigned counter = 0u;
#if 1
    for (auto l = leafs.begin(); l != leafs.end(); ++l){
        ++counter;

        auto pos = q_layout.node_position((*l)->node_id);
        auto node_level = q_layout.level_index((*l)->node_id);
        auto total_node_index = q_layout.total_node_count(node_level);
                
        auto max_pos = q_layout.node_position(total_node_index - 1);
        
        //std::cout << "NodeId: " << (*l)->node_id << " px: " << (float)pos.x / (max_pos.x + 1.0) << " py: " << (float)pos.y / (max_pos.y + 1.0) << " level: " << q_layout.level_index((*l)->node_id) << std::endl;

        auto v_pos = glm::vec2((float)pos.x / (max_pos.x + 1.0), (float)pos.y / (max_pos.y + 1.0));
        auto v_length = 1.0 / (max_pos.x + 1);

        QuadtreeRenderer::Vertex v_1b;
        QuadtreeRenderer::Vertex v_2b;
        QuadtreeRenderer::Vertex v_3b;
        QuadtreeRenderer::Vertex v_4b;
    
        v_1b.position = glm::vec3(v_pos.x, v_pos.y, 0.0f);
        v_1b.color = glm::vec3(1.0f, 1.0f, 1.0f);

        v_2b.position = glm::vec3(v_pos.x + v_length, v_pos.y, 0.0f);
        v_2b.color = glm::vec3(1.0f, 1.0f, 1.0f);

        v_3b.position = glm::vec3(v_pos.x + v_length, v_pos.y + v_length, 0.0f);
        v_3b.color = glm::vec3(1.0f, 1.0f, 1.0f);

        v_4b.position = glm::vec3(v_pos.x, v_pos.y + v_length, 0.0f);
        v_4b.color = glm::vec3(1.0f, 1.0f, 1.0f);

        m_cubeVertices.push_back(v_1b);
        m_cubeVertices.push_back(v_2b);

        m_cubeVertices.push_back(v_2b);
        m_cubeVertices.push_back(v_3b);

        m_cubeVertices.push_back(v_3b);
        m_cubeVertices.push_back(v_4b);

        m_cubeVertices.push_back(v_4b);
        m_cubeVertices.push_back(v_1b);
    }

#else

    QuadtreeRenderer::Vertex v_1b;
    QuadtreeRenderer::Vertex v_2b;
    QuadtreeRenderer::Vertex v_3b;
    QuadtreeRenderer::Vertex v_4b;

    v_1b.position = glm::vec3(0.0, 0.0, 0.0f);
    v_1b.color = glm::vec3(1.0f, 1.0f, 1.0f);

    v_2b.position = glm::vec3(1.0, 0.0, 0.0f);
    v_2b.color = glm::vec3(1.0f, 1.0f, 1.0f);

    v_3b.position = glm::vec3(1.0, 1.0, 0.0f);
    v_3b.color = glm::vec3(1.0f, 1.0f, 1.0f);

    v_4b.position = glm::vec3(0.0, 1.0, 0.0f);
    v_4b.color = glm::vec3(1.0f, 1.0f, 1.0f);

    m_cubeVertices.push_back(v_1b);
    m_cubeVertices.push_back(v_2b);

    m_cubeVertices.push_back(v_2b);
    m_cubeVertices.push_back(v_3b);

    m_cubeVertices.push_back(v_3b);
    m_cubeVertices.push_back(v_4b);

    m_cubeVertices.push_back(v_4b);
    m_cubeVertices.push_back(v_1b);


#endif

    QuadtreeRenderer::Vertex v_1b;
    QuadtreeRenderer::Vertex v_2b;
    QuadtreeRenderer::Vertex v_3b;

    v_1b.position = glm::vec3(screen_pos.x, screen_pos.y + 0.01f, 0.0f);
    v_1b.color = glm::vec3(1.0f, 0.0f, 0.0f);

    v_2b.position = glm::vec3(screen_pos.x - 0.01f, screen_pos.y - 0.01f, 0.0f);
    v_2b.color = glm::vec3(1.0f, 0.0f, 0.0f);

    v_3b.position = glm::vec3(screen_pos.x + 0.01f, screen_pos.y - 0.01f, 0.0f);
    v_3b.color = glm::vec3(1.0f, 0.0f, 0.0f);
    
    std::vector<QuadtreeRenderer::Vertex> pVertices;
    pVertices.push_back(v_1b);
    pVertices.push_back(v_2b);
    pVertices.push_back(v_3b);

    unsigned int i = 0;

    if (m_vao)
        glDeleteBuffers(1, &m_vao);

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 6 * m_cubeVertices.size()
        , m_cubeVertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_TRUE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glBindVertexArray(0);

    if (m_vao_p)
        glDeleteBuffers(1, &m_vao_p);

    glGenVertexArrays(1, &m_vao_p);
    glBindVertexArray(m_vao_p);
    
    GLuint vbo_p;
    glGenBuffers(1, &vbo_p);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_p);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 6 * pVertices.size()
        , pVertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_TRUE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glBindVertexArray(0);

}



void QuadtreeRenderer::update_and_draw(glm::vec2 screen_pos, glm::uvec2 screen_dim)
{


    float ratio = (float)screen_dim.x / screen_dim.y;
    float ratio2 = (float)screen_dim.y / screen_dim.x;

    glm::mat4 model = glm::translate(glm::vec3(0.25f, 0.15f, 0.0f))* glm::scale(glm::vec3(0.4f, 0.4f * ratio, 1.0));
    glm::mat4 view = model * glm::mat4();
    glm::mat4 projection = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f);
        
    glm::vec4 screen_pos_trans = glm::inverse(model) * glm::vec4(screen_pos, 0.0f, 1.0f);

    //std::cout << std::endl
    //std::cout << screen_pos.x << " " << screen_pos.y << std::endl;
    //std::cout << screen_pos_trans.x << " " << screen_pos_trans.y << std::endl;
    
    update_tree(glm::vec2(screen_pos_trans.x, screen_pos_trans.y));
    //update_tree(screen_pos);
    update_vbo(screen_pos);
        
    
    glUseProgram(m_program_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(view));

    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINES, 0, m_cubeVertices.size());
    glBindVertexArray(0);

    glUseProgram(0);


    glUseProgram(m_program_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(glm::mat4()));

    glBindVertexArray(m_vao_p);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glUseProgram(0);
    
}
