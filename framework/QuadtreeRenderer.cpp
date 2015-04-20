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
m_dirty(true),
m_tree()
{
    m_program_id = createProgram(quad_vertex_shader, quad_fragment_shader);

    m_tree.budget = 10;
    m_tree.budget_filled = 0;
    m_tree.frame_budget = 1;
    m_tree.frame_budget_filled = 0;
    m_tree.max_depth = 1;
    
    m_tree.root_node = new q_node();
    m_tree.root_node->node_id = 0;
    m_tree.root_node->root = true;
    m_tree.root_node->leaf = true;
    m_tree.root_node->depth = 0;
    m_tree.root_node->priority = 1.0;
    m_tree.root_node->error = 0.0;
    
    for (unsigned c = 0; c != CHILDREN; ++c){
        m_tree.root_node->child_node[c] = nullptr;
    }
}

void QuadtreeRenderer::add(float data_value, glm::vec4 color)
{
    add((unsigned)(data_value * 255.0), color);
}

void
QuadtreeRenderer::add(unsigned data_value, glm::vec4 color)
{
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
        n->child_node[c]->depth = m_tree.root_node->depth + 1;
        n->child_node[c]->priority = m_tree.root_node->priority * 0.25;
        n->child_node[c]->error = m_tree.root_node->error * 0.25;
    }

    n->leaf = false;

    m_tree.budget_filled += CHILDREN;
    m_tree.frame_budget += 1;
}

bool
QuadtreeRenderer::splitable(QuadtreeRenderer::q_node_ptr n){

    if (n->depth >= m_tree.max_depth){
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
    if (n->leaf){
        for (unsigned c = 0; c != CHILDREN; ++c){
            delete n->child_node[c];

        }
    }

    m_tree.budget_filled -= CHILDREN;    
}


void
QuadtreeRenderer::update_tree(glm::vec2 screen_pos){

    std::vector<q_node_ptr> split_able_nodes;
    std::vector<q_node_ptr> coll_able_nodes;

    std::stack<q_node_ptr> node_stack;

    node_stack.push(m_tree.root_node);

    q_node_ptr current_node;

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
        && current_budget < m_tree.frame_budget 
        && m_tree.budget_filled < m_tree.budget){

        q_node_ptr q_split_node = split_able_nodes_pq.top();
        split_able_nodes_pq.pop();

        split_node(q_split_node);

        current_budget += CHILDREN;
        m_tree.budget_filled += CHILDREN;
    }
}

void
QuadtreeRenderer::reset(){
    m_dirty = true;
}

void
QuadtreeRenderer::update_vbo(){

    std::stack<q_node_ptr> node_stack;

    node_stack.push(m_tree.root_node);

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

    for (auto l = leafs.begin(); l != leafs.end(); ++l){

        auto pos = q_layout.node_position((*l)->node_id);
        auto node_level = q_layout.level_index((*l)->node_id);
        auto total_node_index = q_layout.total_node_count(node_level);
                
        auto max_pos = q_layout.node_position(total_node_index - 1);
        
        std::cout << "NodeId: " << (*l)->node_id << " px: " << (float)pos.x / (max_pos.x + 1.0) << " py: " << (float)pos.y / (max_pos.y + 1.0) << " level: " << q_layout.level_index((*l)->node_id) << std::endl;

        auto v_pos = glm::vec2((float)pos.x / (max_pos.x + 1.0), (float)pos.y / (max_pos.y + 1.0));
        auto v_length = 1.0 / max_pos.x;

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
        m_cubeVertices.push_back(v_3b);
        m_cubeVertices.push_back(v_4b);
        m_cubeVertices.push_back(v_1b);
    }

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

}



void QuadtreeRenderer::update_and_draw(glm::vec2 screen_pos, glm::uvec2 screen_dim)
{
    update_tree(screen_pos);
    update_vbo();


    float ratio = (float)screen_dim.x / screen_dim.y;

    glm::mat4 projection = glm::ortho(-0.1f, 1.1f, -0.1f, ratio);
    glm::mat4 view = glm::mat4();

    glUseProgram(m_program_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(view));

    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINE_STRIP, 0, m_cubeVertices.size());
    glBindVertexArray(0);

    glUseProgram(0);
    
}
