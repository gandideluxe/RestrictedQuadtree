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

const char* quad_vertex_texture_shader = "\
#version 140\n\
#extension GL_ARB_shading_language_420pack : require\n\
#extension GL_ARB_explicit_attrib_location : require\n\
\n\
layout(location = 0) in vec3 position;\n\
out vec2 vtexturePos;\n\
uniform mat4 Projection;\n\
uniform mat4 Modelview;\n\
\n\
void main()\n\
{\n\
vtexturePos = position.xy;\n\
vec4 Position = vec4(position, 1.0);\n\
gl_Position = Projection * Modelview * Position;\n\
}\n\
";

const char* quad_fragment_texture_shader = "\
#version 140\n\
#extension GL_ARB_shading_language_420pack : require\n\
#extension GL_ARB_explicit_attrib_location : require\n\
\n\
uniform sampler2D Texture;\n\
in vec2 vtexturePos;\n\
layout(location = 0) out vec4 FragColor;\n\
\n\
void main()\n\
{\n\
FragColor = texture( Texture, vec2(vtexturePos.x, 1.0 - vtexturePos.y)) * 10.0;\n\
FragColor.a = 1.0;\n\
//FragColor = vec4(vtexturePos.x, 1.0 - vtexturePos.y, 0.0, 1.0);\n\
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

    ///////////////////////////////////////////////////////////////////////
    float Gamma = 0.80;
    float IntensityMax = 255.0;

    ///////////////////////////////////////////////////////////////////////
    float round(float d){
        return floor(d + 0.5);
    }

    ///////////////////////////////////////////////////////////////////////
    float Adjust(float Color, float Factor){
        if (Color == 0.0){
            return 0.0;
        }
        else{
            float res = round(IntensityMax * pow(Color * Factor, Gamma));
            return glm::min(255.0f, glm::max(0.0f, res));
        }
    }

    ///////////////////////////////////////////////////////////////////////
    glm::vec3 WavelengthToRGB(float Wavelength)
    {
        float Blue;
        float factor;
        float Green;
        float Red;

        if (380.0 <= Wavelength && Wavelength <= 440.0){
            Red = -(Wavelength - 440.0) / (440.0 - 380.0);
            Green = 0.0;
            Blue = 1.0;
        }
        else if (440.0 < Wavelength && Wavelength <= 490.0){
            Red = 0.0;
            Green = (Wavelength - 440.0) / (490.0 - 440.0);
            Blue = 1.0;
        }
        else if (490.0 < Wavelength && Wavelength <= 510.0){
            Red = 0.0;
            Green = 1.0;
            Blue = -(Wavelength - 510.0) / (510.0 - 490.0);
        }
        else if (510.0 < Wavelength && Wavelength <= 580.0){
            Red = (Wavelength - 510.0) / (580.0 - 510.0);
            Green = 1.0;
            Blue = 0.0;
        }
        else if (580.0 < Wavelength && Wavelength <= 645.0){
            Red = 1.0;
            Green = -(Wavelength - 645.0) / (645.0 - 580.0);
            Blue = 0.0;
        }
        else if (645.0 < Wavelength && Wavelength <= 780.0){
            Red = 1.0;
            Green = 0.0;
            Blue = 0.0;
        }
        else{
            Red = 0.0;
            Green = 0.0;
            Blue = 0.0;
        }


        if (380.0 <= Wavelength && Wavelength <= 420.0){
            factor = 0.3 + 0.7*(Wavelength - 380.0) / (420.0 - 380.0);
        }
        else if (420.0 < Wavelength && Wavelength <= 701.0){
            factor = 1.0;
        }
        else if (701.0 < Wavelength && Wavelength <= 780.0){
            factor = 0.3 + 0.7*(780.0 - Wavelength) / (780.0 - 701.0);
        }
        else{
            factor = 0.0;
        }
        float R = Adjust(Red, factor);
        float G = Adjust(Green, factor);
        float B = Adjust(Blue, factor);
        return glm::vec3(R / 255.0, G / 255.0, B / 255.0);
    }

    ///////////////////////////////////////////////////////////////////////
    float GetWaveLengthFromDataPoint(float Value, float MinValue, float MaxValue)
    {
        float MinVisibleWavelength = 380.0;//350.0;
        float MaxVisibleWavelength = 780.0;//650.0;
        //Convert data value in the range of MinValues..MaxValues to the
        //range 350..780
        return (Value - MinValue) / (MaxValue - MinValue) * (MaxVisibleWavelength - MinVisibleWavelength) + MinVisibleWavelength;
    }

} // namespace helper

