#ifndef QUADTREERENDERER_HPP
#define QUADTREERENDERER_HPP

#include "data_types_fwd.hpp"

#include <vector>
#include <stack>
#include <queue>
#include <algorithm>
#include <functional>
#include <memory>
#include <iostream>
#include <stdlib.h> 

#include <quadtree_layout.h>

#include <string>
#include <map>

#define GLM_FORCE_RADIANS
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#define CHILDREN 4
#define NEIGHBORS 8


namespace helper {

    template<typename T>
    static const T clamp(const T val, const T min, const T max)
    {
        return ((val > max) ? max : (val < min) ? min : val);
    }

    template<typename T>
    static const T weight(const float w, const T a, const T b)
    {
        return ((1.0 - w) * a + w * b);
    }

    ///////////////////////////////////////////////////////////////////////
    static float Gamma = 0.80;
    static float IntensityMax = 255.0;

    ///////////////////////////////////////////////////////////////////////
    static float round(float d){
        return floor(d + 0.5);
    }

    ///////////////////////////////////////////////////////////////////////
    static float Adjust(float Color, float Factor){
        if (Color == 0.0){
            return 0.0;
        }
        else{
            float res = round(IntensityMax * pow(Color * Factor, Gamma));
            return glm::min(255.0f, glm::max(0.0f, res));
        }
    }

    ///////////////////////////////////////////////////////////////////////
    static glm::vec3 WavelengthToRGB(float Wavelength)
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
    static float GetWaveLengthFromDataPoint(float Value, float MinValue, float MaxValue)
    {
        float MinVisibleWavelength = 380.0;//350.0;
        float MaxVisibleWavelength = 780.0;//650.0;
        //Convert data value in the range of MinValues..MaxValues to the
        //range 350..780
        return (Value - MinValue) / (MaxValue - MinValue) * (MaxVisibleWavelength - MinVisibleWavelength) + MinVisibleWavelength;
    }

} // namespace helper

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
        unsigned used_ideal_budget;

        size_t memory_usage;

        float puffer_size;

        unsigned max_depth;

        float min_prio;
        float max_prio;
        
        size_t time_new_tree_update;
        size_t time_current_tree_update;

        glm::uvec2 page_dim;
        glm::uvec2 ref_dim;

        glm::vec2 ref_pos;
        glm::vec2 ref_pos_trans;

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
        std::vector<unsigned> qtree_id_data; //visulizing only

        std::vector<unsigned char> get_id_data_rgb() const{
            std::vector<unsigned char> rgb_index_data;
            rgb_index_data.resize(qtree_id_data.size() * 3);

            auto counter = 0;
            for (auto i : qtree_id_data){
                                
                //std::cout << "ID: " << i << " Depth: " << (unsigned)qtree_depth_data[counter] << std::endl;
                srand(1);
                srand(i * 4 * rand());
                unsigned blub = rand() % budget;

                auto color = helper::WavelengthToRGB(helper::GetWaveLengthFromDataPoint((float)blub, -1.0f, (float)budget));

                rgb_index_data[counter * 3 + 0] = ((unsigned char)(color.r * 255.0));
                rgb_index_data[counter * 3 + 1] = ((unsigned char)(color.g * 255.0));
                rgb_index_data[counter * 3 + 2] = ((unsigned char)(color.b * 255.0));
                ++counter;
            }

            return rgb_index_data;
        }
        
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

    std::vector<q_node_ptr> get_splitable_nodes(QuadtreeRenderer::q_tree_ptr t) const;
    std::vector<q_node_ptr> get_collabsible_nodes(QuadtreeRenderer::q_tree_ptr t) const;
    std::vector<q_node_ptr> get_leaf_nodes(QuadtreeRenderer::q_tree_ptr t) const;

    q_node_ptr get_neighbor_node(const q_node_ptr n, const q_tree_ptr tree, const unsigned neighbor_nbr) const;
    std::vector<q_node_ptr> check_neighbors_for_level_div(const q_node_ptr n, const float level_div) const;
    std::vector<q_node_ptr> check_neighbors_for_split(const q_node_ptr n) const;
    std::vector<q_node_ptr> check_neighbors_for_merge(const q_node_ptr n) const;
    std::vector<q_node_ptr> check_neighbors_for_restricted(const q_node_ptr n) const;
    void generate_ideal_tree(q_tree_ptr src, q_tree_ptr dst);
    void copy_tree(q_tree_ptr src, q_tree_ptr dst);
    void collapse_negative_nodes(q_tree_ptr t);
    void optimize_current_tree(q_tree_ptr src, q_tree_ptr dst);
    void optimize_ideal_tree(q_tree_ptr t);
    
    void update_vbo();
    void update_tree();    
    void update_priorities(q_tree_ptr m_tree);
    float get_importance_of_node(q_node_ptr n) const;
    float get_error_of_node(q_node_ptr n) const;

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
