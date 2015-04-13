#ifndef M_TRANSFER_FUNCTION_HPP
#define M_TRANSFER_FUNCTION_HPP

#include "data_types_fwd.hpp"

#include <string>
#include <map>

#define GLM_FORCE_RADIANS
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <utils.hpp>
#include <plane.hpp>

class Multi_dim_transfer_function
{
public:
    typedef std::pair<unsigned, glm::vec4> element_type;
    typedef std::map<unsigned, glm::vec4>  container_type;

public:
    Multi_dim_transfer_function();
    ~Multi_dim_transfer_function() {}

    void add(glm::vec2 pos, float radius, glm::vec4 color);

    void remove(unsigned);

    void reset();

    image_data_type       get_RGBA_transfer_function_buffer() const;
    void                  draw_texture(glm::vec2 const& window_dim, glm::vec2 const& tf_pos, GLuint const& texture) const;

private:
    //void update_vbo();

private:
    GLuint          m_transfer_texture;
    GLuint          m_joint_histogram;
    unsigned int      m_program_id;
    //unsigned int      m_vao;
    Plane             m_plane;

    bool              m_dirty;
};

#endif // define M_TRANSFER_FUNCTION_HPP