QuadtreeRenderer::QuadtreeRenderer()
: m_program_id(0),
m_vao(0),
m_vao_p(0),
m_vao_r(0),
m_vbo(0),
m_vbo_p(0),
m_vbo_r(0),
m_dirty(true)
{
    m_program_id = createProgram(quad_vertex_shader, quad_fragment_shader);
    m_program_texture_id = createProgram(quad_vertex_texture_shader, quad_fragment_texture_shader);

    m_tree_current = new q_tree();

    m_tree_current->budget = 5000;
    m_tree_current->budget_filled = 0;
    m_tree_current->frame_budget = 2;    
    m_tree_current->max_depth = 6;
    
    m_tree_current->root_node = new q_node();
    m_tree_current->root_node->node_id = 0;
    m_tree_current->root_node->leaf = true;
    m_tree_current->root_node->depth = 0;
    m_tree_current->root_node->priority = 0.0;
    m_tree_current->root_node->error = 0.0;

    m_tree_current->root_node->tree = m_tree_current;
    
    m_tree_ideal = new q_tree();

    m_tree_ideal->budget = m_tree_current->budget;
    m_tree_ideal->budget_filled = 0;
    m_tree_ideal->frame_budget = 9999999;
    m_tree_ideal->max_depth = m_tree_current->max_depth;

    m_tree_ideal->root_node = new q_node();
    m_tree_ideal->root_node->node_id = 0;
    m_tree_ideal->root_node->leaf = true;
    m_tree_ideal->root_node->depth = 0;
    m_tree_ideal->root_node->priority = 0.0; 
    m_tree_ideal->root_node->error = 0.0;//projection of face to POI/Camera

    m_tree_ideal->root_node->tree = m_tree_ideal;

    size_t max_nodes_finest_level = q_layout.total_node_count_level(m_tree_current->max_depth);
    size_t total_nodes = q_layout.total_node_count(m_tree_current->max_depth);

    std::cout << "max_nodes_finest_level " << max_nodes_finest_level << " Dimx_y: " << (size_t)glm::sqrt((float)max_nodes_finest_level) << " Total nodes:" << total_nodes << std::endl;

    m_tree_resolution = (unsigned)glm::sqrt((float)max_nodes_finest_level);

    m_tree_current->qtree_index_data.resize(max_nodes_finest_level, m_tree_current->root_node);
    m_tree_ideal->qtree_index_data.resize(max_nodes_finest_level, m_tree_ideal->root_node);

    m_tree_current->qtree_depth_data.resize(max_nodes_finest_level, 0);
    m_tree_ideal->qtree_depth_data.resize(max_nodes_finest_level, 0);

    m_tree_current->qtree_id_data.resize(max_nodes_finest_level, 0);
    m_tree_ideal->qtree_id_data.resize(max_nodes_finest_level, 0);

    m_treeInfo.max_budget = m_tree_current->budget;
    m_treeInfo.used_budget = 0;

    m_treeInfo.page_dim = glm::uvec2(64, 64);
    m_treeInfo.ref_dim = glm::uvec2(2560, 2560);

    m_treeInfo.global_error = (float)m_treeInfo.ref_dim.x / m_treeInfo.page_dim.x + (float)m_treeInfo.ref_dim.y / m_treeInfo.page_dim.y;
    m_treeInfo.global_error_difference = (float)m_treeInfo.ref_dim.x / m_treeInfo.page_dim.x + (float)m_treeInfo.ref_dim.y / m_treeInfo.page_dim.y;

    m_quadVertices.clear();
    m_quadVertices.push_back(glm::vec3(0.0, 0.0, 0.0));
    m_quadVertices.push_back(glm::vec3(0.0, 1.0, 0.0));
    m_quadVertices.push_back(glm::vec3(1.0, 0.0, 0.0));
    m_quadVertices.push_back(glm::vec3(1.0, 1.0, 0.0));


    glGenVertexArrays(1, &m_vao_quad);
    glBindVertexArray(m_vao_quad);

    glGenBuffers(1, &m_vbo_quad);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_quad);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 6 * m_quadVertices.size()
        , m_quadVertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    //glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);    
    glBindVertexArray(0);


#if 0
    glActiveTexture(GL_TEXTURE0);
    m_texture_id_current = createTexture2D(m_tree_resolution, m_tree_resolution, (char*)&(m_tree_current->qtree_index_data[0]), GL_RGBA, GL_UNSIGNED_BYTE);
    glActiveTexture(GL_TEXTURE1);
    m_texture_id_ideal = createTexture2D(m_tree_resolution, m_tree_resolution, (char*)&(m_tree_ideal->qtree_index_data[0]), GL_RGBA, GL_UNSIGNED_BYTE);
#elif 1
    glActiveTexture(GL_TEXTURE0);
    m_texture_id_current = createTexture2D(m_tree_resolution, m_tree_resolution, (char*)&m_tree_current->qtree_depth_data[0], GL_R8, GL_RED, GL_UNSIGNED_BYTE);
    glActiveTexture(GL_TEXTURE1);
    m_texture_id_ideal = createTexture2D(m_tree_resolution, m_tree_resolution, (char*)&m_tree_ideal->qtree_depth_data[0], GL_R8, GL_RED, GL_UNSIGNED_BYTE);
#else
    glActiveTexture(GL_TEXTURE0);
    m_texture_id_current = createTexture2D(m_tree_resolution, m_tree_resolution, (char*)&m_tree_current->qtree_id_data[0], GL_R32UI, GL_RED, GL_UNSIGNED_INT);
    glActiveTexture(GL_TEXTURE1);
    m_texture_id_ideal = createTexture2D(m_tree_resolution, m_tree_resolution, (char*)&m_tree_ideal->qtree_id_data[0], GL_R32UI, GL_RED, GL_UNSIGNED_INT);
#endif
                
}

void
QuadtreeRenderer::set_restriction(bool restriction, glm::vec2 restriction_line[2], bool restriction_direction)
{
    m_restriction = restriction;
    m_restriction_line[0] = restriction_line[0];
    m_restriction_line[1] = restriction_line[1];

    m_restriction_direction = restriction_direction;
        
}

void
QuadtreeRenderer::set_frustum(glm::vec2 camera, glm::vec2 frustum_points[2] )
{
    m_camera_point = camera;

    m_frustrum_points[0] = frustum_points[0];
    m_frustrum_points[1] = frustum_points[1];
}

void
QuadtreeRenderer::set_test_point(glm::vec2 test_point)
{    
    m_test_point = test_point;    
}


bool
QuadtreeRenderer::check_frustrum(glm::vec2 pos) const
{
    glm::vec2 p1 = m_frustrum_points[0];
    glm::vec2 p2 = m_frustrum_points[1];

    glm::vec2 c = m_camera_point;
    
    glm::vec2 min = glm::vec2(glm::min(p1.x, p2.x), glm::min(p1.y, p2.y));
    glm::vec2 max = glm::vec2(glm::max(p1.x, p2.x), glm::max(p1.y, p2.y));

    glm::vec2 n1 = c - p1;
    n1 = glm::vec2(-n1.y, n1.x);

    glm::vec2 n2 = p2 - c;
    n2 = glm::vec2(-n2.y, n2.x);

    float s1 = glm::dot(n1, pos - c);
    float s2 = glm::dot(n2, pos - c);
            
    //std::cout << "d1: " << s1 << " d2: " << s2 << std::endl;

    if (s1 > 0.0 && s2 > 0.0)
        return true;
    else
        return false;
        
//http://upload.wikimedia.org/math/c/a/e/cae82210ba48deb8ff6fb0134a90b36e.png

}


