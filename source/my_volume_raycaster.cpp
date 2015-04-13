// -----------------------------------------------------------------------------
// Copyright  : (C) 2014 Andreas-C. Bernstein
//                  2015 Sebastian Thiele
// License    : MIT (see the file LICENSE)
// Maintainer : Sebastian Thiele <sebastian.thiele@uni-weimar.de>
// Stability  : experimantal exercise
//
// scivis exercise Example
// -----------------------------------------------------------------------------
#ifdef _MSC_VER
#pragma warning (disable: 4996)         // 'This function or variable may be unsafe': strcpy, strdup, sprintf, vsnprintf, sscanf, fopen
#endif

#define _USE_MATH_DEFINES
#include "fensterchen.hpp"
#include <string>
#include <iostream>
#include <sstream>      // std::stringstream
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <cmath>

///GLM INCLUDES
#define GLM_FORCE_RADIANS
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>

///PROJECT INCLUDES
#include <volume_loader_raw.hpp>
#include <volume_loader_raw_hurrican.hpp>
#include <transfer_function.hpp>
#include <utils.hpp>
#include <turntable.hpp>
#include <imgui.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>        // stb_image.h for PNG loading
#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))

//-----------------------------------------------------------------------------
// Window Size
//-----------------------------------------------------------------------------
glm::ivec2  g_window_res = glm::ivec2(1920, 1080);

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

#define IM_ARRAYSIZE(_ARR)          ((int)(sizeof(_ARR)/sizeof(*_ARR)))

#undef PI
const float PI = 3.14159265358979323846f;

#ifdef INT_MAX
#define IM_INT_MAX INT_MAX
#else
#define IM_INT_MAX 2147483647
#endif

// Play it nice with Windows users. Notepad in 2014 still doesn't display text data with Unix-style \n.
#ifdef _MSC_VER
#define STR_NEWLINE "\r\n"
#else
#define STR_NEWLINE "\n"
#endif

const std::string g_file_vertex_shader("../../../source/shader/volume.vert");
const std::string g_file_fragment_shader("../../../source/shader/volume.frag");

const std::string g_GUI_file_vertex_shader("../../../source/shader/pass_through_GUI.vert");
const std::string g_GUI_file_fragment_shader("../../../source/shader/pass_through_GUI.frag");

GLuint loadShaders(std::string const& vs, std::string const& fs)
{
    std::string v = readFile(vs);
    std::string f = readFile(fs);
    return createProgram(v, f);
}

GLuint loadShaders(
    std::string const& vs,
    std::string const& fs,
    const int task_nbr,
    const int enable_lightning,
    const int enable_shadowing,
    const int enable_opeacity_cor)
{
    std::string v = readFile(vs);
    std::string f = readFile(fs);

    std::stringstream ss1;
    ss1 << task_nbr;

    int index = f.find("#define TASK");
    f.replace(index + 13, 2, ss1.str());

    std::stringstream ss2;
    ss2 << enable_opeacity_cor;

    index = f.find("#define ENABLE_OPACITY_CORRECTION");
    f.replace(index + 34, 1, ss2.str());

    std::stringstream ss3;
    ss3 << enable_lightning;

    index = f.find("#define ENABLE_LIGHTNING");
    f.replace(index + 25, 1, ss3.str());

    std::stringstream ss4;
    ss4 << enable_shadowing;

    index = f.find("#define ENABLE_SHADOWING");
    f.replace(index + 25, 1, ss4.str());

    //std::cout << f << std::endl;

    return createProgram(v, f);
}

Turntable  g_turntable;

///SETUP VOLUME RAYCASTER HERE
// set the volume file
std::string g_file_string = "../../../data/head_w256_h256_d225_c1_b8.raw";

// set the sampling distance for the ray traversal
float       g_sampling_distance = 0.001f;
float       g_sampling_distance_fact = 0.5f;
float       g_sampling_distance_fact_move = 2.0f;
float       g_sampling_distance_fact_ref = 1.0f;

float       g_iso_value = 0.2f;

// set the light position and color for shading
glm::vec3   g_light_pos = glm::vec3(10.0, 10.0, 10.0);
glm::vec3   g_ambient_light_color = glm::vec3(0.1f, 0.1f, 0.1f);
glm::vec3   g_diffuse_light_color = glm::vec3(1.0f, 1.0f, 1.0f);
glm::vec3   g_specula_light_color = glm::vec3(1.0f, 1.0f, 1.0f);
float       g_ref_coef = 12.0;

// set backgorund color here
//glm::vec3   g_background_color = glm::vec3(1.0f, 1.0f, 1.0f); //white
glm::vec3   g_background_color = glm::vec3(0.0f, 0.0f, 0.0f);   //grey


Window g_win(g_window_res);

// Volume Rendering GLSL Program
GLuint g_volume_program(0);
std::string g_error_message;
bool g_reload_shader_error = false;

Volume_loader_raw g_volume_loader;
Volume_loader_raw_hurricane g_volume_loader_hur;
glm::ivec3 g_vol_dimensions;
glm::vec3 g_max_volume_bounds;
unsigned g_channel_size = 0;
unsigned g_channel_count = 0;
std::vector<GLuint> g_volume_textures;
std::vector<glm::uint64> g_volume_texture_handles;
GLuint g_handles_ssbo;
GLuint g_tf_handles_ssbo;
GLuint g_channels_used_ssbo;
GLuint g_channels_range_ssbo;
GLuint g_color_write_ssbo;

GLuint g_samplerID = 0;

Cube g_cube;

std::vector<char*> g_channels_names;

//std::vector<GLuint> g_volume_texture_handles;

std::vector<Transfer_function> g_transfer_fun;
std::vector<GLuint> g_transfer_texture;
std::vector<glm::uint64> g_transfer_texture_handles;
std::vector<unsigned> g_channels_used;
int g_current_tf = 0;
int g_current_tf_data_value = 0;
bool g_transfer_dirty = true;
bool g_redraw_tf = true;
bool g_lighting_toggle = false;
bool g_shadow_toggle = false;
bool g_opacity_correction_toggle = false;

// imgui variables
static bool g_show_gui = true;

GLuint g_color_tex = 0;

static GLuint fontTex;
static bool mousePressed[2] = { false, false };

static int g_gui_program, vert_handle, frag_handle;
static int texture_location, ortho_location;
static int position_location, uv_location, colour_location;
static size_t vbo_max_size = 20000;
static unsigned int vbo_handle, vao_handle;

bool g_show_transfer_function_in_window = false;
glm::vec2 g_transfer_function_pos = glm::vec2(0.0f);
glm::vec2 g_transfer_function_size = glm::vec2(0.0f);

//imgui values
bool g_over_gui = false;
bool g_reload_shader = false;
bool g_reload_shader_pressed = false;
bool g_show_transfer_function = false;

int g_task_chosen = 21;
int g_task_chosen_old = g_task_chosen;

bool  g_pause = false;

bool g_time_control = false;
bool g_time_repeat = true;
float g_time_last = 0.0;
float g_time_per_sample_in_s = 1.0;
float g_time_position = 2.0;//start
unsigned g_min_time_step = 0u;
unsigned g_max_time_step = 1u;
bool g_time_pause;

int g_bilinear_interpolation = true;

struct Manipulator
{
    Manipulator()
    : m_turntable()
    , m_mouse_button_pressed(0, 0, 0)
    , m_mouse(0.0f, 0.0f)
    , m_lastMouse(0.0f, 0.0f)
    {}

    glm::mat4 matrix()
    {
        m_turntable.rotate(m_slidelastMouse, m_slideMouse);
        return m_turntable.matrix();
    }

