#include "QuadtreeRenderer.hpp"

#include <iostream>
#include <fstream>
#include <GL/glew.h>
#include <GL/gl.h>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "utils.hpp"

const char* vertex_shader = "\
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

const char* fragment_shader = "\
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
: m_piecewise_container(),
m_program_id(0),
m_vao(0),
m_dirty(true)
{
    m_program_id = createProgram(vertex_shader, fragment_shader);
}

void QuadtreeRenderer::add(float data_value, glm::vec4 color)
{
    add((unsigned)(data_value * 255.0), color);
}

void
QuadtreeRenderer::add(unsigned data_value, glm::vec4 color)
{
    helper::clamp(data_value, 0u, 255u);
    helper::clamp(color.r, 1.0f, 1.0f);
    helper::clamp(color.g, 1.0f, 1.0f);
    helper::clamp(color.b, 1.0f, 1.0f);
    helper::clamp(color.a, 1.0f, 1.0f);

    m_piecewise_container.insert(element_type(data_value, color));
}

image_data_type QuadtreeRenderer::get_RGBA_Quadtree_Renderer_buffer() const
{
    size_t buffer_size = 255 * 4; // width =255 height = 1 channels = 4 ///TODO: maybe dont hardcode?
    image_data_type QuadtreeRenderer_buffer;
    QuadtreeRenderer_buffer.resize(buffer_size);

    unsigned data_value_f = 0u;
    unsigned data_value_b = 255u;
    glm::vec4 color_f = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 color_b = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

    unsigned  e_value;
    glm::vec4 e_color;

    for (auto e : m_piecewise_container) {
        e_value = e.first;
        e_color = e.second;

        data_value_b = e_value;
        color_b = e_color;

        unsigned data_value_d = data_value_b - data_value_f;
        float step_size = 1.0 / static_cast<float>(data_value_d);
        float step = 0.0;

        for (unsigned i = data_value_f; i != data_value_b; ++i) {

            QuadtreeRenderer_buffer[i * 4] = static_cast<unsigned char>(helper::weight(step, color_f.r, color_b.r) * 255.0f);
            QuadtreeRenderer_buffer[i * 4 + 1] = static_cast<unsigned char>(helper::weight(step, color_f.g, color_b.g) * 255.0f);
            QuadtreeRenderer_buffer[i * 4 + 2] = static_cast<unsigned char>(helper::weight(step, color_f.b, color_b.b) * 255.0f);
            QuadtreeRenderer_buffer[i * 4 + 3] = static_cast<unsigned char>(helper::weight(step, color_f.a, color_b.a) * 255.0f);
            step += step_size;
        }

        data_value_f = data_value_b;
        color_f = color_b;
    }

    // fill TF
    data_value_b = 255u;
    color_b = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

    if (data_value_f != data_value_b) {
        unsigned data_value_d = data_value_b - data_value_f;
        float step_size = 1.0 / static_cast<float>(data_value_d);
        float step = 0.0;

        for (unsigned i = data_value_f; i != data_value_b; ++i) {
            QuadtreeRenderer_buffer[i * 4] = static_cast<unsigned char>(helper::weight(step, color_f.r, color_b.r) * 255.0f);
            QuadtreeRenderer_buffer[i * 4 + 1] = static_cast<unsigned char>(helper::weight(step, color_f.g, color_b.g) * 255.0f);
            QuadtreeRenderer_buffer[i * 4 + 2] = static_cast<unsigned char>(helper::weight(step, color_f.b, color_b.b) * 255.0f);
            QuadtreeRenderer_buffer[i * 4 + 3] = static_cast<unsigned char>(helper::weight(step, color_f.a, color_b.a) * 255.0f);
            step += step_size;
        }
    }

    return QuadtreeRenderer_buffer;
}

void
QuadtreeRenderer::reset(){
    m_piecewise_container.clear();
    m_dirty = true;
}