void
QuadtreeRenderer::split_node(QuadtreeRenderer::q_node_ptr n)
{
    for (unsigned c = 0; c != CHILDREN; ++c){
        n->child_node[c] = new q_node();
        n->child_node[c]->parent = n;
        n->child_node[c]->node_id = q_layout.child_node_index(n->node_id, c);
        n->child_node[c]->leaf = true;
        n->child_node[c]->depth = n->depth + 1;
        n->child_node[c]->priority = n->priority * (1.0f / CHILDREN);
        n->child_node[c]->error = n->error * (1.0f / CHILDREN);
        n->child_node[c]->tree = n->tree;

        ///////setting node index image
        size_t max_nodes_finest_level = q_layout.total_node_count_level(n->tree->max_depth);
        auto nodes_on_lvl = q_layout.total_node_count_level(n->child_node[c]->depth);
        auto one_node_to_finest = glm::sqrt((float)max_nodes_finest_level / nodes_on_lvl);
        auto node_pos = q_layout.node_position(n->child_node[c]->node_id);
        auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);
        
        for (unsigned y = 0; y != one_node_to_finest; ++y){
            for (unsigned x = 0; x != one_node_to_finest; ++x){
                size_t index = (node_pos.x * one_node_to_finest + x) + (resolution - 1 - ((node_pos.y) * one_node_to_finest + y)) * resolution;
                n->tree->qtree_index_data[index] = n->child_node[c];
                n->tree->qtree_depth_data[index] = n->child_node[c]->depth;
                n->tree->qtree_id_data[index] = n->child_node[c]->node_id;
            }
        }
    }

    n->tree->budget_filled += CHILDREN;
#if 0
    size_t max_nodes_finest_level = q_layout.total_node_count_level(n->tree->max_depth);
    auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);
    
    for (unsigned y = 0; y != resolution; ++y){
        for (unsigned x = 0; x != resolution; ++x){
            auto index = (x) + (y) * resolution;
            if (n->tree->qtree_index_data[index]->node_id < 10)
                std::cout << " " << n->tree->qtree_index_data[index]->node_id << " ";
            else
                std::cout << n->tree->qtree_index_data[index]->node_id << " ";
        }
        std::cout<<std::endl;
    }
    std::cout << std::endl;

#endif
    n->leaf = false;
}

bool
QuadtreeRenderer::splitable(QuadtreeRenderer::q_node_ptr n) const{

    if (n->depth >= n->tree->max_depth){
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
QuadtreeRenderer::collabsible(QuadtreeRenderer::q_node_ptr n) const{
    
    if (n == nullptr){
        return false;
    }

    if (n->leaf){
        return false;
    }
    
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

    assert(!n->leaf);

    //if (!n->leaf){
    for (unsigned c = 0; c != CHILDREN; ++c){
        delete n->child_node[c];
        n->child_node[c] = nullptr;
    }
        
    n->tree->budget_filled -= CHILDREN;

    n->leaf = true;

    ///////setting node index image
    size_t max_nodes_finest_level = q_layout.total_node_count_level(n->tree->max_depth);
    auto nodes_on_lvl = q_layout.total_node_count_level(n->depth);
    auto one_node_to_finest = glm::sqrt((float)max_nodes_finest_level / nodes_on_lvl);
    auto node_pos = q_layout.node_position(n->node_id);
    auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);

    for (unsigned y = 0; y != one_node_to_finest; ++y){
        for (unsigned x = 0; x != one_node_to_finest; ++x){
            size_t index = (node_pos.x * one_node_to_finest + x) + (node_pos.y * one_node_to_finest + y) * resolution;
            n->tree->qtree_index_data[index] = n;
            n->tree->qtree_depth_data[index] = n->depth;
            n->tree->qtree_id_data[index] = n->node_id;
        }
    }   

    //}
}

QuadtreeRenderer::q_node_ptr
QuadtreeRenderer::get_neighbor_node(const QuadtreeRenderer::q_node_ptr n, const QuadtreeRenderer::q_tree_ptr tree, const unsigned neighbor_nbr) const
{
    
    size_t max_nodes_finest_level = q_layout.total_node_count_level(n->tree->max_depth);
    auto nodes_on_lvl = q_layout.total_node_count_level(n->depth);
    unsigned one_node_to_finest = glm::sqrt((float)max_nodes_finest_level / nodes_on_lvl);
    auto node_pos = q_layout.node_position(n->node_id);
    auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);


    auto offset_x = -1;
    auto offset_y = -1;

    switch (neighbor_nbr){
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
    //    std::cout << "Neighbor: " << neighbor_nbr << " Pos: " << posx + offset_x << " , " << node_pos.y * one_node_to_finest - offset_y << " d: " << n->depth << " dn: " << tree->qtree_index_data[index]->depth << " ID: " << n->node_id << std::endl;

    if (tree->qtree_index_data[index] == n){
        std::cout << "Danger Dragons!" << std::endl;
        assert(0);
    }
       
    return tree->qtree_index_data[index];
}

std::vector<QuadtreeRenderer::q_node_ptr>
QuadtreeRenderer::check_neighbors_for_level_div(const QuadtreeRenderer::q_node_ptr n, const float lvl_diff) const
{            
    //std::cout << "!" << std::endl;

    assert(n);

    std::vector<q_node_ptr> p_nodes;

    for (unsigned n_nbr = 0; n_nbr != NEIGHBORS; ++n_nbr){
        auto neighbor = get_neighbor_node(n, n->tree, n_nbr);
        
        if (neighbor)
        if (((int)n->depth - (int)neighbor->depth) > lvl_diff){                
            p_nodes.push_back(neighbor);
        }
    }
    
    return p_nodes;
}


std::vector<QuadtreeRenderer::q_node_ptr>
QuadtreeRenderer::check_neighbors_for_split(const QuadtreeRenderer::q_node_ptr n) const
{
    //std::cout << "!" << std::endl;

    assert(n);

    std::vector<q_node_ptr> p_nodes;

    for (unsigned n_nbr = 0; n_nbr != NEIGHBORS; ++n_nbr){
        auto neighbor = get_neighbor_node(n, n->tree, n_nbr);

        if (neighbor)
        if (((int)n->depth - (int)neighbor->depth) == 1.0){
            p_nodes.push_back(neighbor);
        }
    }

    return p_nodes;
}

std::vector<QuadtreeRenderer::q_node_ptr>
QuadtreeRenderer::check_neighbors_for_merge(const QuadtreeRenderer::q_node_ptr n) const
{
    //std::cout << "!" << std::endl;

    assert(n);

    std::vector<q_node_ptr> p_nodes;

    for (unsigned n_nbr = 0; n_nbr != NEIGHBORS; ++n_nbr){
        auto neighbor = get_neighbor_node(n, n->tree, n_nbr);

        if (neighbor)
        if (((int)neighbor->depth) - (int)n->depth > 1){
            p_nodes.push_back(neighbor);
        }
    }

    return p_nodes;
}

void
QuadtreeRenderer::update_priorities(QuadtreeRenderer::q_tree_ptr tree, glm::vec2 screen_pos){

    std::stack<q_node_ptr> node_stack;

    node_stack.push(tree->root_node);

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
        
        auto v_pos = glm::vec2((float)pos.x / (max_pos.x + 1.0), (float)pos.y / (max_pos.y + 1.0));
        auto v_length = 1.0 / (max_pos.x + 1);

        auto ref_pos = glm::vec2(v_pos.x + v_length * 0.5f, v_pos.y + v_length * 0.5f);

        glm::vec2 vpos1 = v_pos;
        glm::vec2 vpos2 = glm::vec2(v_pos.x + v_length, v_pos.y);
        glm::vec2 vpos3 = glm::vec2(v_pos.x + v_length, v_pos.y + v_length);
        glm::vec2 vpos4 = glm::vec2(v_pos.x, v_pos.y + v_length);


        glm::vec4 trans = m_model * glm::vec4(vpos1, 0.0f, 1.0f);
        glm::vec2 trans1 = glm::vec2(trans.x, trans.y);

        trans = m_model * glm::vec4(vpos2, 0.0f, 1.0f);
        glm::vec2 trans2 = glm::vec2(trans.x, trans.y);

        trans = m_model * glm::vec4(vpos3, 0.0f, 1.0f);
        glm::vec2 trans3 = glm::vec2(trans.x, trans.y);
        
        trans = m_model * glm::vec4(vpos4, 0.0f, 1.0f);
        glm::vec2 trans4 = glm::vec2(trans.x, trans.y);

        auto depth = (*l)->depth + 1;
        auto importance = std::sqrt(1.0f / glm::length(ref_pos - screen_pos) / depth);
        (*l)->importance = importance;

        if (!(check_frustrum(trans1)
            || check_frustrum(trans2)
            || check_frustrum(trans3)
            || check_frustrum(trans4)
            )){
            
            (*l)->error = 0.0;
        }
        else{
            auto local_error = ((float)m_treeInfo.ref_dim.x / (m_treeInfo.page_dim.x * depth) + (float)m_treeInfo.ref_dim.y / (m_treeInfo.page_dim.y * depth)) * 0.5;
                    
            (*l)->error = local_error;
        }
        auto prio = (*l)->importance * (*l)->error;
        (*l)->priority = prio;
    }

    auto global_error = 0.0;
    for (auto l = leafs.begin(); l != leafs.end(); ++l){
        global_error += ((1.0 / (leafs.size() * m_treeInfo.page_dim.x * m_treeInfo.page_dim.y)) * ((*l)->error) * (*l)->importance);
    }
    
    m_treeInfo.global_error_difference = global_error - m_treeInfo.global_error;
    m_treeInfo.global_error = global_error;
}