    glm::mat4 matrix(Window const& g_win)
    {
        m_mouse = g_win.mousePosition();
        if (g_win.isButtonPressed(Window::MOUSE_BUTTON_LEFT)) {
            if (!m_mouse_button_pressed[0]) {
                m_mouse_button_pressed[0] = 1;
            }
            m_turntable.rotate(m_lastMouse, m_mouse);
            m_slideMouse = m_mouse;
            m_slidelastMouse = m_lastMouse;
        }
        else {
            m_mouse_button_pressed[0] = 0;
            m_turntable.rotate(m_slidelastMouse, m_slideMouse);
            //m_slideMouse *= 0.99f;
            //m_slidelastMouse *= 0.99f;
        }

        if (g_win.isButtonPressed(Window::MOUSE_BUTTON_MIDDLE)) {
            if (!m_mouse_button_pressed[1]) {
                m_mouse_button_pressed[1] = 1;
            }
            m_turntable.pan(m_lastMouse, m_mouse);
        }
        else {
            m_mouse_button_pressed[1] = 0;
        }

        if (g_win.isButtonPressed(Window::MOUSE_BUTTON_RIGHT)) {
            if (!m_mouse_button_pressed[2]) {
                m_mouse_button_pressed[2] = 1;
            }
            m_turntable.zoom(m_lastMouse, m_mouse);
        }
        else {
            m_mouse_button_pressed[2] = 0;
        }

        m_lastMouse = m_mouse;
        return m_turntable.matrix();
    }

private:
    Turntable  m_turntable;
    glm::ivec3 m_mouse_button_pressed;
    glm::vec2  m_mouse;
    glm::vec2  m_lastMouse;
    glm::vec2  m_slideMouse;
    glm::vec2  m_slidelastMouse;
};


