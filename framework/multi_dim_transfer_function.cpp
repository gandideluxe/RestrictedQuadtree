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
#include <transfer_function.hpp>
#include <utils.hpp>
#include <turntable.hpp>
#include <imgui.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>        // stb_image.h for PNG loading
#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))

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

    //std::cout << f << std::endl;

    return createProgram(v, f);
}

Turntable  g_turntable;

///SETUP VOLUME RAYCASTER HERE
// set the volume file
std::string g_file_string = "../../../data/head_w256_h256_d225_c1_b8.raw";

// set the sampling distance for the ray traversal
float       g_sampling_distance = 0.001f;
float       g_sampling_distance_ref = 0.001f;

float       g_iso_value = 0.2f;

// set the light position and color for shading
glm::vec3   g_light_pos = glm::vec3(1.0, 1.0, 1.0);
glm::vec3   g_light_color = glm::vec3(1.0f, 1.0f, 1.0f);

// set backgorund color here
//glm::vec3   g_background_color = glm::vec3(1.0f, 1.0f, 1.0f); //white
glm::vec3   g_background_color = glm::vec3(0.08f, 0.08f, 0.08f);   //grey

glm::ivec2  g_window_res = glm::ivec2(1600, 800);
Window g_win(g_window_res);

// Volume Rendering GLSL Program
GLuint g_volume_program(0);
std::string g_error_message;
bool g_reload_shader_error = false;

Transfer_function g_transfer_fun;
int g_current_tf_data_value = 0;
GLuint g_transfer_texture;
bool g_transfer_dirty = true;
bool g_redraw_tf = true;
bool g_lighting_toggle = false;
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
bool g_show_transfer_function_pressed = false;

int g_task_chosen = 21;
int g_task_chosen_old = g_task_chosen;

bool  g_pause = false;