std::map<unsigned, QuadtreeRenderer::q_node_ptr>
QuadtreeRenderer::get_all_current_leafs(QuadtreeRenderer::q_tree_ptr tree) const
{
    std::stack<q_node_ptr> node_stack;
    node_stack.push(tree->root_node);

    std::map<unsigned, q_node_ptr> leaf_node_map;
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
            leaf_node_map.insert(std::pair<unsigned, q_node_ptr>(current_node->node_id, current_node));
        }
    }

    return leaf_node_map;
}

void 
QuadtreeRenderer::copy_tree(QuadtreeRenderer::q_tree_ptr src, QuadtreeRenderer::q_tree_ptr dst){

    auto leaf_map = get_all_current_leafs(src);

    std::map<unsigned, q_node_ptr> ideal_nodes;
    typedef std::pair<unsigned, q_node_ptr> ideal_nodes_pair;

    for (auto l : leaf_map){

        auto current_node = new q_node();

        current_node->node_id = l.second->node_id;
        current_node->depth = l.second->depth;
        current_node->leaf = true;

        current_node->importance = l.second->importance;
        current_node->error = l.second->error;
        current_node->priority = l.second->priority;

        current_node->tree = dst;
        ideal_nodes.insert(ideal_nodes_pair(current_node->node_id, current_node));

        /////insert into map
        size_t max_nodes_finest_level = q_layout.total_node_count_level(src->max_depth);
        auto nodes_on_lvl = q_layout.total_node_count_level(current_node->depth);
        auto one_node_to_finest = glm::sqrt((float)max_nodes_finest_level / nodes_on_lvl);
        auto node_pos = q_layout.node_position(current_node->node_id);
        auto resolution = (size_t)glm::sqrt((float)max_nodes_finest_level);

        for (unsigned y = 0; y != one_node_to_finest; ++y){
            for (unsigned x = 0; x != one_node_to_finest; ++x){
                size_t index = (node_pos.x * one_node_to_finest + x) + (resolution - 1 - ((node_pos.y) * one_node_to_finest + y)) * resolution;
                dst->qtree_index_data[index] = current_node;
                dst->qtree_depth_data[index] = current_node->depth;
                dst->qtree_id_data[index] = current_node->node_id;
            }
        }
        ////////////////////

        while (current_node->depth != 0){
            auto parent_node_id = q_layout.parent_node_index(current_node->node_id);
            auto find_node = ideal_nodes.find(parent_node_id);

            q_node_ptr parent;

            if (find_node == ideal_nodes.end()){
                parent = new q_node();
                parent->node_id = parent_node_id;
                parent->depth = current_node->depth - 1;
                parent->tree = dst;
                ideal_nodes.insert(ideal_nodes_pair(parent->node_id, parent));
            }
            else{                
                parent = find_node->second;
                assert(parent);
            }

            parent->importance += current_node->importance;
            parent->error += current_node->error;
            parent->priority = current_node->importance * current_node->error;

            parent->child_node[q_layout.child_node_number(q_layout.node_position(current_node->node_id))] = current_node;

            current_node = parent;
            
        }

        if (current_node->depth == 0){
            dst->root_node = current_node;
        }

    }

    /////// Setting up parents
    std::stack<q_node_ptr> node_stack;
    node_stack.push(dst->root_node);

    q_node_ptr current_node;

    while (!node_stack.empty()){
        current_node = node_stack.top();
        node_stack.pop();
                
        for (unsigned c = 0; c != CHILDREN; ++c){
            if (current_node->child_node[c]){
                current_node->child_node[c]->parent = current_node;
                if (!current_node->child_node[c]->leaf)
                    node_stack.push(current_node->child_node[c]);
            }
        }        
    }
        
    dst->budget_filled = dst->budget_filled;
    dst->strict = true;

}