void
QuadtreeRenderer::update_vbo(){

    std::vector<QuadtreeRenderer::Vertex> cubeVertices;

    QuadtreeRenderer::Vertex v_rb;
    QuadtreeRenderer::Vertex v_gb;
    QuadtreeRenderer::Vertex v_bb;
    QuadtreeRenderer::Vertex v_ab;

    QuadtreeRenderer::Vertex v_re;
    QuadtreeRenderer::Vertex v_ge;
    QuadtreeRenderer::Vertex v_be;
    QuadtreeRenderer::Vertex v_ae;

    v_rb.position = glm::vec3(0.0, 0.0, 0.0f);
    v_rb.color = glm::vec3(1.0f, 0.0f, 0.0f);
    v_rb.position = glm::vec3(0.0, 0.0, 0.0f);
    v_gb.color = glm::vec3(0.0f, 1.0f, 0.0f);
    v_rb.position = glm::vec3(0.0, 0.0, 0.0f);
    v_bb.color = glm::vec3(0.0f, 0.0f, 1.0f);
    v_rb.position = glm::vec3(0.0, 0.0, 0.0f);
    v_ab.color = glm::vec3(1.0f, 1.0f, 1.0f);

    for (auto e = m_piecewise_container.begin(); e != m_piecewise_container.end(); ++e) {

        v_re.position = glm::vec3((float)e->first / 255.0f, (float)e->second.r, 0.0f);
        v_re.color = glm::vec3(1.0f, 0.0f, 0.0f);
        v_ge.position = glm::vec3((float)e->first / 255.0f, e->second.g, 0.0f);
        v_ge.color = glm::vec3(0.0f, 1.0f, 0.0f);
        v_be.position = glm::vec3((float)e->first / 255.0f, e->second.b, 0.0f);
        v_be.color = glm::vec3(0.0f, 0.0f, 1.0f);
        v_ae.position = glm::vec3((float)e->first / 255.0f, e->second.a, 0.0f);
        v_ae.color = glm::vec3(0.4f, 0.4f, 0.4f);

        cubeVertices.push_back(v_rb);
        cubeVertices.push_back(v_re);

        cubeVertices.push_back(v_gb);
        cubeVertices.push_back(v_ge);

        cubeVertices.push_back(v_bb);
        cubeVertices.push_back(v_be);

        cubeVertices.push_back(v_ab);
        cubeVertices.push_back(v_ae);

        v_rb.position = v_re.position;
        v_rb.color = v_re.color;
        v_gb.position = v_ge.position;
        v_gb.color = v_ge.color;
        v_bb.position = v_be.position;
        v_bb.color = v_be.color;
        v_ab.position = v_ae.position;
        v_ab.color = v_ae.color;

    }

    v_re.position = glm::vec3(1.0, 0.0, 0.0f);
    v_re.color = glm::vec3(1.0f, 0.0f, 0.0f);
    v_ge.position = glm::vec3(1.0, 0.0, 0.0f);
    v_ge.color = glm::vec3(0.0f, 1.0f, 0.0f);
    v_be.position = glm::vec3(1.0, 0.0, 0.0f);
    v_be.color = glm::vec3(0.0f, 0.0f, 1.0f);
    v_ae.position = glm::vec3(1.0, 0.0, 0.0f);
    v_ae.color = glm::vec3(1.0f, 1.0f, 1.0f);

    if (m_piecewise_container.size() == 0){

        cubeVertices.push_back(v_rb);
        cubeVertices.push_back(v_re);

        cubeVertices.push_back(v_gb);
        cubeVertices.push_back(v_ge);

        cubeVertices.push_back(v_bb);
        cubeVertices.push_back(v_be);

        cubeVertices.push_back(v_ab);
        cubeVertices.push_back(v_ae);
    }
    else{
        cubeVertices.push_back(v_re);

        cubeVertices.push_back(v_ge);

        cubeVertices.push_back(v_be);

        cubeVertices.push_back(v_ae);
    }

    unsigned int i = 0;

    if (m_vao)
        glDeleteBuffers(1, &m_vao);

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float)* 6 * cubeVertices.size()
        , cubeVertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_TRUE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glBindVertexArray(0);

}

void QuadtreeRenderer::update_and_draw()
{
    if (m_dirty){
        update_vbo();
        m_dirty = false;
    }

    glm::mat4 projection = glm::ortho(-0.5f, 3.5f, -0.5f, 5.5f);
    glm::mat4 view = glm::mat4();

    glUseProgram(m_program_id);
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_program_id, "Modelview"), 1, GL_FALSE,
        glm::value_ptr(view));

    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINES, 0, m_piecewise_container.size() * 2 * 6);
    glBindVertexArray(0);

    glUseProgram(0);


}