Volume_loader_raw g_volume_loader;
volume_data_type g_volume_data;
glm::ivec3 g_vol_dimensions;
glm::vec3 g_max_volume_bounds;
unsigned g_channel_size = 0;
unsigned g_channel_count = 0;
GLuint g_volume_texture = 0;

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

    // calculating max volume bounds of volume (0.0 .. 1.0)
    g_max_volume_bounds = glm::vec3(g_vol_dimensions) / glm::vec3((float)max_dim);

    // loading volume file data
    g_volume_data = g_volume_loader.load_volume(g_file_string);
    g_channel_size = g_volume_loader.get_bit_per_channel(g_file_string) / 8;
    g_channel_count = g_volume_loader.get_channel_count(g_file_string);

    glActiveTexture(GL_TEXTURE0);
    g_volume_texture = createTexture3D(g_vol_dimensions.x, g_vol_dimensions.y, g_vol_dimensions.z, g_channel_size, g_channel_count, (char*)&g_volume_data[0]);

    return g_volume_texture;

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

    if (ImGui::CollapsingHeader("Task", 0, true, true))
    {
        ImGui::Text("Introduction");
        ImGui::RadioButton("Max Intensity Projection", &g_task_chosen, 21);
        ImGui::RadioButton("Average Intensity Projection", &g_task_chosen, 22);
        ImGui::Text("Iso Surface Rendering");
        ImGui::SliderFloat("Iso Value", &g_iso_value, 0.0f, 1.0f, "%.8f", 1.0f);
        ImGui::RadioButton("Inaccurate", &g_task_chosen, 31);
        ImGui::RadioButton("Binary Search", &g_task_chosen, 32);
        ImGui::Text("Direct Volume Rendering");
        ImGui::RadioButton("Compositing", &g_task_chosen, 41);
        g_reload_shader ^= ImGui::Checkbox("Enable Lightning", &g_lighting_toggle);
        g_reload_shader ^= ImGui::Checkbox("Opacity Correction", &g_opacity_correction_toggle);

        if (g_task_chosen != g_task_chosen_old){
            g_reload_shader = true;
            g_task_chosen_old = g_task_chosen;
        }
    }

    if (ImGui::CollapsingHeader("Load Volumes", 0, true, false))
    {


        bool load_volume_1 = false;
        bool load_volume_2 = false;
        bool load_volume_3 = false;

        ImGui::Text("Volumes");
        load_volume_1 ^= ImGui::Button("Load Volume Head");
        load_volume_2 ^= ImGui::Button("Load Volume Engine");
        load_volume_3 ^= ImGui::Button("Load Volume Bucky");


        if (load_volume_1){
            g_file_string = "../../../data/head_w256_h256_d225_c1_b8.raw";
            read_volume(g_file_string);
        }
        if (load_volume_2){
            g_file_string = "../../../data/Engine_w256_h256_d256_c1_b8.raw";
            read_volume(g_file_string);
        }

        if (load_volume_3){
            g_file_string = "../../../data/Bucky_uncertainty_data_w32_h32_d32_c1_b8.raw";
            read_volume(g_file_string);
        }
    }


    if (ImGui::CollapsingHeader("Quality Settings"))
    {
        ImGui::Text("Interpolation");
        ImGui::RadioButton("Nearest Neighbour", &g_bilinear_interpolation, 0);
        ImGui::RadioButton("Bilinear", &g_bilinear_interpolation, 1);

        ImGui::Text("Slamping Size");
        ImGui::SliderFloat("sampling step", &g_sampling_distance, 0.0005f, 0.1f, "%.5f", 4.0f);
        ImGui::SliderFloat("reference sampling step", &g_sampling_distance_ref, 0.0005f, 0.1f, "%.5f", 4.0f);
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

    ImGui::Begin("Transfer Function Window", &g_show_transfer_function_in_window, ImVec2(100, 200));
    if (ImGui::CollapsingHeader("Add Values"), true)
    {

        g_show_transfer_function = true;

        g_transfer_function_pos.x = ImGui::GetItemBoxMin().x;
        g_transfer_function_pos.y = ImGui::GetItemBoxMin().y;

        g_transfer_function_size.x = ImGui::GetItemBoxMax().x - ImGui::GetItemBoxMin().x;
        g_transfer_function_size.y = ImGui::GetItemBoxMax().y - ImGui::GetItemBoxMin().y;

        static unsigned byte_size = 255;

        static ImVector<float> A; if (A.empty()){ A.resize(byte_size); }

        if (g_redraw_tf){
            g_redraw_tf = false;

            image_data_type color_con = g_transfer_fun.get_RGBA_transfer_function_buffer();

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

        static int data_value = 0;
        ImGui::SliderInt("Data Value", &data_value, 0, 255);
        static float col[4] = { 0.4f, 0.7f, 0.0f, 0.5f };
        ImGui::ColorEdit4("color", col);
        bool add_entry_to_tf = false;
        add_entry_to_tf ^= ImGui::Button("Add entry"); ImGui::SameLine();

        bool reset_tf = false;
        reset_tf ^= ImGui::Button("Reset");

        if (reset_tf){
            g_transfer_fun.reset();
            g_transfer_dirty = true;
            g_redraw_tf = true;
        }

        if (add_entry_to_tf){
            g_current_tf_data_value = data_value;
            g_transfer_fun.add((unsigned)data_value, glm::vec4(col[0], col[1], col[2], col[3]));
            g_transfer_dirty = true;
            g_redraw_tf = true;
        }

        if (ImGui::CollapsingHeader("Manipulate Values")){


            Transfer_function::container_type con = g_transfer_fun.get_piecewise_container();

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
                        g_transfer_fun.remove(c_data_value);
                        g_transfer_fun.add((unsigned)g_c_data_value[i], c_color_value);
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
                    g_transfer_fun.remove(g_current_tf_data_value);
                    g_transfer_dirty = true;
                    g_redraw_tf = true;
                }

                static float n_col[4] = { 0.4f, 0.7f, 0.0f, 0.5f };
                memcpy(&n_col, &c_color_value, sizeof(float)* 4);

                bool change_color = false;
                change_color ^= ImGui::ColorEdit4(ss.str().c_str(), n_col);

                if (change_color){
                    g_transfer_fun.add((unsigned)g_c_data_value[i], glm::vec4(n_col[0], n_col[1], n_col[2], n_col[3]));
                    g_current_tf_data_value = g_c_data_value[i];
                    g_transfer_dirty = true;
                    g_redraw_tf = true;
                }

                ImGui::Separator();

                ++i;
            }
        }

    }
    else{ g_show_transfer_function = false; }


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
            Transfer_function::container_type con = g_transfer_fun.get_piecewise_container();
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
            Transfer_function::container_type con = g_transfer_fun.get_piecewise_container();
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

                g_transfer_fun.reset();
                g_transfer_dirty = true;
                for (std::vector<Transfer_function::element_type>::iterator c = load_vect.begin(); c != load_vect.end(); ++c)
                {
                    g_transfer_fun.add(c->first, c->second);
                }
            }

            tf_file.close();

        }

    }

    ImGui::End();
}