void
QuadtreeRenderer::collapse_negative_nodes(QuadtreeRenderer::q_tree_ptr t){
    
    std::stack<q_node_ptr> node_stack;
    std::stack<q_node_ptr> collapsible_node_stack;
    std::stack<q_node_ptr> negative_node_stack;
    node_stack.push(t->root_node);
    
    q_node_ptr current_node;
    
    while (!node_stack.empty()){
        current_node = node_stack.top();
        node_stack.pop();

        if (!collabsible(current_node)){
            for (unsigned c = 0; c != CHILDREN; ++c){
                if (current_node->child_node[c]){
                    node_stack.push(current_node->child_node[c]);
                }
            }
        }
        else{    
            collapsible_node_stack.push(current_node);
        }
    }

    while (!collapsible_node_stack.empty()){

        auto current_canidate = collapsible_node_stack.top();
        collapsible_node_stack.pop();
            
        if (current_canidate->priority < 0.3){
            if (collabsible(current_canidate)){
                auto p_nodes = check_neighbors_for_merge(current_canidate);
                if (p_nodes.empty()){
                    collapse_node(current_canidate);
                    for (unsigned c = 0; c != CHILDREN; ++c){
                        assert(current_canidate->parent || current_canidate->depth == 0);
                        if (collabsible(current_canidate->parent)){
                            collapsible_node_stack.push(current_canidate->parent);
                        }
                    }
                }
            }
        }
    }
}

void
QuadtreeRenderer::optimize_tree(QuadtreeRenderer::q_tree_ptr t){

    std::vector<q_node_ptr> split_able_nodes;
    std::vector<q_node_ptr> colap_able_nodes;
    
    std::stack<q_node_ptr> node_stack;

    node_stack.push(m_tree_current->root_node);

    q_node_ptr current_node;

    update_priorities(m_tree_current, m_camera_point_trans);

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
    }

    std::priority_queue<q_node_ptr, std::vector<q_node_ptr>, lesser_prio_ptr> split_able_nodes_pq(split_able_nodes.begin(), split_able_nodes.end());
        
    while (!node_stack.empty()){
        current_node = node_stack.top();
        node_stack.pop();

        if (!collabsible(current_node)){
            for (unsigned c = 0; c != CHILDREN; ++c){
                if (current_node->child_node[c]){
                    node_stack.push(current_node->child_node[c]);
                }
            }
        }
        else{
            colap_able_nodes.push_back(current_node);
        }
    }

    std::priority_queue<q_node_ptr, std::vector<q_node_ptr>, lesser_prio_ptr> colap_able_nodes_pq(colap_able_nodes.begin(), colap_able_nodes.end());
    
    while (!split_able_nodes_pq.empty()        
        && m_tree_current->budget_filled < m_tree_current->budget){

        q_node_ptr q_split_node = split_able_nodes_pq.top();
        split_able_nodes_pq.pop();

        auto p_nodes = check_neighbors_for_split(q_split_node);
        std::priority_queue<q_node_ptr, std::vector<q_node_ptr>, lesser_prio_ptr> split_able_nodes_forced_pq(p_nodes.begin(), p_nodes.end());

        if (p_nodes.empty()
            && q_split_node->error > 0.4){
            split_node(q_split_node);            
        }
        else{
            if (!split_able_nodes_forced_pq.empty()){
                while (!split_able_nodes_forced_pq.empty()
                    && m_tree_current->budget_filled < m_tree_current->budget){
                    
                    q_node_ptr q_split_node = split_able_nodes_forced_pq.top();
                    
                    auto pn_nodes = check_neighbors_for_split(q_split_node);

                    auto stack_size = split_able_nodes_forced_pq.size();

                    for (auto n : pn_nodes){
                        split_able_nodes_forced_pq.push(n);
                    }

                    if (stack_size == split_able_nodes_forced_pq.size()){
                        split_able_nodes_forced_pq.pop();
                        split_node(q_split_node);
                    }
                }
            }
        }
    }
    
    //while (!split_able_nodes_pq.empty()
    //    && !colap_able_nodes_pq.empty()
    //    && colap_able_nodes_pq.top()->priority < split_able_nodes_pq.top()->priority){

    //    q_node_ptr q_split_node = split_able_nodes_pq.top();        
    //    q_node_ptr q_colap_node = colap_able_nodes_pq.top();
    //    colap_able_nodes_pq.pop();

    //    if (check_neighbors_for_level_div(q_colap_node, 0.0f)){

    //        collapse_node(q_colap_node);
    //        
    //        for (unsigned c = 0; c != CHILDREN; ++c){
    //            if (collabsible(q_colap_node->parent->child_node[c])){
    //                colap_able_nodes_pq.emplace(q_colap_node->parent->child_node[c]);
    //            }
    //        }

    //        if (check_neighbors_for_level_div(q_colap_node, 1.0f)
    //            && q_split_node->error > 0.4){

    //            split_node(q_split_node);

    //            split_able_nodes_pq.pop();
    //        }            

    //    }


    //}
}

void
QuadtreeRenderer::generate_ideal_tree(QuadtreeRenderer::q_tree_ptr src, QuadtreeRenderer::q_tree_ptr dst){
    
    copy_tree(src, dst);

    collapse_negative_nodes(dst);

    optimize_tree(dst);
    
}