bool read_volume(std::string& volume_string){

    //init volume g_volume_loader
    //Volume_loader_raw g_volume_loader;
    //read volume dimensions
    g_vol_dimensions = g_volume_loader.get_dimensions(g_file_string);

    unsigned max_dim = std::max(std::max(g_vol_dimensions.x,
        g_vol_dimensions.y),
        g_vol_dimensions.z);

    g_sampling_distance = 1.0f / max_dim;

    // calculating max volume bounds of volume (0.0 .. 1.0)
    g_max_volume_bounds = glm::vec3(g_vol_dimensions) / glm::vec3((float)max_dim);

    GLuint error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "OpenGL Error 307: " << error << std::endl;
    }
    //g_volum
    // setting up proxy geometry
    g_cube.freeVAO();
    g_cube = Cube(glm::vec3(0.0, 0.0, 0.0), g_max_volume_bounds);

    // loading volume file data
    volume_data_type volume_data = g_volume_loader.load_volume(g_file_string);
    g_channel_size = g_volume_loader.get_bit_per_channel(g_file_string) / 8;
    g_channel_count = g_volume_loader.get_channel_count(g_file_string);
        
    for (auto textures_iter = g_volume_textures.begin(); textures_iter != g_volume_textures.end(); ++textures_iter){
        glDeleteTextures(1, &(*textures_iter));
    }
        
    g_volume_textures.clear();
    g_volume_texture_handles.clear();
        
    if (!g_samplerID){
        glGenSamplers(1, &g_samplerID);
        glSamplerParameteri(g_samplerID, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glSamplerParameteri(g_samplerID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glSamplerParameteri(g_samplerID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(g_samplerID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glSamplerParameterf(g_samplerID, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16.0f);
    }
    //glActiveTexture(GL_TEXTURE0);
    g_volume_textures.push_back(createTexture3D(g_vol_dimensions.x, g_vol_dimensions.y, g_vol_dimensions.z, g_channel_size, g_channel_count, (char*)&volume_data[0]));
    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "OpenGL Error 338: " << error << std::endl;
    }
    
    g_volume_texture_handles.push_back(glGetTextureHandleNV(g_volume_textures[0]));    
    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "OpenGL Error 345: " << error << std::endl;
    }
    
    // init and upload transfer function texture    
    if (g_transfer_texture.size() == 0){
        glActiveTexture(GL_TEXTURE1);
        g_transfer_texture.push_back(createTexture2D(255u, 1u, (char*)&g_transfer_fun[0].get_RGBA_transfer_function_buffer()[0]));        
        g_transfer_texture_handles.push_back(glGetTextureHandleNV(g_transfer_texture[0]));
    }

    g_channels_used.clear();
    g_channels_used.push_back(true);

    {       
        if (g_handles_ssbo)
            glDeleteBuffers(1, &g_handles_ssbo);
       
        glGenBuffers(1, &g_handles_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_handles_ssbo);
        unsigned buffer_data_size = g_volume_texture_handles.size() * sizeof(glm::uint64);
        glBufferData(GL_SHADER_STORAGE_BUFFER, buffer_data_size, g_volume_texture_handles.data(), GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    }

    {
        if (g_tf_handles_ssbo)
            glDeleteBuffers(1, &g_tf_handles_ssbo);

        glGenBuffers(1, &g_tf_handles_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_tf_handles_ssbo);
        unsigned buffer_data_size = g_transfer_texture_handles.size() * sizeof(glm::uint64);
        glBufferData(GL_SHADER_STORAGE_BUFFER, buffer_data_size, g_transfer_texture_handles.data(), GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }


    {
        if (g_channels_used_ssbo)
            glDeleteBuffers(1, &g_channels_used_ssbo);

        glGenBuffers(1, &g_channels_used_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_channels_used_ssbo);
        unsigned buffer_data_size = g_channels_used.size() * sizeof(unsigned);
        glBufferData(GL_SHADER_STORAGE_BUFFER, buffer_data_size, g_channels_used.data(), GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    {

        std::vector<glm::vec2> channels_range;
        channels_range.resize(g_channels_used.size(), glm::vec2(0.0f, 1.0f));

        if (g_channels_range_ssbo)
            glDeleteBuffers(1, &g_channels_range_ssbo);

        glGenBuffers(1, &g_channels_range_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_channels_range_ssbo);
        unsigned buffer_data_size = channels_range.size() * sizeof(glm::vec2);
        glBufferData(GL_SHADER_STORAGE_BUFFER, buffer_data_size, channels_range.data(), GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    {
        std::vector<glm::vec4> interpolation_write;
        interpolation_write.resize(2u, glm::vec4(0.0f));

        if (g_color_write_ssbo)
            glDeleteBuffers(1, &g_color_write_ssbo);

        glGenBuffers(1, &g_color_write_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_color_write_ssbo);
        unsigned buffer_data_size = interpolation_write.size() * sizeof(glm::vec4);
        glBufferData(GL_SHADER_STORAGE_BUFFER, buffer_data_size, interpolation_write.data(), GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    
    return g_volume_textures[0];
}

bool read_volumes(std::string volume_string, unsigned channel_count, unsigned start_time_step, unsigned last_time_steps, unsigned time_step_size, bool hurricane){

    //init volume g_volume_loader
    //Volume_loader_raw g_volume_loader;
    //read volume dimensions
    if (!hurricane){
        g_vol_dimensions = g_volume_loader.get_dimensions(volume_string);

        unsigned max_dim = std::max(std::max(g_vol_dimensions.x,
            g_vol_dimensions.y),
            g_vol_dimensions.z);

        g_sampling_distance = 1.0f / max_dim;

        // calculating max volume bounds of volume (0.0 .. 1.0)
        g_max_volume_bounds = glm::vec3(g_vol_dimensions) / glm::vec3((float)max_dim);

        // loading volume file data
        //volume_data = g_volume_loader.load_volume(volume_string);
        g_channel_size = g_volume_loader.get_bit_per_channel(volume_string) / 8;
        g_channel_count = channel_count;// g_volume_loader.get_channel_count(volume_string);
        
        //first delete _t _cn and filending from volume string
        int index = volume_string.find("_cn");
        volume_string.erase(index, volume_string.size() - index);
    }
    else{
        g_vol_dimensions = g_volume_loader_hur.get_dimensions(volume_string);

        unsigned max_dim = std::max(std::max(g_vol_dimensions.x,
            g_vol_dimensions.y),
            g_vol_dimensions.z);

        // calculating max volume bounds of volume (0.0 .. 1.0)
        g_max_volume_bounds = glm::vec3(g_vol_dimensions) / glm::vec3((float)max_dim);

        // loading volume file data
        //volume_data = g_volume_loader_hur.load_volume(volume_string);
        g_channel_size = g_volume_loader_hur.get_bit_per_channel(volume_string) / 8;
        g_channel_count = channel_count;// g_volume_loader_hur.get_channel_count(volume_string);
    }
    // setting up proxy geometry
    g_cube = Cube(glm::vec3(0.0, 0.0, 0.0), g_max_volume_bounds);
    
    if (g_samplerID){
        glGenSamplers(1, &g_samplerID);
        glSamplerParameteri(g_samplerID, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glSamplerParameteri(g_samplerID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glSamplerParameteri(g_samplerID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(g_samplerID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glSamplerParameterf(g_samplerID, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16.0f);
    }

    for (auto textures_iter = g_volume_textures.begin(); textures_iter != g_volume_textures.end(); ++textures_iter){
        glDeleteTextures(1, &(*textures_iter));
    }

    g_volume_textures.clear();
    g_volume_texture_handles.clear();
    g_channels_used.clear(); 
    g_channels_used.resize(channel_count);
    unsigned i = 0;

    auto tfcount = g_transfer_fun.size();
    int dif = tfcount - channel_count;
    while (dif < 0){
        g_transfer_fun.push_back(Transfer_function());
        g_transfer_fun.back().reset();

        // the add_stop method takes:
        //  - unsigned char or float - data value     (0.0 .. 1.0) or (0..255)
        //  - vec4f         - color and alpha value   (0.0 .. 1.0) per channel
        g_transfer_fun.back().add(0.0f, glm::vec4(0.0, 0.0, 0.0, 0.0));
        g_transfer_fun.back().add(1.0f, glm::vec4(1.0, 1.0, 1.0, 1.0));


        image_data_type color_con = g_transfer_fun.back().get_RGBA_transfer_function_buffer();
        g_transfer_texture.push_back(createTexture2D(255u, 1u, (char*)&color_con[0]));
        //g_transfer_texture_handles[dif] = glGetTextureSamplerHandleNV(g_transfer_texture[dif], g_samplerID);
        g_transfer_texture_handles.push_back(glGetTextureHandleNV(g_transfer_texture.back()));

        ++dif;        
    }
    
    size_t loaded_volumes_size = 0;
    size_t file_size = size_t(500)*500*100*4;

    for (unsigned c = 0; c <= g_channel_count - 1; ++c){
        g_channels_used[c] = 0u;
        for (unsigned t = start_time_step; t <= last_time_steps; t += time_step_size){
            
            std::stringstream ss;
                        
            std::cout << ss.str() << std::endl;            
            volume_data_type volume_data;
            
            if (!hurricane){
                ss << volume_string << "_cn" << c << "_t" << t << ".raw";
                volume_data = g_volume_loader.load_volume(ss.str());
            }
            else{
                loaded_volumes_size += file_size;
                std::cout << loaded_volumes_size / 1024 / 1024 / 1024 << " GB loaded!" << std::endl;
                volume_data = g_volume_loader_hur.load_volume(volume_string, c, t);
            }
                        
            g_volume_textures.push_back(createTexture3D(g_vol_dimensions.x, g_vol_dimensions.y, g_vol_dimensions.z, g_channel_size, g_channel_count, (char*)&volume_data[0]));            
            g_volume_texture_handles.push_back(glGetTextureHandleNV(g_volume_textures[i]));
                                    
            ++i;
        }
    }    

    g_channels_used[0] = 1u;
    
    {
        if (g_handles_ssbo)
            glDeleteBuffers(1, &g_handles_ssbo);

        glGenBuffers(1, &g_handles_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_handles_ssbo);
        unsigned buffer_data_size = g_volume_texture_handles.size() * sizeof(glm::uint64);
        glBufferData(GL_SHADER_STORAGE_BUFFER, buffer_data_size, g_volume_texture_handles.data(), GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    {
        if (g_tf_handles_ssbo)
            glDeleteBuffers(1, &g_tf_handles_ssbo);

        glGenBuffers(1, &g_tf_handles_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_tf_handles_ssbo);
        unsigned buffer_data_size = g_transfer_texture_handles.size() * sizeof(glm::uint64);
        glBufferData(GL_SHADER_STORAGE_BUFFER, buffer_data_size, g_transfer_texture_handles.data(), GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }


    {
        if (g_channels_used_ssbo)
            glDeleteBuffers(1, &g_channels_used_ssbo);

        glGenBuffers(1, &g_channels_used_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_channels_used_ssbo);
        unsigned buffer_data_size = g_channels_used.size() * sizeof(unsigned);
        glBufferData(GL_SHADER_STORAGE_BUFFER, buffer_data_size, g_channels_used.data(), GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    {

        std::vector<glm::vec2> channels_range;
        //channels_range.resize(g_channels_used.size(), glm::vec2(0.0f, 1.0f));

        channels_range = g_volume_loader_hur.get_channel_ranges();

        if (g_channels_range_ssbo)
            glDeleteBuffers(1, &g_channels_range_ssbo);

        glGenBuffers(1, &g_channels_range_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_channels_range_ssbo);
        unsigned buffer_data_size = channels_range.size() * sizeof(glm::vec2);
        glBufferData(GL_SHADER_STORAGE_BUFFER, buffer_data_size, channels_range.data(), GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    {
        std::vector<glm::vec4> interpolation_write;
        interpolation_write.resize(2u, glm::vec4(0.0f));

        if (g_color_write_ssbo)
            glDeleteBuffers(1, &g_color_write_ssbo);

        glGenBuffers(1, &g_color_write_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_color_write_ssbo);
        unsigned buffer_data_size = interpolation_write.size() * sizeof(glm::vec4);
        glBufferData(GL_SHADER_STORAGE_BUFFER, buffer_data_size, interpolation_write.data(), GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    return true;
}

// This is the main rendering function that you have to implement and provide to ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure)
// If text or lines are blurry when integrating ImGui in your engine:
// - try adjusting ImGui::GetIO().PixelCenterOffset to 0.0f or 0.5f
// - in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
static void ImImpl_RenderDrawLists(ImDrawList** const cmd_lists, int cmd_lists_count)
{
    if (cmd_lists_count == 0)
        return;

    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    // Setup texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTex);

    // Setup orthographic projection matrix
    const float width = ImGui::GetIO().DisplaySize.x;
    const float height = ImGui::GetIO().DisplaySize.y;
    const float ortho_projection[4][4] =
    {
        { 2.0f / width, 0.0f, 0.0f, 0.0f },
        { 0.0f, 2.0f / -height, 0.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f, 0.0f },
        { -1.0f, 1.0f, 0.0f, 1.0f },
    };
    glUseProgram(g_gui_program);
    glUniform1i(texture_location, 0);
    glUniformMatrix4fv(ortho_location, 1, GL_FALSE, &ortho_projection[0][0]);

    // Grow our buffer according to what we need
    size_t total_vtx_count = 0;
    for (int n = 0; n < cmd_lists_count; n++)
        total_vtx_count += cmd_lists[n]->vtx_buffer.size();
    glBindBuffer(GL_ARRAY_BUFFER, vbo_handle);
    size_t neededBufferSize = total_vtx_count * sizeof(ImDrawVert);
    if (neededBufferSize > vbo_max_size)
    {
        vbo_max_size = neededBufferSize + 5000;  // Grow buffer
        glBufferData(GL_ARRAY_BUFFER, vbo_max_size, NULL, GL_STREAM_DRAW);
    }

    // Copy and convert all vertices into a single contiguous buffer
    unsigned char* buffer_data = (unsigned char*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (!buffer_data)
        return;
    for (int n = 0; n < cmd_lists_count; n++)
    {
        const ImDrawList* cmd_list = cmd_lists[n];
        memcpy(buffer_data, &cmd_list->vtx_buffer[0], cmd_list->vtx_buffer.size() * sizeof(ImDrawVert));
        buffer_data += cmd_list->vtx_buffer.size() * sizeof(ImDrawVert);
    }
    glUnmapBuffer(GL_ARRAY_BUFFER);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(vao_handle);

    int cmd_offset = 0;
    for (int n = 0; n < cmd_lists_count; n++)
    {
        const ImDrawList* cmd_list = cmd_lists[n];
        int vtx_offset = cmd_offset;
        const ImDrawCmd* pcmd_end = cmd_list->commands.end();
        for (const ImDrawCmd* pcmd = cmd_list->commands.begin(); pcmd != pcmd_end; pcmd++)
        {
            glScissor((int)pcmd->clip_rect.x, (int)(height - pcmd->clip_rect.w), (int)(pcmd->clip_rect.z - pcmd->clip_rect.x), (int)(pcmd->clip_rect.w - pcmd->clip_rect.y));
            glDrawArrays(GL_TRIANGLES, vtx_offset, pcmd->vtx_count);
            vtx_offset += pcmd->vtx_count;
        }
        cmd_offset = vtx_offset;
    }

    // Restore modified state
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_SCISSOR_TEST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static const char* ImImpl_GetClipboardTextFn()
{
    return glfwGetClipboardString(g_win.getGLFWwindow());
}

static void ImImpl_SetClipboardTextFn(const char* text)
{
    glfwSetClipboardString(g_win.getGLFWwindow(), text);
}


void InitImGui()
{
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = 1.0f / 60.0f;                                  // Time elapsed since last frame, in seconds (in this sample app we'll override this every frame because our timestep is variable)
    io.PixelCenterOffset = 0.5f;                                  // Align OpenGL texels
    io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;                       // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
    io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
    io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
    io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
    io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
    io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
    io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
    io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
    io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

    io.RenderDrawListsFn = ImImpl_RenderDrawLists;
    io.SetClipboardTextFn = ImImpl_SetClipboardTextFn;
    io.GetClipboardTextFn = ImImpl_GetClipboardTextFn;

    // Load font texture
    glGenTextures(1, &fontTex);
    glBindTexture(GL_TEXTURE_2D, fontTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    const void* png_data;
    unsigned int png_size;
    ImGui::GetDefaultFontData(NULL, NULL, &png_data, &png_size);
    int tex_x, tex_y, tex_comp;
    void* tex_data = stbi_load_from_memory((const unsigned char*)png_data, (int)png_size, &tex_x, &tex_y, &tex_comp, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_x, tex_y, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data);
    stbi_image_free(tex_data);
    
    try {
        g_gui_program = loadShaders(g_GUI_file_vertex_shader, g_GUI_file_fragment_shader);
    }
    catch (std::logic_error& e) {
        std::cerr << e.what() << std::endl;
    }

    texture_location = glGetUniformLocation(g_gui_program, "Texture");
    ortho_location = glGetUniformLocation(g_gui_program, "ortho");
    position_location = glGetAttribLocation(g_gui_program, "Position");
    uv_location = glGetAttribLocation(g_gui_program, "UV");
    colour_location = glGetAttribLocation(g_gui_program, "Colour");

    glGenBuffers(1, &vbo_handle);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_handle);
    glBufferData(GL_ARRAY_BUFFER, vbo_max_size, NULL, GL_DYNAMIC_DRAW);

    glGenVertexArrays(1, &vao_handle);
    glBindVertexArray(vao_handle);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_handle);
    glEnableVertexAttribArray(position_location);
    glEnableVertexAttribArray(uv_location);
    glEnableVertexAttribArray(colour_location);

    glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer(uv_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer(colour_location, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    g_time_last = ImGui::GetTime();

    g_channels_names.push_back(" 1\0\0");
    g_channels_names.push_back(" 1\0 2\0\0");
    g_channels_names.push_back(" 1\0 2\0 3\0\0");
    g_channels_names.push_back(" 1\0 2\0 3\0 4\0\0");
    g_channels_names.push_back(" 1\0 2\0 3\0 4\0 5\0\0");
    g_channels_names.push_back(" 1\0 2\0 3\0 4\0 5\0 6\0\0");
    g_channels_names.push_back(" 1\0 2\0 3\0 4\0 5\0 6\0 7\0\0");
    g_channels_names.push_back(" 1\0 2\0 3\0 4\0 5\0 6\0 7\0 8\0\0");
    g_channels_names.push_back(" 1\0 2\0 3\0 4\0 5\0 6\0 7\0 8\0 9\0\0");
    g_channels_names.push_back(" 1\0 2\0 3\0 4\0 5\0 6\0 7\0 8\0 9\0 10\0\0");
    g_channels_names.push_back(" 1\0 2\0 3\0 4\0 5\0 6\0 7\0 8\0 9\0 10\0 11\0\0");
    g_channels_names.push_back(" 1\0 2\0 3\0 4\0 5\0 6\0 7\0 8\0 9\0 10\0 11\0 12\0\0");
    g_channels_names.push_back(" 1\0 2\0 3\0 4\0 5\0 6\0 7\0 8\0 9\0 10\0 11\0 12\0 13\0\0");
    g_channels_names.push_back(" 1\0 2\0 3\0 4\0 5\0 6\0 7\0 8\0 9\0 10\0 11\0 12\0 13\0 14\0\0");
}

void UpdateImGui()
{
    ImGuiIO& io = ImGui::GetIO();

    // Setup resolution (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    glfwGetWindowSize(g_win.getGLFWwindow(), &w, &h);
    glfwGetFramebufferSize(g_win.getGLFWwindow(), &display_w, &display_h);
    io.DisplaySize = ImVec2((float)display_w, (float)display_h);                                   // Display size, in pixels. For clamping windows positions.

    // Setup time step
    static double time = 0.0f;
    const double current_time = glfwGetTime();
    io.DeltaTime = (float)(current_time - time);
    time = current_time;

    // Setup inputs
    // (we already got mouse wheel, keyboard keys & characters from glfw callbacks polled in glfwPollEvents())
    double mouse_x, mouse_y;
    glfwGetCursorPos(g_win.getGLFWwindow(), &mouse_x, &mouse_y);
    mouse_x *= (float)display_w / w;                                                               // Convert mouse coordinates to pixels
    mouse_y *= (float)display_h / h;
    io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);                                          // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
    io.MouseDown[0] = mousePressed[0] || glfwGetMouseButton(g_win.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) != 0;  // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
    io.MouseDown[1] = mousePressed[1] || glfwGetMouseButton(g_win.getGLFWwindow(), GLFW_MOUSE_BUTTON_RIGHT) != 0;

    // Start the frame
    ImGui::NewFrame();
}


void showGUI(){
    ImGui::Begin("Volume Settings", &g_show_gui, ImVec2(300, 500));
    static float f;
    g_over_gui = ImGui::IsMouseHoveringAnyWindow();

    // Calculate and show frame rate
    static ImVector<float> ms_per_frame; if (ms_per_frame.empty()) { ms_per_frame.resize(400); memset(&ms_per_frame.front(), 0, ms_per_frame.size()*sizeof(float)); }
    static int ms_per_frame_idx = 0;
    static float ms_per_frame_accum = 0.0f;
    if (!g_pause){
        ms_per_frame_accum -= ms_per_frame[ms_per_frame_idx];
        ms_per_frame[ms_per_frame_idx] = ImGui::GetIO().DeltaTime * 1000.0f;
        ms_per_frame_accum += ms_per_frame[ms_per_frame_idx];

        ms_per_frame_idx = (ms_per_frame_idx + 1) % ms_per_frame.size();
    }
    const float ms_per_frame_avg = ms_per_frame_accum / 120;

    if (ImGui::CollapsingHeader("Input", 0, true, false))
    {
        if (ImGui::TreeNode("Introduction")){
            ImGui::RadioButton("Max Intensity Projection", &g_task_chosen, 21);
            ImGui::TreePop();
        }
                
        if (g_task_chosen != g_task_chosen_old){
            g_reload_shader = true;
            g_task_chosen_old = g_task_chosen;
        }
    }

    if (ImGui::CollapsingHeader("Load Volumes", 0, true, true))
    {        
        bool load_volume_1 = false;
        
        ImGui::Text("Volumes");
        load_volume_1 ^= ImGui::Button("Load Volume Head");
        
        if (load_volume_1){
            g_file_string = "../../../data/head_w256_h256_d225_c1_b8.raw";
            read_volume(g_file_string);
            g_time_control = false;
        }       
    }

    if (ImGui::CollapsingHeader("Lighting Settings"))
    {
        ImGui::SliderFloat3("Position Light", &g_light_pos[0], -10.0f, 10.0f);

        ImGui::ColorEdit3("Ambient Color", &g_ambient_light_color[0]);
        ImGui::ColorEdit3("Diffuse Color", &g_diffuse_light_color[0]);
        ImGui::ColorEdit3("Specular Color", &g_specula_light_color[0]);

        ImGui::SliderFloat("Reflection Coefficient kd", &g_ref_coef, 0.0f, 20.0f, "%.5f", 1.0f);        
    }

    if (ImGui::CollapsingHeader("Quality Settings"))
    {
        ImGui::Text("Interpolation");
        ImGui::RadioButton("Nearest Neighbour", &g_bilinear_interpolation, 0);
        ImGui::RadioButton("Bilinear", &g_bilinear_interpolation, 1);

        ImGui::Text("Slamping Size");
        ImGui::SliderFloat("sampling factor", &g_sampling_distance_fact, 0.1f, 10.0f, "%.5f", 4.0f);
        ImGui::SliderFloat("sampling factor move", &g_sampling_distance_fact_move, 0.1f, 10.0f, "%.5f", 4.0f);
        ImGui::SliderFloat("reference sampling factor", &g_sampling_distance_fact_ref, 0.1f, 10.0f, "%.5f", 4.0f);
    }

    if (g_time_control){
        if (ImGui::CollapsingHeader("Play Settings"), true)
        {
            ImGui::Checkbox("Pause", &g_time_pause);

            ImGui::SliderFloat("Time step", &g_time_position, (float)g_min_time_step + 1.0, (float)g_max_time_step - 1.0, "%.5f", 1.0f);
            ImGui::SliderFloat("Time step speed per s", &g_time_per_sample_in_s, -10.0, 10.0, "%.5f", 1.0f);
        }
    }

    if (ImGui::CollapsingHeader("Shader", 0, true, true))
    {
        static ImVec4 text_color(1.0, 1.0, 1.0, 1.0);

        if (g_reload_shader_error) {
            text_color = ImVec4(1.0, 0.0, 0.0, 1.0);
        }
        else
        {
            text_color = ImVec4(0.0, 1.0, 0.0, 1.0);
        }

        ImGui::TextColored(text_color, "Shader OK");
        ImGui::TextWrapped(g_error_message.c_str());

        g_reload_shader ^= ImGui::Button("Reload Shader");

    }

    if (ImGui::CollapsingHeader("Timing"))
    {
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", ms_per_frame_avg, 1000.0f / ms_per_frame_avg);

        float min = *std::min_element(ms_per_frame.begin(), ms_per_frame.end());
        float max = *std::max_element(ms_per_frame.begin(), ms_per_frame.end());

        if (max - min < 10.0f){
            float mid = (max + min) * 0.5f;
            min = mid - 5.0f;
            max = mid + 5.0f;
        }

        static size_t values_offset = 0;

        char buf[50];
        sprintf(buf, "avg %f", ms_per_frame_avg);
        ImGui::PlotLines("Frame Times", &ms_per_frame.front(), (int)ms_per_frame.size(), (int)values_offset, buf, min - max * 0.1f, max * 1.1f, ImVec2(0, 70));

        ImGui::SameLine(); ImGui::Checkbox("pause", &g_pause);

    }

    if (ImGui::CollapsingHeader("Window options"))
    {
        if (ImGui::TreeNode("Background Color")){
            ImGui::ColorEdit3("BC", &g_background_color[0]);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Style Editor"))
        {
            ImGui::ShowStyleEditor();
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Logging"))
        {
            ImGui::LogButtons();
            ImGui::TreePop();
        }
    }

    ImGui::End();

    g_show_transfer_function = ImGui::Begin("Transfer Function Window", &g_show_transfer_function_in_window, ImVec2(100, 200));
        
    g_transfer_function_pos.x = ImGui::GetItemBoxMin().x;
    g_transfer_function_pos.y = ImGui::GetItemBoxMin().y;

    g_transfer_function_size.x = ImGui::GetItemBoxMax().x - ImGui::GetItemBoxMin().x;
    g_transfer_function_size.y = ImGui::GetItemBoxMax().y - ImGui::GetItemBoxMin().y;

    static unsigned byte_size = 255;

    static ImVector<float> A; if (A.empty()){ A.resize(byte_size); }

    if (g_redraw_tf){
        g_redraw_tf = false;

        image_data_type color_con = g_transfer_fun[g_current_tf].get_RGBA_transfer_function_buffer();

        for (unsigned i = 0; i != byte_size; ++i){
            A[i] = color_con[i * 4 + 3];
        }
    }

    ImGui::PlotLines("", &A.front(), (int)A.size(), (int)0, "", 0.0, 255.0, ImVec2(0, 70));

    g_transfer_function_pos.x = ImGui::GetItemBoxMin().x;
    g_transfer_function_pos.y = ImGui::GetIO().DisplaySize.y - ImGui::GetItemBoxMin().y - 75;

    g_transfer_function_size.x = ImGui::GetItemBoxMax().x - ImGui::GetItemBoxMin().x;
    g_transfer_function_size.y = ImGui::GetItemBoxMax().y - ImGui::GetItemBoxMin().y;

    ImGui::SameLine(); ImGui::Text("Color:RGB Plot: Alpha");

    unsigned combobox_entry_old = g_current_tf;
    ImGui::Combo("Channel", &g_current_tf, g_channels_names[g_channels_used.size() - 1], 3);
    ImGui::SameLine();    
    std::stringstream ss_ch;
    ss_ch << "Use Channel " << g_current_tf;
    
    if (combobox_entry_old != g_current_tf){
        g_transfer_dirty = true;
        g_redraw_tf = true;
    }

    bool checkbox_entry_old = (bool)g_channels_used[g_current_tf];
    ImGui::Checkbox(ss_ch.str().c_str(), (bool*)&g_channels_used[g_current_tf]);
    //g_channels_used[g_current_tf] = u;
    
    if (checkbox_entry_old != (bool)g_channels_used[g_current_tf]){
        g_transfer_dirty = true;
        g_redraw_tf = true;
    }
        
    static int data_value = 0;
    ImGui::SliderInt("Data Value", &data_value, 0, 255);
    static float col[4] = { 0.4f, 0.7f, 0.0f, 0.5f };
    ImGui::ColorEdit4("color", col);
    bool add_entry_to_tf = false;
    add_entry_to_tf ^= ImGui::Button("Add entry"); ImGui::SameLine();

    bool reset_tf = false;
    reset_tf ^= ImGui::Button("Reset");

    if (reset_tf){
        g_transfer_fun[g_current_tf].reset();
        g_transfer_dirty = true;
        g_redraw_tf = true;
    }

    if (add_entry_to_tf){
        g_current_tf_data_value = data_value;
        g_transfer_fun[g_current_tf].add((unsigned)data_value, glm::vec4(col[0], col[1], col[2], col[3]));
        g_transfer_dirty = true;
        g_redraw_tf = true;
    }

    if (ImGui::CollapsingHeader("Manipulate Values")){


        Transfer_function::container_type con = g_transfer_fun[g_current_tf].get_piecewise_container();

        bool delete_entry_from_tf = false;

        static std::vector<int> g_c_data_value;

        if (g_c_data_value.size() != con.size())
            g_c_data_value.resize(con.size());

        int i = 0;

        for (Transfer_function::container_type::iterator c = con.begin(); c != con.end(); ++c)
        {

            int c_data_value = c->first;
            glm::vec4 c_color_value = c->second;

            g_c_data_value[i] = c_data_value;

            std::stringstream ss;
            if (c->first < 10)
                ss << c->first << "  ";
            else if (c->first < 100)
                ss << c->first << " ";
            else
                ss << c->first;

            bool change_value = false;
            change_value ^= ImGui::SliderInt(std::to_string(i).c_str(), &g_c_data_value[i], 0, 255); ImGui::SameLine();

            if (change_value){
                if (con.find(g_c_data_value[i]) == con.end()){
                    g_transfer_fun[g_current_tf].remove(c_data_value);
                    g_transfer_fun[g_current_tf].add((unsigned)g_c_data_value[i], c_color_value);
                    g_current_tf_data_value = g_c_data_value[i];
                    g_transfer_dirty = true;
                    g_redraw_tf = true;
                }
            }

            //delete             
            bool delete_entry_from_tf = false;
            delete_entry_from_tf ^= ImGui::Button(std::string("Delete: ").append(ss.str()).c_str());

            if (delete_entry_from_tf){
                g_current_tf_data_value = c_data_value;
                g_transfer_fun[g_current_tf].remove(g_current_tf_data_value);
                g_transfer_dirty = true;
                g_redraw_tf = true;
            }

            static float n_col[4] = { 0.4f, 0.7f, 0.0f, 0.5f };
            memcpy(&n_col, &c_color_value, sizeof(float)* 4);

            bool change_color = false;
            change_color ^= ImGui::ColorEdit4(ss.str().c_str(), n_col);

            if (change_color){
                g_transfer_fun[g_current_tf].add((unsigned)g_c_data_value[i], glm::vec4(n_col[0], n_col[1], n_col[2], n_col[3]));
                g_current_tf_data_value = g_c_data_value[i];
                g_transfer_dirty = true;
                g_redraw_tf = true;
            }

            ImGui::Separator();

            ++i;
        }
    }

    if (ImGui::CollapsingHeader("Transfer Function - Save/Load", 0, true, false))
    {

        ImGui::Text("Transferfunctions");
        bool load_tf_1 = false;
        bool load_tf_2 = false;
        bool load_tf_3 = false;
        bool load_tf_4 = false;
        bool load_tf_5 = false;
        bool load_tf_6 = false;
        bool save_tf_1 = false;
        bool save_tf_2 = false;
        bool save_tf_3 = false;
        bool save_tf_4 = false;
        bool save_tf_5 = false;
        bool save_tf_6 = false;

        save_tf_1 ^= ImGui::Button("Save TF1"); ImGui::SameLine();
        load_tf_1 ^= ImGui::Button("Load TF1");
        save_tf_2 ^= ImGui::Button("Save TF2"); ImGui::SameLine();
        load_tf_2 ^= ImGui::Button("Load TF2");
        save_tf_3 ^= ImGui::Button("Save TF3"); ImGui::SameLine();
        load_tf_3 ^= ImGui::Button("Load TF3");
        save_tf_4 ^= ImGui::Button("Save TF4"); ImGui::SameLine();
        load_tf_4 ^= ImGui::Button("Load TF4");
        save_tf_5 ^= ImGui::Button("Save TF5"); ImGui::SameLine();
        load_tf_5 ^= ImGui::Button("Load TF5");
        save_tf_6 ^= ImGui::Button("Save TF6"); ImGui::SameLine();
        load_tf_6 ^= ImGui::Button("Load TF6");

        if (save_tf_1 || save_tf_2 || save_tf_3 || save_tf_4 || save_tf_5 || save_tf_6){
            Transfer_function::container_type con = g_transfer_fun[g_current_tf].get_piecewise_container();
            std::vector<Transfer_function::element_type> save_vect;

            for (Transfer_function::container_type::iterator c = con.begin(); c != con.end(); ++c)
            {
                save_vect.push_back(*c);
            }

            std::ofstream tf_file;

            if (save_tf_1){ tf_file.open("TF1", std::ios::out | std::ofstream::binary); }
            if (save_tf_2){ tf_file.open("TF2", std::ios::out | std::ofstream::binary); }
            if (save_tf_3){ tf_file.open("TF3", std::ios::out | std::ofstream::binary); }
            if (save_tf_4){ tf_file.open("TF4", std::ios::out | std::ofstream::binary); }
            if (save_tf_5){ tf_file.open("TF5", std::ios::out | std::ofstream::binary); }
            if (save_tf_6){ tf_file.open("TF6", std::ios::out | std::ofstream::binary); }

            //std::copy(save_vect.begin(), save_vect.end(), std::ostreambuf_iterator<char>(tf_file));
            tf_file.write((char*)&save_vect[0], sizeof(Transfer_function::element_type) * save_vect.size());
            tf_file.close();
        }

        if (load_tf_1 || load_tf_2 || load_tf_3 || load_tf_4 || load_tf_5 || load_tf_6){
            Transfer_function::container_type con = g_transfer_fun[g_current_tf].get_piecewise_container();
            std::vector<Transfer_function::element_type> load_vect;

            std::ifstream tf_file;

            if (load_tf_1){ tf_file.open("TF1", std::ios::in | std::ifstream::binary); }
            if (load_tf_2){ tf_file.open("TF2", std::ios::in | std::ifstream::binary); }
            if (load_tf_3){ tf_file.open("TF3", std::ios::in | std::ifstream::binary); }
            if (load_tf_4){ tf_file.open("TF4", std::ios::in | std::ifstream::binary); }
            if (load_tf_5){ tf_file.open("TF5", std::ios::in | std::ifstream::binary); }
            if (load_tf_6){ tf_file.open("TF6", std::ios::in | std::ifstream::binary); }


            if (tf_file.good()){
                tf_file.seekg(0, tf_file.end);

                size_t size = tf_file.tellg();
                unsigned elements = size / (unsigned)sizeof(Transfer_function::element_type);
                tf_file.seekg(0);

                load_vect.resize(elements);
                tf_file.read((char*)&load_vect[0], size);

                g_transfer_fun[g_current_tf].reset();
                g_transfer_dirty = true;
                for (std::vector<Transfer_function::element_type>::iterator c = load_vect.begin(); c != load_vect.end(); ++c)
                {
                    g_transfer_fun[g_current_tf].add(c->first, c->second);
                }
            }

            tf_file.close();

        }

    }

    ImGui::End();
}

int main(int argc, char* argv[])
{

    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "OpenGL Error: " << error << std::endl;
    }


    //g_win = Window(g_window_res);
    InitImGui();

    // first clear possible old values
    g_transfer_fun.clear();
    g_transfer_fun.push_back(Transfer_function());
    g_transfer_fun[0].reset();

    // the add_stop method takes:
    //  - unsigned char or float - data value     (0.0 .. 1.0) or (0..255)
    //  - vec4f         - color and alpha value   (0.0 .. 1.0) per channel
    g_transfer_fun[0].add(0.0f, glm::vec4(0.0, 0.0, 0.0, 0.0));
    g_transfer_fun[0].add(1.0f, glm::vec4(1.0, 1.0, 1.0, 1.0));
    
    // initialize the transfer function
    g_current_tf = 0;
    g_transfer_dirty = true;

    ///NOTHING TODO HERE-------------------------------------------------------------------------------

    // init and upload volume texture
    bool check = read_volume(g_file_string);
    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "OpenGL Error @ Volumes: " << error << std::endl;
    }
    // loading actual raytracing shader code (volume.vert, volume.frag)
    // edit volume.frag to define the result of our volume raycaster  
    try {
        g_volume_program = loadShaders(g_file_vertex_shader, g_file_fragment_shader,
            g_task_chosen,
            g_lighting_toggle,
            g_shadow_toggle,
            g_opacity_correction_toggle);
    }
    catch (std::logic_error& e) {
        //std::cerr << e.what() << std::endl;
        std::stringstream ss;
        ss << e.what() << std::endl;
        g_error_message = ss.str();
        g_reload_shader_error = true;
    }

    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "OpenGL Error @ Shader: " << error << std::endl;
    }


    // init object manipulator (turntable)
    Manipulator manipulator;

    // manage keys here
    // add new input if neccessary (ie changing sampling distance, isovalues, ...)
    while (!g_win.shouldClose()) {

        // exit window with escape
        if (g_win.isKeyPressed(GLFW_KEY_ESCAPE)) {
            g_win.stop();
        }

        if (g_win.isKeyPressed(GLFW_KEY_LEFT)) {
            g_light_pos.x -= 0.5f;
        }

        if (g_win.isKeyPressed(GLFW_KEY_RIGHT)) {
            g_light_pos.x += 0.5f;
        }

        if (g_win.isKeyPressed(GLFW_KEY_UP)) {
            g_light_pos.z -= 0.5f;
        }

        if (g_win.isKeyPressed(GLFW_KEY_DOWN)) {
            g_light_pos.z += 0.5f;
        }

        if (g_win.isKeyPressed(GLFW_KEY_MINUS)) {
            g_iso_value -= 0.002f;
            g_iso_value = std::max(g_iso_value, 0.0f);
        }

        if (g_win.isKeyPressed(GLFW_KEY_EQUAL) || g_win.isKeyPressed(GLFW_KEY_KP_ADD)) {
            g_iso_value += 0.002f;
            g_iso_value = std::min(g_iso_value, 1.0f);
        }

        if (g_win.isKeyPressed(GLFW_KEY_D)) {
            g_sampling_distance -= 0.0001f;
            g_sampling_distance = std::max(g_sampling_distance, 0.0001f);
        }

        if (g_win.isKeyPressed(GLFW_KEY_S)) {
            g_sampling_distance += 0.0001f;
            g_sampling_distance = std::min(g_sampling_distance, 0.2f);
        }

        float sampling_fact = g_sampling_distance_fact;
        if (g_win.isButtonPressed(Window::MOUSE_BUTTON_LEFT) || g_win.isButtonPressed(Window::MOUSE_BUTTON_MIDDLE) || g_win.isButtonPressed(Window::MOUSE_BUTTON_RIGHT)) {
            sampling_fact = g_sampling_distance_fact_move;
        }


        // to add key inputs:
        // check g_win.isKeyPressed(KEY_NAME)
        // - KEY_NAME - key name      (GLFW_KEY_A ... GLFW_KEY_Z)

        //if (g_win.isKeyPressed(GLFW_KEY_X)){
        //    
        //        ... do something
        //    
        //}

        /// reload shader if key R ist pressed
        if (g_reload_shader){

            GLuint newProgram(0);
            try {
                //std::cout << "Reload shaders" << std::endl;
                newProgram = loadShaders(g_file_vertex_shader, g_file_fragment_shader, g_task_chosen, g_lighting_toggle, g_shadow_toggle, g_opacity_correction_toggle);
                g_error_message = "";
            }
            catch (std::logic_error& e) {
                //std::cerr << e.what() << std::endl;
                std::stringstream ss;
                ss << e.what() << std::endl;
                g_error_message = ss.str();
                g_reload_shader_error = true;
                newProgram = 0;
            }
            if (0 != newProgram) {
                glDeleteProgram(g_volume_program);
                g_volume_program = newProgram;
                g_reload_shader_error = false;

            }
            else
            {
                g_reload_shader_error = true;

            }
        }

        if (g_win.isKeyPressed(GLFW_KEY_R)) {
            if (g_reload_shader_pressed != true) {
                g_reload_shader = true;
                g_reload_shader_pressed = true;
            }
            else{
                g_reload_shader = false;
            }
        }
        else {
            g_reload_shader = false;
            g_reload_shader_pressed = false;
        }

        if (g_transfer_dirty){

            g_transfer_dirty = false;

            static unsigned byte_size = 255;

            image_data_type color_con = g_transfer_fun[g_current_tf].get_RGBA_transfer_function_buffer();

            //g_transfer_texture[dif] = createTexture2D(255u, 1u, (char*)&g_transfer_fun[dif].get_RGBA_transfer_function_buffer()[0]);
            glActiveTexture(GL_TEXTURE1);
            updateTexture2D(g_transfer_texture[g_current_tf], 255u, 1u, (char*)color_con.data());

            GLuint error = glGetError();
            if (error != GL_NO_ERROR)
            {
                std::cout << "OpenGL Error updateTexture2D: " << error << std::endl;
            }
            
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_channels_used_ssbo);
            unsigned buffer_data_size = g_channels_used.size() * sizeof(unsigned);
            glBufferData(GL_SHADER_STORAGE_BUFFER, buffer_data_size, g_channels_used.data(), GL_DYNAMIC_COPY);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }

        GLuint error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "OpenGL Error 1370: " << error << std::endl;
        }

        glm::ivec2 size = g_win.windowSize();
        glViewport(0, 0, size.x, size.y);
        glClearColor(g_background_color.x, g_background_color.y, g_background_color.z, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float fovy = 45.0f;
        float aspect = (float)size.x / (float)size.y;
        float zNear = 0.025f, zFar = 10.0f;
        glm::mat4 projection = glm::perspective(fovy, aspect, zNear, zFar);

        glm::vec3 translate_rot = g_max_volume_bounds * glm::vec3(-0.5f, -0.5f, -0.5f);
        glm::vec3 translate_pos = g_max_volume_bounds * glm::vec3(+0.5f, -0.0f, -0.0f);

        glm::vec3 eye = glm::vec3(0.0f, 0.0f, 1.5f);
        glm::vec3 target = glm::vec3(0.0f);
        glm::vec3 up(0.0f, 1.0f, 0.0f);

        glm::detail::tmat4x4<float, glm::highp> view = glm::lookAt(eye, target, up);

        glm::detail::tmat4x4<float, glm::highp> turntable_matrix = manipulator.matrix();// manipulator.matrix(g_win);

        if (!g_over_gui){
            turntable_matrix = manipulator.matrix(g_win);
        }

        glm::detail::tmat4x4<float, glm::highp> model = 
             glm::translate(translate_pos)
            * turntable_matrix
            // rotate head upright
            * glm::rotate(0.5f*float(M_PI), glm::vec3(0.0f, 1.0f, 0.0f))
            * glm::rotate(0.5f*float(M_PI), glm::vec3(1.0f, 0.0f, 0.0f))
            * glm::translate(translate_rot)
            ;


        glm::detail::tmat4x4<float, glm::highp> model_view = view * model;
        glm::detail::tmat4x4<float, glm::highp> model_view_inv = glm::inverse(model_view);
                
        glm::vec4 bbmin_view = (/*model_view* */ glm::vec4(0.0, 0.0, 0.0, 1.0));
        glm::vec4 bbmax_view = (/*model_view* */ glm::vec4(g_max_volume_bounds, 1.0));

        glm::vec4 camera_translate = glm::column(glm::inverse(model_view), 3);
        glm::vec3 camera_location = glm::vec3(camera_translate.x, camera_translate.y, camera_translate.z);

        camera_location /= glm::vec3(camera_translate.w);

        glm::vec4 light_location = glm::vec4(g_light_pos, 1.0f) * model_view;

        
        float time_current = ImGui::GetTime();
        if (g_time_control && !g_time_pause){
            float time_delta = time_current - g_time_last;
            g_time_position += time_delta / (g_time_per_sample_in_s);
            if (g_time_position > g_max_time_step - 1.0){
                g_time_per_sample_in_s *= -1.0;
                //g_time_position = g_min_time_step + 1.0;
            }
            if (g_time_position < g_min_time_step + 1.0){
                g_time_per_sample_in_s *= -1.0;
                //g_time_position = g_min_time_step + 1.0;
            }
        }
        g_time_position = std::max(g_time_position, (float)g_min_time_step + 1.0f);
        g_time_position = std::min(g_time_position, (float)g_max_time_step - 1.0f);
        g_time_last = time_current;
        
        unsigned current_step = (unsigned)std::floorf(g_time_position);
        unsigned current_step_next = std::min(current_step, (unsigned)g_max_time_step - 1u) + 1u;
        if (!g_time_control){
            current_step = 0u;
            current_step_next = current_step;
            g_time_position = 0.0;
        }

        error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "OpenGL Error 1448: " << error << std::endl;
        }

        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        
        error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "OpenGL Error 1470: " << error << std::endl;
        }


        unsigned channel = 0;

        for (auto i = g_channels_used.begin(); i != g_channels_used.end(); ++i){            
            if (&i != 0){                
                unsigned idx = (current_step - g_min_time_step) + channel * (g_volume_texture_handles.size() / g_channels_used.size());
                glm::uint64 vol_tex_handle = g_volume_texture_handles[idx];
                glMakeTextureHandleResidentNV(vol_tex_handle);

                if (g_volume_texture_handles.size() > 1){
                    glm::uint64 vol_tex_handle_next = g_volume_texture_handles[idx + 1];
                    glMakeTextureHandleResidentNV(vol_tex_handle_next);

                    //std::cout << vol_tex_handle << " n " << vol_tex_handle_next << std::endl;                    
                }

                glm::uint64 tf_tex_handle = g_transfer_texture_handles[channel];
                glMakeTextureHandleResidentNV(tf_tex_handle);
            }
            ++channel;
        }
        error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "OpenGL Error 1477: " << error << std::endl;
        }
        
        glBindTexture(GL_TEXTURE_2D, g_transfer_texture[g_current_tf]);

        glUseProgram(g_volume_program);

        glUniform1ui(glGetUniformLocation(g_volume_program, "volume_handles_count"), (unsigned)g_volume_texture_handles.size());
        glUniform1ui(glGetUniformLocation(g_volume_program, "channel_count"), (unsigned)g_channels_used.size());
        glUniform1ui(glGetUniformLocation(g_volume_program, "time_step_count"), g_max_time_step);
        
        glUniform1f(glGetUniformLocation(g_volume_program, "time_position"), g_time_position);

        glUniform3fv(glGetUniformLocation(g_volume_program, "camera_location"), 1,
            glm::value_ptr(camera_location));
        glUniform1f(glGetUniformLocation(g_volume_program, "sampling_distance"), g_sampling_distance * sampling_fact);
        glUniform1f(glGetUniformLocation(g_volume_program, "sampling_distance_ref"), g_sampling_distance_fact_ref);
        glUniform1f(glGetUniformLocation(g_volume_program, "iso_value"), g_iso_value);

        glm::vec3 obj_to_tex(1.0 / g_max_volume_bounds.x, 1.0 / g_max_volume_bounds.y, 1.0 / g_max_volume_bounds.z);
        glUniform3fv(glGetUniformLocation(g_volume_program, "obj_to_tex"), 1, 
            glm::value_ptr(obj_to_tex));
        glUniform4fv(glGetUniformLocation(g_volume_program, "bbmin_view"), 1,
            glm::value_ptr(bbmin_view));
        glUniform4fv(glGetUniformLocation(g_volume_program, "bbmax_view"), 1,
            glm::value_ptr(bbmax_view));
        
        glUniform3iv(glGetUniformLocation(g_volume_program, "volume_dimensions"), 1,
            glm::value_ptr(g_vol_dimensions));

        glUniform3fv(glGetUniformLocation(g_volume_program, "light_position"), 1,
            glm::value_ptr(g_light_pos));
        glUniform3fv(glGetUniformLocation(g_volume_program, "light_ambient_color"), 1,
            glm::value_ptr(g_ambient_light_color));
        glUniform3fv(glGetUniformLocation(g_volume_program, "light_diffuse_color"), 1,
            glm::value_ptr(g_diffuse_light_color));
        glUniform3fv(glGetUniformLocation(g_volume_program, "light_specular_color"), 1,
            glm::value_ptr(g_specula_light_color));
        glUniform1f(glGetUniformLocation(g_volume_program, "light_ref_coef"), g_ref_coef);

        glUniform4fv(glGetUniformLocation(g_volume_program, "camera_os"), 1,
            glm::value_ptr(model_view_inv * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)));
        glUniformMatrix4fv(glGetUniformLocation(g_volume_program, "Projection"), 1, GL_FALSE,
            glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(g_volume_program, "Model"), 1, GL_FALSE,
            glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(g_volume_program, "Modelview"), 1, GL_FALSE,
            glm::value_ptr(model_view));
        glUniformMatrix4fv(glGetUniformLocation(g_volume_program, "Modelview_inv"), 1, GL_FALSE,
            glm::value_ptr(model_view_inv));

        GLuint v_block_index = 0;
        v_block_index = glGetProgramResourceIndex(g_volume_program, GL_SHADER_STORAGE_BLOCK, "volume_data");
        GLuint vol_ssbo_binding_point_index = 2;
        glShaderStorageBlockBinding(g_volume_program, v_block_index, vol_ssbo_binding_point_index);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, vol_ssbo_binding_point_index, g_handles_ssbo);

        GLuint tf_block_index = 0;
        tf_block_index = glGetProgramResourceIndex(g_volume_program, GL_SHADER_STORAGE_BLOCK, "transfer_data");
        GLuint tf_ssbo_binding_point_index = 3;
        glShaderStorageBlockBinding(g_volume_program, tf_block_index, tf_ssbo_binding_point_index);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, tf_ssbo_binding_point_index, g_tf_handles_ssbo);

        GLuint utf_block_index = 0;
        utf_block_index = glGetProgramResourceIndex(g_volume_program, GL_SHADER_STORAGE_BLOCK, "transfer_used_data");
        GLuint utf_ssbo_binding_point_index = 4;
        glShaderStorageBlockBinding(g_volume_program, utf_block_index, utf_ssbo_binding_point_index);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, utf_ssbo_binding_point_index, g_channels_used_ssbo);

        GLuint crf_block_index = 0;
        crf_block_index = glGetProgramResourceIndex(g_volume_program, GL_SHADER_STORAGE_BLOCK, "channel_range_data");
        GLuint crf_ssbo_binding_point_index = 5;
        glShaderStorageBlockBinding(g_volume_program, crf_block_index, crf_ssbo_binding_point_index);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, crf_ssbo_binding_point_index, g_channels_range_ssbo);
        
        GLuint cwr_block_index = 0;
        cwr_block_index = glGetProgramResourceIndex(g_volume_program, GL_SHADER_STORAGE_BLOCK, "color_write_buffer");
        GLuint cwr_ssbo_binding_point_index = 6;
        glShaderStorageBlockBinding(g_volume_program, cwr_block_index, cwr_ssbo_binding_point_index);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, cwr_ssbo_binding_point_index, g_color_write_ssbo);

        if (!g_pause)
            g_cube.draw();

        glUseProgram(0);

        error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "OpenGL Error 1544: " << error << std::endl;
        }


        channel = 0;
        for (auto i = g_channels_used.begin(); i != g_channels_used.end(); ++i){            
            if (&i != 0){
                //unsigned idx = current_step + channel * (g_volume_texture_handles.size() / g_channels_used.size());
                unsigned idx = (current_step - g_min_time_step) + channel * (g_volume_texture_handles.size() / g_channels_used.size());
                glm::uint64 vol_tex_handle = g_volume_texture_handles[idx];                                      
                glMakeTextureHandleNonResidentNV(vol_tex_handle);                

                if (g_volume_texture_handles.size() > 1){
                    glm::uint64 vol_tex_handle_next = g_volume_texture_handles[idx + 1];
                    glMakeTextureHandleNonResidentNV(vol_tex_handle_next);
                }
                
                glm::uint64 tf_tex_handle = g_transfer_texture_handles[channel];
                glMakeTextureHandleNonResidentNV(tf_tex_handle);

            }
            ++channel;
        }

        //IMGUI ROUTINE begin    
        ImGuiIO& io = ImGui::GetIO();
        io.MouseWheel = 0;
        mousePressed[0] = mousePressed[1] = false;

        error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "OpenGL Error 1566: " << error << std::endl;
        }


        glfwPollEvents();
        UpdateImGui();
        showGUI();

        // Rendering
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        ImGui::Render();
        //IMGUI ROUTINE end        
        if (g_show_transfer_function)
            g_transfer_fun[g_current_tf].draw_texture(g_transfer_function_pos, g_transfer_function_size, g_transfer_texture[g_current_tf]);


        glBindTexture(GL_TEXTURE_2D, 0);
        g_win.update();

    }

    //IMGUI shutdown
    if (vao_handle) glDeleteVertexArrays(1, &vao_handle);
    if (vbo_handle) glDeleteBuffers(1, &vbo_handle);
    glDetachShader(g_gui_program, vert_handle);
    glDetachShader(g_gui_program, frag_handle);
    glDeleteShader(vert_handle);
    glDeleteShader(frag_handle);
    glDeleteProgram(g_gui_program);
    //IMGUI shutdown end

    ImGui::Shutdown();

    return 0;
}