int main(int argc, char* argv[])
{
    //g_win = Window(g_window_res);
    InitImGui();

    // initialize the transfer function

    // first clear possible old values
    g_transfer_fun.reset();

    // the add_stop method takes:
    //  - unsigned char or float - data value     (0.0 .. 1.0) or (0..255)
    //  - vec4f         - color and alpha value   (0.0 .. 1.0) per channel
    g_transfer_fun.add(0.0f, glm::vec4(0.0, 0.0, 0.0, 0.0));
    g_transfer_fun.add(1.0f, glm::vec4(1.0, 1.0, 1.0, 1.0));


    ///NOTHING TODO HERE-------------------------------------------------------------------------------

    // init and upload volume texture
    bool check = read_volume(g_file_string);
    
    GLuint linear_sampler_state;
    glGenSamplers(1, &linear_sampler_state);

    if (!linear_sampler_state)
        std::cerr << "Error: Initial Sampler State" << std::endl;

    long long int handle = glGetTextureSamplerHandleARB(g_volume_texture, linear_sampler_state);
    
    if (!handle)
        std::cerr << "Error: Initial Texture Sample" << std::endl;
    
    //glGetTextureSamplerHandleARB(handle);

    // init and upload transfer function texture
    glActiveTexture(GL_TEXTURE1);
    g_transfer_texture = createTexture2D(255u, 1u, (char*)&g_transfer_fun.get_RGBA_transfer_function_buffer()[0]);
        
    // setting up proxy geometry
    Cube cube(glm::vec3(0.0, 0.0, 0.0), g_max_volume_bounds);

    // loading actual raytracing shader code (volume.vert, volume.frag)
    // edit volume.frag to define the result of our volume raycaster  
    try {
        g_volume_program = loadShaders(g_file_vertex_shader, g_file_fragment_shader,
            g_task_chosen,
            g_lighting_toggle,
            g_opacity_correction_toggle);
    }
    catch (std::logic_error& e) {
        //std::cerr << e.what() << std::endl;
        std::stringstream ss;
        ss << e.what() << std::endl;
        g_error_message = ss.str();
        g_reload_shader_error = true;
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
                newProgram = loadShaders(g_file_vertex_shader, g_file_fragment_shader, g_task_chosen, g_lighting_toggle, g_opacity_correction_toggle);
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



        /// show transfer function if T is pressed
        if (g_win.isKeyPressed(GLFW_KEY_T)){
            if (!g_show_transfer_function_pressed){
                g_show_transfer_function = !g_show_transfer_function;
            }
            g_show_transfer_function_pressed = true;
        }
        else {
            g_show_transfer_function_pressed = false;
        }

        if (g_transfer_dirty){
            g_transfer_dirty = false;

            static unsigned byte_size = 255;

            image_data_type color_con = g_transfer_fun.get_RGBA_transfer_function_buffer();

            glActiveTexture(GL_TEXTURE1);
            glDeleteTextures(1, &g_transfer_texture);
            g_transfer_texture = createTexture2D(255u, 1u, (char*)&g_transfer_fun.get_RGBA_transfer_function_buffer()[0]);

        }

        glBindTexture(GL_TEXTURE_3D, g_volume_texture);

        if (g_bilinear_interpolation){
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else{
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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

        glm::detail::tmat4x4<float, glm::highp> model_view = view
            //* glm::inverse(glm::translate(translate_pos))
            //* glm::translate(translate_rot)
            * glm::translate(translate_pos)
            * turntable_matrix
            // rotate head upright
            * glm::rotate(0.5f*float(M_PI), glm::vec3(0.0f, 1.0f, 0.0f))
            * glm::rotate(0.5f*float(M_PI), glm::vec3(1.0f, 0.0f, 0.0f))
            * glm::translate(translate_rot)
            ;

        glm::vec4 camera_translate = glm::column(glm::inverse(model_view), 3);
        glm::vec3 camera_location = glm::vec3(camera_translate.x, camera_translate.y, camera_translate.z);

        camera_location /= glm::vec3(camera_translate.w);

        glm::vec4 light_location = glm::vec4(g_light_pos, 1.0f) * model_view;

        glUseProgram(g_volume_program);

        glUniform1i(glGetUniformLocation(g_volume_program, "volume_texture"), 0);
        glUniform1i(glGetUniformLocation(g_volume_program, "transfer_texture"), 1);

        glUniform3fv(glGetUniformLocation(g_volume_program, "camera_location"), 1,
            glm::value_ptr(camera_location));
        glUniform1f(glGetUniformLocation(g_volume_program, "sampling_distance"), g_sampling_distance);
        glUniform1f(glGetUniformLocation(g_volume_program, "sampling_distance_ref"), g_sampling_distance_ref);
        glUniform1f(glGetUniformLocation(g_volume_program, "iso_value"), g_iso_value);
        glUniform3fv(glGetUniformLocation(g_volume_program, "max_bounds"), 1,
            glm::value_ptr(g_max_volume_bounds));
        glUniform3iv(glGetUniformLocation(g_volume_program, "volume_dimensions"), 1,
            glm::value_ptr(g_vol_dimensions));

        glUniform3fv(glGetUniformLocation(g_volume_program, "light_position"), 1,
            //glm::value_ptr(glm::vec3(light_location.x, light_location.y, light_location.z)));
            glm::value_ptr(g_light_pos));
        glUniform3fv(glGetUniformLocation(g_volume_program, "light_color"), 1,
            glm::value_ptr(g_light_color));

        glUniform3fv(glGetUniformLocation(g_volume_program, "light_color"), 1,
            glm::value_ptr(g_light_color));

        glUniformMatrix4fv(glGetUniformLocation(g_volume_program, "Projection"), 1, GL_FALSE,
            glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(g_volume_program, "Modelview"), 1, GL_FALSE,
            glm::value_ptr(model_view));
        if (!g_pause)
            cube.draw();
        glUseProgram(0);

        //IMGUI ROUTINE begin    
        ImGuiIO& io = ImGui::GetIO();
        io.MouseWheel = 0;
        mousePressed[0] = mousePressed[1] = false;
        glfwPollEvents();
        UpdateImGui();
        showGUI();

        // Rendering
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        ImGui::Render();
        //IMGUI ROUTINE end
        if (g_show_transfer_function)
            g_transfer_fun.draw_texture(g_transfer_function_pos, g_transfer_function_size, g_transfer_texture);

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