void
QuadtreeRenderer::update_tree(){

    delete m_tree_ideal->root_node;
    
    generate_ideal_tree(m_tree_current, m_tree_ideal);

    m_tree_ideal->budget = m_tree_current->budget;
    m_tree_ideal->budget_filled = m_tree_current->budget_filled;    
    
    //std::cout << "?" << std::endl;
    std::vector<q_node_ptr> split_able_nodes;
    std::vector<q_node_ptr> coll_able_nodes;

    std::stack<q_node_ptr> node_stack;

    node_stack.push(m_tree_current->root_node);

    q_node_ptr current_node;

    update_priorities(m_tree_current, m_camera_point_trans);

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
    }
        
    std::priority_queue<q_node_ptr, std::vector<q_node_ptr>, lesser_prio_ptr> split_able_nodes_pq(split_able_nodes.begin(), split_able_nodes.end());
    std::priority_queue<q_node_ptr, std::vector<q_node_ptr>, greater_prio_ptr> coll_able_nodes_pq(coll_able_nodes.begin(), coll_able_nodes.end());

    unsigned current_budget = 0;
        
    //while (!split_able_nodes_pq.empty()
    //    && current_budget < m_tree_current->frame_budget 
    //    && m_tree_current->budget_filled < m_tree_current->budget){

    //    q_node_ptr q_split_node = split_able_nodes_pq.top();
    //    split_able_nodes_pq.pop();
    //    
    //    if (check_neighbors_for_level_div(q_split_node, 1.0f)
    //        && q_split_node->error > 0.8){

    //        split_node(q_split_node);
    //        ++current_budget;            
    //    }
    //}

    m_treeInfo.used_budget = m_tree_current->budget_filled;

}

void
QuadtreeRenderer::reset(){
    m_dirty = true;
}

