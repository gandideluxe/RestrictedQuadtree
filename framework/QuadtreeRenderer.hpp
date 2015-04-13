#ifndef QUADTREERENDERER_HPP
#define QUADTREERENDERER_HPP

#include "data_types_fwd.hpp"

#include <string>
#include <map>

#define GLM_FORCE_RADIANS
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

class QuadtreeRenderer
{
public:
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 color;
    };
    typedef std::pair<unsigned, glm::vec4> element_type;
    typedef std::map<unsigned, glm::vec4>  container_type;

public:
    QuadtreeRenderer();
    ~QuadtreeRenderer() {}

    void add(float, glm::vec4);
    void add(unsigned, glm::vec4);

    void reset();

    image_data_type   get_RGBA_Quadtree_Renderer_buffer() const;
    void              update_and_draw();

private:
    void update_vbo();

private:
    container_type    m_piecewise_container;

    unsigned int      m_program_id;
    unsigned int      m_vao;

    bool              m_dirty;
};

#endif // define QUADTREERENDERER_HPP