void
QuadtreeRenderer::update_vbo(){
    
    {
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

            float error = (*l)->error;
            //std::cout << error << std::endl;

            glm::vec4 trans = m_model * glm::vec4(v_pos, 0.0f, 1.0f);
            glm::vec2 trans2 = glm::vec2(trans.x, trans.y);

            glm::vec3 color = helper::WavelengthToRGB(helper::GetWaveLengthFromDataPoint(error, 0.0f, 10.5f));// glm::vec3(0.0f, 1.0f, 0.0f);
            //color = glm::vec3(1.0f, 0.0f, 0.0f);
            //if (!check_frustrum(glm::vec2(v_pos.x, v_pos.y)))
            //if (!check_frustrum(trans2))
            //color = glm::vec3(1.0f, 0.0f, 0.0f);            
            v_1b.position = glm::vec3(v_pos.x, v_pos.y, 0.0f);
            v_1b.color = color;
            //color = glm::vec3(0.0f, 1.0f, 0.0f);

            trans = m_model * glm::vec4(v_pos.x + v_length, v_pos.y, 0.0f, 1.0f);
            trans2 = glm::vec2(trans.x, trans.y);
            //if (!check_frustrum(trans2))
            //color = glm::vec3(1.0f, 0.0f, 0.0f);
            v_2b.position = glm::vec3(v_pos.x + v_length, v_pos.y, 0.0f);
            v_2b.color = color;
            //color = glm::vec3(0.0f, 1.0f, 0.0f);

            trans = m_model * glm::vec4(v_pos.x + v_length, v_pos.y + v_length, 0.0f, 1.0f);
            trans2 = glm::vec2(trans.x, trans.y);
            //if (!check_frustrum(trans2))
            //color = glm::vec3(1.0f, 0.0f, 0.0f);
            v_3b.position = glm::vec3(v_pos.x + v_length, v_pos.y + v_length, 0.0f);
            v_3b.color = color;
            //color = glm::vec3(0.0f, 1.0f, 0.0f);

            trans = m_model * glm::vec4(v_pos.x, v_pos.y + v_length, 0.0f, 1.0f);
            trans2 = glm::vec2(trans.x, trans.y);
            //if (!check_frustrum(trans2))
            // color = glm::vec3(1.0f, 0.0f, 0.0f);
            v_4b.position = glm::vec3(v_pos.x, v_pos.y + v_length, 0.0f);
            v_4b.color = color;


            m_cubeVertices.push_back(v_1b);
            m_cubeVertices.push_back(v_2b);

            m_cubeVertices.push_back(v_2b);
            m_cubeVertices.push_back(v_3b);

            m_cubeVertices.push_back(v_3b);
            m_cubeVertices.push_back(v_4b);

            m_cubeVertices.push_back(v_4b);
            m_cubeVertices.push_back(v_1b);
        }

    }

    {
        std::stack<q_node_ptr> node_stack;

        node_stack.push(m_tree_ideal->root_node);
    
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

        m_cubeVertices_i.clear();
        //std::cout << std::endl;
        unsigned counter = 0u;

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

            float error = (*l)->error;
            //std::cout << error << std::endl;

            glm::vec4 trans = m_model * glm::vec4(v_pos, 0.0f, 1.0f);
            glm::vec2 trans2 = glm::vec2(trans.x, trans.y);

            glm::vec3 color = helper::WavelengthToRGB(helper::GetWaveLengthFromDataPoint(error, 0.0f, 10.5f));// glm::vec3(0.0f, 1.0f, 0.0f);
            //color = glm::vec3(1.0f, 0.0f, 0.0f);
            //if (!check_frustrum(glm::vec2(v_pos.x, v_pos.y)))
            //if (!check_frustrum(trans2))
            //color = glm::vec3(1.0f, 0.0f, 0.0f);            
            v_1b.position = glm::vec3(v_pos.x, v_pos.y, 0.0f);
            v_1b.color = color;
            //color = glm::vec3(0.0f, 1.0f, 0.0f);

            trans = m_model * glm::vec4(v_pos.x + v_length, v_pos.y, 0.0f, 1.0f);
            trans2 = glm::vec2(trans.x, trans.y);
            //if (!check_frustrum(trans2))
            //color = glm::vec3(1.0f, 0.0f, 0.0f);
            v_2b.position = glm::vec3(v_pos.x + v_length, v_pos.y, 0.0f);
            v_2b.color = color;
            //color = glm::vec3(0.0f, 1.0f, 0.0f);

            trans = m_model * glm::vec4(v_pos.x + v_length, v_pos.y + v_length, 0.0f, 1.0f);
            trans2 = glm::vec2(trans.x, trans.y);
            //if (!check_frustrum(trans2))
            //color = glm::vec3(1.0f, 0.0f, 0.0f);
            v_3b.position = glm::vec3(v_pos.x + v_length, v_pos.y + v_length, 0.0f);
            v_3b.color = color;
            //color = glm::vec3(0.0f, 1.0f, 0.0f);

            trans = m_model * glm::vec4(v_pos.x, v_pos.y + v_length, 0.0f, 1.0f);
            trans2 = glm::vec2(trans.x, trans.y);
            //if (!check_frustrum(trans2))
            // color = glm::vec3(1.0f, 0.0f, 0.0f);
            v_4b.position = glm::vec3(v_pos.x, v_pos.y + v_length, 0.0f);
            v_4b.color = color;


            m_cubeVertices_i.push_back(v_1b);
            m_cubeVertices_i.push_back(v_2b);

            m_cubeVertices_i.push_back(v_2b);
            m_cubeVertices_i.push_back(v_3b);

            m_cubeVertices_i.push_back(v_3b);
            m_cubeVertices_i.push_back(v_4b);
                          
            m_cubeVertices_i.push_back(v_4b);
            m_cubeVertices_i.push_back(v_1b);
        }
    }
    std::vector<QuadtreeRenderer::Vertex> pVertices;

    {
        QuadtreeRenderer::Vertex v_1b;
        QuadtreeRenderer::Vertex v_2b;
        QuadtreeRenderer::Vertex v_3b;
        QuadtreeRenderer::Vertex v_4b;
        QuadtreeRenderer::Vertex v_5b;
        QuadtreeRenderer::Vertex v_6b;

        v_1b.position = glm::vec3(m_camera_point_trans.x, m_camera_point_trans.y, 0.0f);
        v_1b.color = glm::vec3(1.0f, 0.0f, 0.0f);
        pVertices.push_back(v_1b);


        glm::vec3 color = glm::vec3(0.0f, 1.0f, 0.0f);
        if (m_restriction_direction)
            color = glm::vec3(0.0f, 0.0f, 1.0f);
        if (!m_restriction)
            color = glm::vec3(0.2f, 0.2f, 0.2f);

        v_2b.position = glm::vec3(m_restriction_line_trans[0].x, m_restriction_line_trans[0].y, 0.0f);
        v_2b.color = color;

        v_3b.position = glm::vec3(m_restriction_line_trans[1].x, m_restriction_line_trans[1].y, 0.0f);
        v_3b.color = color;

        pVertices.push_back(v_2b);
        pVertices.push_back(v_3b);

        color = glm::vec3(1.0f, 1.0f, 0.0f);

        v_4b.position = glm::vec3(m_frustrum_points_trans[0].x, m_frustrum_points_trans[0].y, 0.0f);
        v_4b.color = color;

        v_5b.position = glm::vec3(m_frustrum_points_trans[1].x, m_frustrum_points_trans[1].y, 0.0f);
        v_5b.color = color;

        pVertices.push_back(v_4b);
        pVertices.push_back(v_5b);

        if (check_frustrum(m_test_point)){
            color = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        else{
            color = glm::vec3(1.0f, 0.0f, 0.0f);
        }

        v_6b.position = glm::vec3(m_test_point_trans.x, m_test_point_trans.y, 0.0f);;
        v_6b.color = color;

        pVertices.push_back(v_6b);

    }

    std::vector<QuadtreeRenderer::Vertex> rVertices;
    {
        QuadtreeRenderer::Vertex v_1b;
        QuadtreeRenderer::Vertex v_2b;
            
        glm::vec3 color = glm::vec3(0.0f, 1.0f, 0.0f);
        
        if (m_restriction_direction)
            color = glm::vec3(0.0f, 0.0f, 1.0f);

        if (!m_restriction)
            color = glm::vec3(0.2f, 0.2f, 0.2f);
        
        v_1b.position = glm::vec3(m_restriction_line[0].x, m_restriction_line[0].y, 0.0f);
        v_1b.color = color;

        v_2b.position = glm::vec3(m_restriction_line[1].x, m_restriction_line[1].y, 0.0f);
        v_2b.color = color;
                
        rVertices.push_back(v_1b);
        rVertices.push_back(v_2b);

        QuadtreeRenderer::Vertex v_3b;
        QuadtreeRenderer::Vertex v_4b;
        QuadtreeRenderer::Vertex v_5b;
        QuadtreeRenderer::Vertex v_6b;

        color = glm::vec3(1.0f, 1.0f, 0.0f);

        glm::vec2 ep = m_frustrum_points[0];
        
        v_3b.position = glm::vec3(m_camera_point.x, m_camera_point.y, 0.0f);
        v_3b.color = color;
        v_4b.position = glm::vec3(ep.x, ep.y, 0.0f);
        v_4b.color = color;

        ep = m_frustrum_points[1];

        v_5b.position = glm::vec3(m_camera_point.x, m_camera_point.y, 0.0f);
        v_5b.color = color;
        v_6b.position = glm::vec3(ep.x, ep.y, 0.0f);
        v_6b.color = color;

        rVertices.push_back(v_3b);
        rVertices.push_back(v_4b);
        rVertices.push_back(v_5b);
        rVertices.push_back(v_6b);
    }


    unsigned int i = 0;

    if (m_vao)
        glDeleteBuffers(1, &m_vao);

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
        
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 6 * m_cubeVertices.size()
        , m_cubeVertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_TRUE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glBindVertexArray(0);

    if (m_vao_i)
        glDeleteBuffers(1, &m_vao_i);

    glGenVertexArrays(1, &m_vao_i);
    glBindVertexArray(m_vao_i);

    if (m_vbo_i)
        glDeleteBuffers(1, &m_vbo_i);

    glGenBuffers(1, &m_vbo_i);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_i);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 6 * m_cubeVertices_i.size()
        , m_cubeVertices_i.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_TRUE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glBindVertexArray(0);

    if (m_vao_p)
        glDeleteBuffers(1, &m_vao_p);

    glGenVertexArrays(1, &m_vao_p);
    glBindVertexArray(m_vao_p);
    
    if (m_vbo_p)
        glDeleteBuffers(1, &m_vbo_p);
    glGenBuffers(1, &m_vbo_p);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_p);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 6 * pVertices.size()
        ,pVertices.data() , GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_TRUE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glBindVertexArray(0);

    if (m_vao_r)
        glDeleteBuffers(1, &m_vao_r);

    glGenVertexArrays(1, &m_vao_r);
    glBindVertexArray(m_vao_r);

    if (m_vbo_r)
        glDeleteBuffers(1, &m_vbo_r);
    glGenBuffers(1, &m_vbo_r);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_r);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 6 * rVertices.size()
        , rVertices.data(), GL_STATIC_DRAW);

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

    glm::mat4 model = glm::translate(glm::vec3(0.35f, 0.15f, 0.0f))* glm::scale(glm::vec3(0.4f, 0.4f * ratio, 1.0));
    glm::mat4 view = model * glm::mat4();
    glm::mat4 projection = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f);

    m_model = model;
    m_model_inverse = glm::inverse(model);
        
    glm::vec4 screen_pos_trans = m_model_inverse * glm::vec4(screen_pos, 0.0f, 1.0f);

    m_camera_point = screen_pos;
    m_camera_point_trans = glm::vec2(screen_pos_trans.x, screen_pos_trans.y);

    auto lin1trans = (m_model_inverse * glm::vec4(m_restriction_line[0], 0.0f, 1.0f));
    m_restriction_line_trans[0] = glm::vec2(lin1trans.x, lin1trans.y);
    auto lin2trans = (m_model_inverse * glm::vec4(m_restriction_line[1], 0.0f, 1.0f));
    m_restriction_line_trans[1] = glm::vec2(lin2trans.x, lin2trans.y);

    auto frus1trans = (m_model_inverse * glm::vec4(m_frustrum_points[0], 0.0f, 1.0f));
    m_frustrum_points_trans[0] = glm::vec2(frus1trans.x, frus1trans.y);
    auto frus2trans = (m_model_inverse * glm::vec4(m_frustrum_points[1], 0.0f, 1.0f));
    m_frustrum_points_trans[1] = glm::vec2(frus2trans.x, frus2trans.y);

    auto testtrans = (m_model_inverse * glm::vec4(m_test_point, 0.0f, 1.0f));
    m_test_point_trans = glm::vec2(testtrans.x, testtrans.y);

    //std::cout << std::endl
    //std::cout << screen_pos.x << " " << screen_pos.y << std::endl;
    //std::cout << screen_pos_trans.x << " " << screen_pos_trans.y << std::endl;
    
    update_tree();
    //update_tree(screen_pos);
    update_vbo();


#if 0
    glActiveTexture(GL_TEXTURE0);
    updateTexture2D(m_texture_id_current, m_tree_resolution, m_tree_resolution, (char*)&m_tree_current->qtree_index_data[0], GL_RGBA, GL_UNSIGNED_BYTE);
    glActiveTexture(GL_TEXTURE1);
    updateTexture2D(m_texture_id_ideal, m_tree_resolution, m_tree_resolution, (char*)&m_tree_ideal->qtree_index_data[0], GL_RGBA, GL_UNSIGNED_BYTE);
#elif 1
    glActiveTexture(GL_TEXTURE0);
    updateTexture2D(m_texture_id_current, m_tree_resolution, m_tree_resolution, (char*)&m_tree_current->qtree_depth_data[0], GL_RED, GL_UNSIGNED_BYTE);
    glActiveTexture(GL_TEXTURE1);
    updateTexture2D(m_texture_id_ideal, m_tree_resolution, m_tree_resolution, (char*)&m_tree_ideal->qtree_depth_data[0], GL_RED, GL_UNSIGNED_BYTE);
#else
    glActiveTexture(GL_TEXTURE0);
    updateTexture2D(m_texture_id_current, m_tree_resolution, m_tree_resolution, (char*)&m_tree_current->qtree_id_data[0], GL_RED, GL_UNSIGNED_INT);
    glActiveTexture(GL_TEXTURE1);
    updateTexture2D(m_texture_id_ideal, m_tree_resolution, m_tree_resolution, (char*)&m_tree_ideal->qtree_id_data[0], GL_RED, GL_UNSIGNED_INT);
#endif
    
    glUseProgram(m_program_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(view));

    glBindVertexArray(m_vao);
    //glDrawArrays(GL_LINES, 0, m_cubeVertices.size());
    glBindVertexArray(0);

    glUseProgram(0);


    glm::mat4 model_s = glm::translate(glm::vec3(0.05f, 0.15f, 0.0f))* glm::scale(glm::vec3(0.2f, 0.2f * ratio, 1.0));    
    glUseProgram(m_program_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(model_s));

    glBindVertexArray(m_vao_i);
    glDrawArrays(GL_LINES, 0, m_cubeVertices_i.size());
    glBindVertexArray(0);

    glUseProgram(0);
        
    glUseProgram(m_program_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(glm::mat4()));

    glBindVertexArray(m_vao_r);
    glDrawArrays(GL_LINES, 0, 6);
    glBindVertexArray(0);

    glUseProgram(0);

    glPointSize(30.0);

    glUseProgram(m_program_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(view));

    glBindVertexArray(m_vao_p);
    glDrawArrays(GL_POINTS, 0, 6);
    glBindVertexArray(0);

    glUseProgram(0);

#if 1
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture_id_current);
    glUseProgram(m_program_texture_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_texture_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_texture_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(view));
    glUniform1i(glGetUniformLocation(m_program_texture_id, "Texture"), 0);

    glBindVertexArray(m_vao_quad);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glUseProgram(0);

    glBindTexture(GL_TEXTURE_2D, 0);
#endif
#if 0

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_texture_id_ideal);
    glUseProgram(m_program_texture_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_texture_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_texture_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(view));
    glUniform1i(glGetUniformLocation(m_program_texture_id, "Texture"), 1);

    glBindVertexArray(m_vao_quad);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glUseProgram(0);

    glBindTexture(GL_TEXTURE_2D, 0);
#endif

}
